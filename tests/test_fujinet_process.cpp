/*
 * Fujisan Test Suite - FujiNet Process Manager Tests
 *
 * Tests process lifecycle (start, stop, forceKill),
 * state transitions, and config file validation.
 */

#include <QCoreApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "fujinetprocessmanager.h"

#ifdef Q_OS_WIN
/* Use native Windows binaries only. CI runs from MSYS2 but QProcess + bash/sleep
 * was fragile (wrong MSYS root, PATH, DLL); 60s = many 5s spy timeouts. */
static QString windowsSystemRoot()
{
    QString r =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("SystemRoot"));
    if (r.isEmpty())
        r = QStringLiteral("C:/Windows");
    return QDir::fromNativeSeparators(r);
}

static QString windowsCmdExe()
{
    return QDir(windowsSystemRoot()).filePath(QStringLiteral("System32/cmd.exe"));
}

static QString windowsTimeoutExe()
{
    return QDir(windowsSystemRoot()).filePath(QStringLiteral("System32/timeout.exe"));
}
#endif

class TestFujiNetProcess : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // FujiNetProcessManager::start() checks QFile::exists() and ExeUser permission.
    // Unix: helper shell scripts in temp dir. Windows: timeout.exe + cmd.exe only.
    QString m_sleepScript;
    QString m_exitScript;
    QString m_echoScript;

    bool startSleep(FujiNetProcessManager &mgr, int seconds)
    {
#ifdef Q_OS_WIN
        const QString exe = windowsTimeoutExe();
        return mgr.start(exe, QStringList{QStringLiteral("/t"),
                                          QString::number(seconds),
                                          QStringLiteral("/nobreak")});
#else
        return mgr.start(m_sleepScript, {QString::number(seconds)});
#endif
    }

    bool startExit(FujiNetProcessManager &mgr, int code)
    {
#ifdef Q_OS_WIN
        const QString exe = windowsCmdExe();
        return mgr.start(exe, QStringList{QStringLiteral("/c"),
                                          QStringLiteral("exit %1").arg(code)});
#else
        return mgr.start(m_exitScript, {QString::number(code)});
#endif
    }

    bool startEcho(FujiNetProcessManager &mgr, const QString &msg)
    {
#ifdef Q_OS_WIN
        const QString exe = windowsCmdExe();
        return mgr.start(exe, QStringList{QStringLiteral("/c"),
                                            QStringLiteral("echo %1").arg(msg)});
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
        QVERIFY2(QFile::exists(windowsTimeoutExe()),
                 qPrintable(QStringLiteral("missing ") + windowsTimeoutExe()));
        QVERIFY2(QFile::exists(windowsCmdExe()),
                 qPrintable(QStringLiteral("missing ") + windowsCmdExe()));
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

        bool result = mgr.start("/nonexistent/binary/path");
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
