/*
 * Fujisan Test Suite - FujiNet Process Manager Tests
 *
 * Tests process lifecycle (start, stop, forceKill),
 * state transitions, and config file validation.
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "fujinetprocessmanager.h"

#ifdef Q_OS_WIN
/*
 * Use PowerShell from SystemRoot (same as CI / interactives). ComSpec/SystemRoot
 * may be unset or misleading under some MSYS2-spawned tools; timeout.exe can also
 * exit immediately when stdin is not a console. PowerShell Start-Sleep / exit /
 * Write-Output behave predictably under QProcess.
 */
static QString windowsSystemRoot()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString r = env.value(QStringLiteral("SystemRoot"));
    if (!r.isEmpty())
        return QDir::cleanPath(QDir::fromNativeSeparators(r));
    const QString comspec = env.value(QStringLiteral("ComSpec"));
    if (!comspec.isEmpty()) {
        const QFileInfo fi(QDir::fromNativeSeparators(comspec));
        if (fi.exists()) {
            QDir system32 = fi.absoluteDir();
            if (system32.cdUp())
                return system32.absolutePath();
        }
    }
    return QStringLiteral("C:/Windows");
}

static QString windowsPowerShellExe()
{
    const QString p = QDir(windowsSystemRoot()).filePath(
        QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe"));
    return QDir::cleanPath(p);
}
#endif

class TestFujiNetProcess : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // FujiNetProcessManager::start() checks QFile::exists() and ExeUser permission.
    // Unix: helper shell scripts in temp dir. Windows: PowerShell only.
    QString m_sleepScript;
    QString m_exitScript;
    QString m_echoScript;

    bool startSleep(FujiNetProcessManager &mgr, int seconds)
    {
#ifdef Q_OS_WIN
        const QString ps = windowsPowerShellExe();
        return mgr.start(ps,
                         QStringList{QStringLiteral("-NoProfile"),
                                     QStringLiteral("-NonInteractive"),
                                     QStringLiteral("-ExecutionPolicy"),
                                     QStringLiteral("Bypass"),
                                     QStringLiteral("-Command"),
                                     QStringLiteral("Start-Sleep -Seconds %1").arg(seconds)});
#else
        return mgr.start(m_sleepScript, {QString::number(seconds)});
#endif
    }

    bool startExit(FujiNetProcessManager &mgr, int code)
    {
#ifdef Q_OS_WIN
        const QString ps = windowsPowerShellExe();
        return mgr.start(ps,
                         QStringList{QStringLiteral("-NoProfile"),
                                     QStringLiteral("-NonInteractive"),
                                     QStringLiteral("-ExecutionPolicy"),
                                     QStringLiteral("Bypass"),
                                     QStringLiteral("-Command"),
                                     QStringLiteral("exit %1").arg(code)});
#else
        return mgr.start(m_exitScript, {QString::number(code)});
#endif
    }

    bool startEcho(FujiNetProcessManager &mgr, const QString &msg)
    {
#ifdef Q_OS_WIN
        QString quoted = msg;
        quoted.replace(QLatin1Char('\''), QLatin1String("''"));
        const QString ps = windowsPowerShellExe();
        return mgr.start(
            ps, QStringList{QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                             QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                             QStringLiteral("-Command"),
                             QStringLiteral("Write-Output '%1'").arg(quoted)});
#else
        return mgr.start(m_echoScript, {msg});
#endif
    }

    void createHelperScripts()
    {
#ifdef Q_OS_WIN
        (void)0;
#else
        m_sleepScript = m_tempDir.path() + "/helper_sleep.sh";
        QFile sf(m_sleepScript);
        sf.open(QIODevice::WriteOnly);
        sf.write("#!/bin/sh\nsleep \"$1\"\n");
        sf.close();
        sf.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

        m_exitScript = m_tempDir.path() + "/helper_exit.sh";
        QFile ef(m_exitScript);
        ef.open(QIODevice::WriteOnly);
        ef.write("#!/bin/sh\nexit \"$1\"\n");
        ef.close();
        ef.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

        m_echoScript = m_tempDir.path() + "/helper_echo.sh";
        QFile ecf(m_echoScript);
        ecf.open(QIODevice::WriteOnly);
        ecf.write("#!/bin/sh\necho \"$@\"\n");
        ecf.close();
        ecf.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        createHelperScripts();
#ifdef Q_OS_WIN
        QVERIFY2(QFileInfo::exists(windowsPowerShellExe()),
                 qPrintable(QStringLiteral("missing ") + windowsPowerShellExe()));
#endif
    }

    // ---------------------------------------------------------------
    // Initial state
    // ---------------------------------------------------------------
    void testInitialState()
    {
        FujiNetProcessManager mgr;
        QCOMPARE(mgr.getState(), FujiNetProcessManager::NotRunning);
        QVERIFY(!mgr.isRunning());
        QVERIFY(mgr.getLastError().isEmpty());
    }

    // ---------------------------------------------------------------
    // Starting a valid process
    // ---------------------------------------------------------------
    void testStartProcess()
    {
        FujiNetProcessManager mgr;
        QSignalSpy startedSpy(&mgr, &FujiNetProcessManager::processStarted);

        bool result = startSleep(mgr, 30);
        QVERIFY(result);

        if (startedSpy.isEmpty()) {
            QVERIFY(startedSpy.wait(5000));
        }

        QCOMPARE(mgr.getState(), FujiNetProcessManager::Running);
        QVERIFY(mgr.isRunning());

        mgr.forceKill();
    }

    // ---------------------------------------------------------------
    // Starting when already running fails
    // ---------------------------------------------------------------
    void testStartWhileRunning()
    {
        FujiNetProcessManager mgr;
        QSignalSpy startedSpy(&mgr, &FujiNetProcessManager::processStarted);

        startSleep(mgr, 30);
        if (startedSpy.isEmpty()) {
            QVERIFY(startedSpy.wait(5000));
        }

        bool secondStart = startSleep(mgr, 30);
        QVERIFY(!secondStart);

        mgr.forceKill();
    }

    // ---------------------------------------------------------------
    // forceKill transitions to NotRunning
    // ---------------------------------------------------------------
    void testForceKill()
    {
        FujiNetProcessManager mgr;
        QSignalSpy startedSpy(&mgr, &FujiNetProcessManager::processStarted);
        QSignalSpy stoppedSpy(&mgr, &FujiNetProcessManager::processStopped);

        startSleep(mgr, 60);
        if (startedSpy.isEmpty()) {
            QVERIFY(startedSpy.wait(5000));
        }
        QVERIFY(mgr.isRunning());

        mgr.forceKill();

        if (stoppedSpy.isEmpty()) {
            QVERIFY(stoppedSpy.wait(5000));
        }
        QCOMPARE(mgr.getState(), FujiNetProcessManager::NotRunning);
    }

    // ---------------------------------------------------------------
    // stop() gracefully terminates
    // ---------------------------------------------------------------
    void testStop()
    {
        FujiNetProcessManager mgr;
        QSignalSpy startedSpy(&mgr, &FujiNetProcessManager::processStarted);
        QSignalSpy stoppedSpy(&mgr, &FujiNetProcessManager::processStopped);

        startSleep(mgr, 60);
        if (startedSpy.isEmpty()) {
            QVERIFY(startedSpy.wait(5000));
        }

        mgr.stop();

        if (stoppedSpy.isEmpty()) {
            QVERIFY(stoppedSpy.wait(10000));
        }
        QVERIFY(!mgr.isRunning());
    }

    // ---------------------------------------------------------------
    // Starting with nonexistent binary triggers error
    // ---------------------------------------------------------------
    void testStartNonexistentBinary()
    {
        FujiNetProcessManager mgr;
        QSignalSpy errorSpy(&mgr, &FujiNetProcessManager::processError);

#ifdef Q_OS_WIN
        const QString bad = QDir::cleanPath(
            QDir(QDir::tempPath())
                .filePath(QStringLiteral("___fujisan_nonexistent___/nope.exe")));
#else
        const QString bad = QStringLiteral("/nonexistent/binary/path");
#endif
        bool result = mgr.start(bad);
        // start() returns false synchronously because QFile::exists() fails
        QVERIFY(!result);
        QCOMPARE(mgr.getState(), FujiNetProcessManager::Error);
        QCOMPARE(errorSpy.count(), 1);
    }

    // ---------------------------------------------------------------
    // Configuration: launch behavior
    // ---------------------------------------------------------------
    void testLaunchBehaviorConfig()
    {
        FujiNetProcessManager mgr;
        QCOMPARE(mgr.getLaunchBehavior(),
                 FujiNetProcessManager::AutoLaunch);

        mgr.setLaunchBehavior(FujiNetProcessManager::Manual);
        QCOMPARE(mgr.getLaunchBehavior(),
                 FujiNetProcessManager::Manual);

        mgr.setLaunchBehavior(FujiNetProcessManager::DetectExisting);
        QCOMPARE(mgr.getLaunchBehavior(),
                 FujiNetProcessManager::DetectExisting);
    }

    // ---------------------------------------------------------------
    // Binary path and arguments storage
    // ---------------------------------------------------------------
    void testBinaryPathAndArgs()
    {
        FujiNetProcessManager mgr;
        mgr.setBinaryPath("/usr/bin/fujinet");
        QCOMPARE(mgr.getBinaryPath(), QString("/usr/bin/fujinet"));

        QStringList args = {"-u", "http://0.0.0.0:8000", "-c", "fnconfig.ini"};
        mgr.setArguments(args);
        QCOMPARE(mgr.getArguments(), args);
    }

    // ---------------------------------------------------------------
    // Output buffer management
    // ---------------------------------------------------------------
    void testClearOutputBuffers()
    {
        FujiNetProcessManager mgr;
        mgr.clearOutputBuffers();
        QCOMPARE(mgr.getStdout(), QString(""));
        QCOMPARE(mgr.getStderr(), QString(""));
    }

    // ---------------------------------------------------------------
    // Process exits with specific code
    // ---------------------------------------------------------------
    void testExitCode()
    {
        FujiNetProcessManager mgr;
        QSignalSpy stoppedSpy(&mgr, &FujiNetProcessManager::processStopped);

        startExit(mgr, 42);

        if (stoppedSpy.isEmpty()) {
            QVERIFY(stoppedSpy.wait(5000));
        }

        QCOMPARE(mgr.getLastExitCode(), 42);
    }

    // ---------------------------------------------------------------
    // Stdout/stderr capture
    // ---------------------------------------------------------------
    void testOutputCapture()
    {
        FujiNetProcessManager mgr;
        QSignalSpy stdoutSpy(&mgr, &FujiNetProcessManager::stdoutReceived);
        QSignalSpy stoppedSpy(&mgr, &FujiNetProcessManager::processStopped);

        startEcho(mgr, QStringLiteral("hello_fujisan"));

        if (stoppedSpy.isEmpty()) {
            QVERIFY(stoppedSpy.wait(5000));
        }

        // stdout should contain our message
        QVERIFY(mgr.getStdout().contains("hello_fujisan"));
    }
};

QTEST_MAIN(TestFujiNetProcess)
#include "test_fujinet_process.moc"
