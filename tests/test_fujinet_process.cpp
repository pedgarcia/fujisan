/*
 * Fujisan Test Suite - FujiNet Process Manager Tests
 *
 * Tests process lifecycle (start, stop, forceKill),
 * state transitions, and config file validation.
 */

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest/QtTest>

#include "fujinetprocessmanager.h"

#ifdef Q_OS_WIN
/* bash.exe must come from the *same* MSYS2 tree as mingw64/bin (where cmake runs).
 * A stale or partial C:/msys64 is often present on runners; checking it first breaks
 * QProcess::start (wrong DLLs / no startup) while every test burns a 5s spy timeout. */
static QString resolveWindowsMsysBash()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *key : {"MSYS2", "MSYS2_PATH"}) {
        const QString root = env.value(QString::fromLatin1(key));
        if (!root.isEmpty()) {
            const QString bash = QDir(root).filePath(QStringLiteral("usr/bin/bash.exe"));
            if (QFile::exists(bash))
                return QDir::cleanPath(bash);
        }
    }

    const QString pathVar = env.value(QStringLiteral("PATH"));
    for (QString raw : pathVar.split(QLatin1Char(';'), QString::SkipEmptyParts)) {
        raw = QDir::fromNativeSeparators(raw.trimmed());
        if (raw.isEmpty())
            continue;

        const bool isToolBin =
            raw.endsWith(QStringLiteral("/bin"), Qt::CaseInsensitive)
            && (raw.contains(QStringLiteral("mingw64"), Qt::CaseInsensitive)
                || raw.contains(QStringLiteral("ucrt64"), Qt::CaseInsensitive)
                || raw.contains(QStringLiteral("clang64"), Qt::CaseInsensitive));

        if (isToolBin) {
            QDir binDir(raw);
            if (binDir.cdUp() && binDir.cdUp()) {
                const QString bash =
                    binDir.filePath(QStringLiteral("usr/bin/bash.exe"));
                if (QFile::exists(bash))
                    return QDir::cleanPath(bash);
            }
        }

        const QFileInfo bashInDir(raw, QStringLiteral("bash.exe"));
        if (bashInDir.exists()
            && raw.endsWith(QStringLiteral("/usr/bin"), Qt::CaseInsensitive)) {
            return QDir::cleanPath(bashInDir.absoluteFilePath());
        }
    }

    static const char *const kFallback[] = {
        "D:/a/_temp/msys64/usr/bin/bash.exe",
        "C:/msys64/usr/bin/bash.exe",
    };
    for (const char *p : kFallback) {
        if (QFile::exists(QString::fromLocal8Bit(p)))
            return QString::fromLocal8Bit(p);
    }
    return {};
}
#endif

class TestFujiNetProcess : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // FujiNetProcessManager::start() checks QFile::exists() and ExeUser permission.
    // We create tiny helper scripts in the temp dir so we control permissions.
    // On Windows, QProcess can't run .sh directly so we launch via m_bash.
    QString m_sleepScript;
    QString m_exitScript;
    QString m_echoScript;
#ifdef Q_OS_WIN
    QString m_bash;
#endif

    bool startHelper(FujiNetProcessManager &mgr, const QString &script,
                     const QStringList &args = {})
    {
#ifdef Q_OS_WIN
        QStringList fullArgs = {script};
        fullArgs.append(args);
        return mgr.start(m_bash, fullArgs);
#else
        return mgr.start(script, args);
#endif
    }

    void createHelperScripts()
    {
#ifdef Q_OS_WIN
        // On Windows/MSYS2, .bat files spawn cmd.exe subprocess trees that
        // QProcess::kill() cannot reliably terminate. Instead, launch
        // usr/bin/bash.exe from the same MSYS2 root as mingw64 (see resolveWindowsMsysBash).
        m_bash = resolveWindowsMsysBash();

        m_sleepScript = m_tempDir.path() + "/helper_sleep.sh";
        QFile sf(m_sleepScript);
        sf.open(QIODevice::WriteOnly);
        sf.write("#!/bin/sh\nsleep \"$1\"\n");
        sf.close();

        m_exitScript = m_tempDir.path() + "/helper_exit.sh";
        QFile ef(m_exitScript);
        ef.open(QIODevice::WriteOnly);
        ef.write("#!/bin/sh\nexit \"$1\"\n");
        ef.close();

        m_echoScript = m_tempDir.path() + "/helper_echo.sh";
        QFile eof(m_echoScript);
        eof.open(QIODevice::WriteOnly);
        eof.write("#!/bin/sh\necho \"$@\"\n");
        eof.close();
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
        if (m_bash.isEmpty() || !QFile::exists(m_bash)) {
            QSKIP("No MSYS2 bash.exe found; cannot run process tests on this Windows runner");
        }
        qDebug() << "Using bash:" << m_bash;
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

        bool result = startHelper(mgr, m_sleepScript, {"30"});
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

        startHelper(mgr, m_sleepScript, {"30"});
        if (startedSpy.isEmpty()) {
            QVERIFY(startedSpy.wait(5000));
        }

        bool secondStart = startHelper(mgr, m_sleepScript, {"30"});
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

        startHelper(mgr, m_sleepScript, {"60"});
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

        startHelper(mgr, m_sleepScript, {"60"});
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

        startHelper(mgr, m_exitScript, {"42"});

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

        startHelper(mgr, m_echoScript, {"hello_fujisan"});

        if (stoppedSpy.isEmpty()) {
            QVERIFY(stoppedSpy.wait(5000));
        }

        // stdout should contain our message
        QVERIFY(mgr.getStdout().contains("hello_fujisan"));
    }
};

QTEST_MAIN(TestFujiNetProcess)
#include "test_fujinet_process.moc"
