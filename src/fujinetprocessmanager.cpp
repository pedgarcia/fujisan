/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "fujinetprocessmanager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QSettings>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QtGlobal>

namespace {
/* Quit / stop: shorter waits than generic defaults — user expects the window to close promptly. */
constexpr int kManagedGracefulStopMs = 800;
constexpr int kAfterKillWaitMs = 800;
constexpr int kExternalSignalWaitMs = 1200;
constexpr int kExternalPollIntervalMs = 100;
constexpr int kExternalPollMaxIterations = 25;
} // namespace

#ifdef Q_OS_WIN
static QString fujisanSystem32Exe(const QString &fileName)
{
    const QString root = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("SystemRoot"), QStringLiteral("C:/Windows"));
    const QString p = QDir(root).filePath(QStringLiteral("System32/") + fileName);
    return QDir::toNativeSeparators(QDir::cleanPath(p));
}
#endif

FujiNetProcessManager::FujiNetProcessManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_state(NotRunning)
    , m_launchBehavior(AutoLaunch)
    , m_skipExternalCheck(false)
    , m_lastExitCode(0)
    , m_lastExitStatus(QProcess::NormalExit)
    , m_startTimer(new QTimer(this))
{
    // Connect process signals
    connect(m_process, &QProcess::started, this, &FujiNetProcessManager::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FujiNetProcessManager::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &FujiNetProcessManager::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &FujiNetProcessManager::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &FujiNetProcessManager::onReadyReadStderr);

    // Connect start timeout
    m_startTimer->setSingleShot(true);
    connect(m_startTimer, &QTimer::timeout, this, &FujiNetProcessManager::onStartTimeout);
}

FujiNetProcessManager::~FujiNetProcessManager()
{
    if (m_process->state() != QProcess::NotRunning) {
        stop();
    }
}

bool FujiNetProcessManager::start(const QString& binaryPath, const QStringList& arguments)
{
    if (m_state == Running || m_state == Starting) {
        qWarning() << "FujiNet-PC is already running or starting";
        return false;
    }

    // Skip isProcessRunningExternally() after forceKill() — we just killed the process and are
    // restarting it, so there is no external instance; that check runs a nested event loop
    // (tasklist.waitForFinished) which can crash the emulator timer on Windows.
    if (m_skipExternalCheck) {
        qDebug() << "Skipping isProcessRunningExternally() after forceKill";
        m_skipExternalCheck = false;
    } else if (qEnvironmentVariableIsSet("FUJISAN_TEST_SKIP_EXTERNAL_FUJINET_CHECK")) {
        // Unit tests spawn non-FujiNet children; skip tasklist/taskkill (MSYS PATH,
        // nested event loops). Set only from test_fujinet_process.
    } else if (isProcessRunningExternally("fujinet")) {
        // Kill any stale/external FujiNet-PC process before launching a managed one.
        // Previous sessions may leave zombie or orphaned processes behind; pgrep can
        // match those and, if we short-circuited here, the state would be stuck at
        // Running with no QProcess and no onProcessFinished signal — leaving the
        // health-check polling a dead server forever.
        // The "Detect existing" launch mode already handles attaching to a user-started
        // external process in MainWindow before calling start().
        qDebug() << "Found existing FujiNet-PC process - killing before managed launch";
#ifdef Q_OS_WIN
        QProcess taskkill;
        taskkill.start(fujisanSystem32Exe(QStringLiteral("taskkill.exe")),
                       QStringList() << "/F" << "/IM" << "fujinet.exe");
        taskkill.waitForFinished(2000);
#else
        QProcess pkill;
        pkill.start("pkill", QStringList() << "-9" << "fujinet");
        pkill.waitForFinished(2000);
#endif
    }

    m_binaryPath = binaryPath.isEmpty() ? m_binaryPath : binaryPath;
    m_arguments = arguments.isEmpty() ? m_arguments : arguments;

    // Check if binary exists
    if (!QFile::exists(m_binaryPath)) {
        m_lastError = QString("Binary not found: %1").arg(m_binaryPath);
        qWarning() << m_lastError;
        setState(Error);
        emit processError(m_lastError);
        return false;
    }

    // Check if binary is executable (Unix-like systems)
#ifndef _WIN32
    QFile file(m_binaryPath);
    if (!file.permissions().testFlag(QFile::ExeUser)) {
        m_lastError = QString("Binary is not executable: %1").arg(m_binaryPath);
        qWarning() << m_lastError;
        setState(Error);
        emit processError(m_lastError);
        return false;
    }
#endif

    // Set working directory to the binary's directory
    // FujiNet-PC expects fnconfig.ini, data/, and SD/ folders in its working directory
    QFileInfo fileInfo(m_binaryPath);
    QString workingDir = fileInfo.absolutePath();
#ifdef Q_OS_WIN
    m_process->setWorkingDirectory(QDir::toNativeSeparators(workingDir));
#else
    m_process->setWorkingDirectory(workingDir);
#endif

    // Verify required files exist
    QString configPath = workingDir + "/fnconfig.ini";
    QString dataPath = workingDir + "/data";

    if (!QFile::exists(configPath)) {
        m_lastError = QString("Required file not found: %1\nFujiNet-PC may not function properly").arg(configPath);
        qWarning() << m_lastError;
        // Don't fail - let FujiNet create default config
    }

    if (!QDir(dataPath).exists()) {
        m_lastError = QString("Required data folder not found: %1\nFujiNet-PC may not function properly").arg(dataPath);
        qWarning() << m_lastError;
        // Don't fail - continue anyway
    }

    qDebug() << "Starting FujiNet-PC:" << m_binaryPath << m_arguments;
    qDebug() << "Working directory:" << workingDir;

    setState(Starting);
#ifdef Q_OS_WIN
    m_process->start(QDir::toNativeSeparators(m_binaryPath), m_arguments);
#else
    m_process->start(m_binaryPath, m_arguments);
#endif

    // Start timeout timer
    m_startTimer->start(START_TIMEOUT_MS);

    return true;
}

void FujiNetProcessManager::stop()
{
    if (m_state == NotRunning) {
        qDebug() << "FujiNet-PC is not running";
        return;
    }

    qDebug() << "Stopping FujiNet-PC";
    setState(Stopping);

    // Check if we're managing the process or just tracking an external one
    if (m_process->state() == QProcess::NotRunning) {
        // External process - use platform-specific kill command
#ifdef Q_OS_WIN
        qDebug() << "Stopping external FujiNet-PC process with taskkill";
        QProcess taskkill;
        taskkill.start(fujisanSystem32Exe(QStringLiteral("taskkill.exe")),
                       QStringList() << "/IM" << "fujinet.exe");
        taskkill.waitForFinished(kExternalSignalWaitMs);

        for (int i = 0; i < kExternalPollMaxIterations
             && isProcessRunningExternally(QStringLiteral("fujinet")); ++i) {
            QThread::msleep(kExternalPollIntervalMs);
        }
        if (isProcessRunningExternally("fujinet")) {
            qDebug() << "External FujiNet-PC did not terminate, force killing";
            QProcess taskkillForce;
            taskkillForce.start(fujisanSystem32Exe(QStringLiteral("taskkill.exe")),
                                QStringList() << "/F" << "/IM" << "fujinet.exe");
            taskkillForce.waitForFinished(kExternalSignalWaitMs);
        }
#else
        qDebug() << "Stopping external FujiNet-PC process with pkill";
        QProcess pkill;
        pkill.start("pkill", QStringList() << "-TERM" << "fujinet");
        pkill.waitForFinished(kExternalSignalWaitMs);

        for (int i = 0; i < kExternalPollMaxIterations
             && isProcessRunningExternally(QStringLiteral("fujinet")); ++i) {
            QThread::msleep(kExternalPollIntervalMs);
        }
        if (isProcessRunningExternally("fujinet")) {
            qDebug() << "External FujiNet-PC did not terminate, force killing";
            QProcess pkillForce;
            pkillForce.start("pkill", QStringList() << "-9" << "fujinet");
            pkillForce.waitForFinished(kExternalSignalWaitMs);
        }
#endif
    } else {
        // Process we started - use QProcess methods
        m_process->terminate();

        if (!m_process->waitForFinished(kManagedGracefulStopMs)) {
            qWarning() << "FujiNet-PC did not terminate gracefully, killing process";
            m_process->kill();
            m_process->waitForFinished(kAfterKillWaitMs);
        }
    }

    setState(NotRunning);
}

void FujiNetProcessManager::restart()
{
    qDebug() << "Restarting FujiNet-PC";
    stop();
    start(m_binaryPath, m_arguments);
}

void FujiNetProcessManager::forceKill()
{
    if (m_state == NotRunning) {
        qDebug() << "FujiNet-PC is not running, nothing to kill";
        return;
    }

    qDebug() << "Force-killing FujiNet-PC (no graceful shutdown)";
    // Signal start() to skip isProcessRunningExternally() — we know the process is dead
    // and that call runs a nested event loop (tasklist) on Windows which can crash.
    m_skipExternalCheck = true;
    setState(Stopping);

    if (m_process->state() != QProcess::NotRunning) {
        // Process we started — SIGKILL / TerminateProcess
        m_process->kill();
#ifdef Q_OS_WIN
        // On Windows, waitForFinished() uses MsgWaitForMultipleObjects which pumps
        // the Qt event loop. The emulator frame timer can fire during that nested loop
        // and crash. Instead, transition to NotRunning immediately; when the async
        // onProcessFinished(CrashExit) signal arrives, its guard will skip setState(Error).
        setState(NotRunning);
#else
        m_process->waitForFinished(1000);
        if (m_process->state() == QProcess::NotRunning) {
            setState(NotRunning);
        }
#endif
    } else {
        // Externally running process — no QProcess::finished signal will come,
        // so we must transition state ourselves after the kill completes.
#ifdef Q_OS_WIN
        QProcess taskkill;
        taskkill.start(fujisanSystem32Exe(QStringLiteral("taskkill.exe")),
                       QStringList() << "/F" << "/IM" << "fujinet.exe");
        taskkill.waitForFinished(1000);
#else
        QProcess pkill;
        pkill.start("pkill", QStringList() << "-9" << "fujinet");
        pkill.waitForFinished(1000);
#endif
        setState(NotRunning);
    }
}

void FujiNetProcessManager::clearOutputBuffers()
{
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
}

bool FujiNetProcessManager::isProcessRunningExternally(const QString& processName)
{
#ifdef Q_OS_WIN
    // Use tasklist to check if process is running (Windows)
    QString imageName = processName;
    if (!imageName.endsWith(".exe", Qt::CaseInsensitive)) {
        imageName += ".exe";
    }
    QProcess tasklist;
    tasklist.start(fujisanSystem32Exe(QStringLiteral("tasklist.exe")),
                   QStringList() << "/FI" << QString("IMAGENAME eq %1").arg(imageName) << "/NH");
    tasklist.waitForFinished(1000);

    // tasklist outputs lines; if process exists, we get a line containing the image name
    QByteArray output = tasklist.readAllStandardOutput();
    bool isRunning = output.toLower().contains(imageName.toUtf8().toLower());

    if (isRunning) {
        qDebug() << "Detected existing" << imageName << "process";
    }

    return isRunning;
#else
    // Use pgrep to check if process is running (Unix/Linux/macOS)
    QProcess pgrep;
    pgrep.start("pgrep", QStringList() << "-x" << processName);
    pgrep.waitForFinished(1000);

    // pgrep returns 0 if process found, 1 if not found
    bool isRunning = (pgrep.exitCode() == 0);

    if (isRunning) {
        qDebug() << "Detected existing" << processName << "process";
    }

    return isRunning;
#endif
}

// Private slots

void FujiNetProcessManager::onProcessStarted()
{
    m_startTimer->stop();
    qDebug() << "FujiNet-PC started successfully";
    setState(Running);
    emit processStarted();
}

void FujiNetProcessManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "FujiNet-PC exited with code:" << exitCode << "status:" << exitStatus;

    // Store exit code for auto-restart logic
    m_lastExitCode = exitCode;
    m_lastExitStatus = exitStatus;

    if (exitStatus == QProcess::CrashExit) {
        // On Windows, forceKill() already called setState(NotRunning) immediately
        // (to avoid waitForFinished's nested event loop). Don't regress to Error.
        if (m_state != NotRunning) {
            m_lastError = "FujiNet-PC crashed";
            setState(Error);
        }
    } else {
        setState(NotRunning);
    }

    emit processStopped(exitCode, exitStatus);
}

void FujiNetProcessManager::onProcessError(QProcess::ProcessError error)
{
    m_startTimer->stop();

    m_lastError = processErrorToString(error);
    qWarning() << "FujiNet-PC process error:" << m_lastError;

    setState(Error);
    emit processError(m_lastError);
}

void FujiNetProcessManager::onReadyReadStdout()
{
    QString output = m_process->readAllStandardOutput();
    if (!output.isEmpty()) {
        m_stdoutBuffer.append(output);

        // Trim buffer if it gets too large
        if (m_stdoutBuffer.size() > MAX_BUFFER_SIZE) {
            m_stdoutBuffer = m_stdoutBuffer.right(MAX_BUFFER_SIZE / 2);
        }

        // Parse for LED activity (works regardless of log/hideFujiNetLogs setting)
        parseFujiNetLogsForLEDActivity(output);

        // Check if FujiNet-PC logs should be hidden
        QSettings settings("8bitrelics", "Fujisan");
        bool hideFujiNetLogs = settings.value("log/hideFujiNetLogs", false).toBool();

        if (!hideFujiNetLogs) {
            // Convert literal \r\n and \n to actual newlines for better formatting
            QString formatted = output;
            formatted.replace("\\r\\n", "\n").replace("\\n", "\n");

            // Log each line separately for cleaner output
            QStringList lines = formatted.split('\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() && !shouldFilterLogMessage(trimmed)) {
                    qDebug() << "[FUJINET]" << trimmed;
                }
            }
        }

        emit stdoutReceived(output);
    }
}

void FujiNetProcessManager::onReadyReadStderr()
{
    QString output = m_process->readAllStandardError();
    if (!output.isEmpty()) {
        m_stderrBuffer.append(output);

        // Trim buffer if it gets too large
        if (m_stderrBuffer.size() > MAX_BUFFER_SIZE) {
            m_stderrBuffer = m_stderrBuffer.right(MAX_BUFFER_SIZE / 2);
        }

        // Drive LED parsing (same patterns as stdout; some builds or hosts may log here)
        parseFujiNetLogsForLEDActivity(output);

        // Check if FujiNet-PC logs should be hidden
        QSettings settings("8bitrelics", "Fujisan");
        bool hideFujiNetLogs = settings.value("log/hideFujiNetLogs", false).toBool();

        if (!hideFujiNetLogs) {
            // Convert literal \r\n and \n to actual newlines for better formatting
            QString formatted = output;
            formatted.replace("\\r\\n", "\n").replace("\\n", "\n");

            // Log each line separately for cleaner output
            QStringList lines = formatted.split('\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() && !shouldFilterLogMessage(trimmed)) {
                    qWarning() << "[FUJINET ERROR]" << trimmed;
                }
            }
        }

        emit stderrReceived(output);
    }
}

void FujiNetProcessManager::onStartTimeout()
{
    if (m_state == Starting) {
        m_lastError = "FujiNet-PC failed to start within timeout period";
        qWarning() << m_lastError;

        m_process->kill();
        setState(Error);
        emit processError(m_lastError);
    }
}

// Private methods

void FujiNetProcessManager::setState(ProcessState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

QString FujiNetProcessManager::processErrorToString(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        return "Failed to start FujiNet-PC. The binary may be missing or not executable.";
    case QProcess::Crashed:
        return "FujiNet-PC crashed after starting.";
    case QProcess::Timedout:
        return "Operation timed out.";
    case QProcess::WriteError:
        return "Error writing to FujiNet-PC process.";
    case QProcess::ReadError:
        return "Error reading from FujiNet-PC process.";
    case QProcess::UnknownError:
    default:
        return "Unknown error occurred.";
    }
}

bool FujiNetProcessManager::shouldFilterLogMessage(const QString& message)
{
    QSettings settings("8bitrelics", "Fujisan");

    QString filterString = settings.value("log/filterString", "").toString();
    if (filterString.isEmpty()) {
        return false;  // No filter: show all lines
    }

    bool useRegex = settings.value("log/useRegex", false).toBool();

    if (useRegex) {
        QRegularExpression regex(filterString);
        if (!regex.isValid()) {
            return false;  // Invalid pattern: show lines (same as before)
        }
        // Include only lines that match the regex
        return !regex.match(message).hasMatch();
    }

    // Include only lines that contain the substring (case-sensitive)
    return !message.contains(filterString, Qt::CaseSensitive);
}

// FujiNet log parsing for LED activity
void FujiNetProcessManager::parseFujiNetLogsForLEDActivity(const QString& output)
{
    // Split into lines and parse each one
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Step 1: Device ID detection
        if (trimmed.contains("CF:")) {
            parseDeviceId(trimmed);
            continue;
        }

        // Step 2: Operation detection (requires cached device ID)
        if (m_lastDeviceId != -1) {
            // FujiNet-PC logs disk I/O per image type (ATR / ATX / XEX). Fujisan must
            // recognize all of them; only matching "ATR READ" misses common .atx/.xex use.
            const bool atrRead = trimmed.contains(QLatin1String("ATR READ"));
            const bool atrWrite = trimmed.contains(QLatin1String("ATR WRITE"));
            const bool atxRead = trimmed.contains(QLatin1String("ATX READ"));
            const bool xexRead = trimmed.contains(QLatin1String("XEX READ"));
            if (atrRead || atxRead || xexRead) {
                int drive = m_lastDeviceId - 0x30;
                if (drive >= 1 && drive <= 8) {
                    startDriveOperation(drive, false); // READ
                    m_lastDeviceId = -1;
                }
                continue;
            }
            if (atrWrite) {
                int drive = m_lastDeviceId - 0x30;
                if (drive >= 1 && drive <= 8) {
                    startDriveOperation(drive, true); // WRITE
                    m_lastDeviceId = -1;
                }
                continue;
            }
        }

        // Step 3: Completion detection
        if (trimmed.contains("COMPLETE")) {
            parseCompletion(trimmed);
        }
    }
}

void FujiNetProcessManager::parseDeviceId(const QString& line)
{
    // Extract from "CF: 31 52 6c 01 f0"
    int cfIndex = line.indexOf("CF:");
    if (cfIndex == -1) return;

    QString afterCF = line.mid(cfIndex + 3).trimmed();
    QStringList bytes = afterCF.split(' ', Qt::SkipEmptyParts);
    if (bytes.isEmpty()) return;

    bool ok;
    int deviceId = bytes[0].toInt(&ok, 16);  // Parse hex
    if (ok && deviceId >= 0x31 && deviceId <= 0x38) {
        m_lastDeviceId = deviceId;

        // 500ms timeout to clear stale device ID
        QTimer::singleShot(500, this, [this, deviceId]() {
            if (m_lastDeviceId == deviceId) {
                m_lastDeviceId = -1;
            }
        });
    }
}

void FujiNetProcessManager::startDriveOperation(int driveNumber, bool isWriting)
{
    // Force-complete any pending operation on this drive
    if (m_driveStates.contains(driveNumber) && m_driveStates[driveNumber].isPending) {
        completeDriveOperation(driveNumber);
    }

    // Initialize state structure if needed
    if (!m_driveStates.contains(driveNumber)) {
        DriveIOState state;
        state.timeoutTimer = new QTimer(this);
        state.timeoutTimer->setSingleShot(true);
        connect(state.timeoutTimer, &QTimer::timeout, this, [this, driveNumber]() {
            onDriveOperationTimeout(driveNumber);
        });
        m_driveStates[driveNumber] = state;
    }

    // Start operation
    m_driveStates[driveNumber].isPending = true;
    m_driveStates[driveNumber].isWriting = isWriting;
    m_driveStates[driveNumber].startTime = QDateTime::currentMSecsSinceEpoch();
    m_driveStates[driveNumber].timeoutTimer->start(2000);

    // Turn LED ON
    emit diskIOStart(driveNumber, isWriting);
}

void FujiNetProcessManager::parseCompletion(const QString& line)
{
    Q_UNUSED(line);

    // Find first pending operation (FujiNet is typically sequential)
    for (auto it = m_driveStates.begin(); it != m_driveStates.end(); ++it) {
        if (it.value().isPending) {
            completeDriveOperation(it.key());
            return;
        }
    }
}

void FujiNetProcessManager::completeDriveOperation(int driveNumber)
{
    if (!m_driveStates.contains(driveNumber)) return;

    DriveIOState& state = m_driveStates[driveNumber];
    if (!state.isPending) return;

    // Cancel timeout
    state.timeoutTimer->stop();

    // Debug logging
    qint64 duration = QDateTime::currentMSecsSinceEpoch() - state.startTime;

    state.isPending = false;

    // Turn LED OFF
    emit diskIOEnd(driveNumber);
}

void FujiNetProcessManager::onDriveOperationTimeout(int driveNumber)
{
    qWarning() << "FujiNet D" << driveNumber << "operation timeout (2000ms) - forcing LED off";
    completeDriveOperation(driveNumber);
}
