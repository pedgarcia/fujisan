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
#include <QRegularExpression>

FujiNetProcessManager::FujiNetProcessManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_state(NotRunning)
    , m_launchBehavior(AutoLaunch)
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

    // Check if FujiNet-PC is already running externally (not managed by us)
    if (isProcessRunningExternally("fujinet")) {
        qDebug() << "FujiNet-PC process already running externally - attaching to external process";
        // Set state to Running even though we didn't start it
        // This allows UI to show correct state and cold boot logic to work
        setState(Running);
        emit processStarted();
        return true; // Return success - we have a running FujiNet-PC
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
    m_process->setWorkingDirectory(workingDir);

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
    m_process->start(m_binaryPath, m_arguments);

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
        // External process - use pkill to stop it
        qDebug() << "Stopping external FujiNet-PC process with pkill";
        QProcess pkill;
        pkill.start("pkill", QStringList() << "-TERM" << "fujinet");
        pkill.waitForFinished(2000);

        // Give it a moment, then force kill if needed
        QThread::msleep(1000);
        if (isProcessRunningExternally("fujinet")) {
            qDebug() << "External FujiNet-PC did not terminate, force killing";
            QProcess pkillForce;
            pkillForce.start("pkill", QStringList() << "-9" << "fujinet");
            pkillForce.waitForFinished(2000);
        }
    } else {
        // Process we started - use QProcess methods
        m_process->terminate();

        // Wait up to 5 seconds for graceful shutdown
        if (!m_process->waitForFinished(5000)) {
            qWarning() << "FujiNet-PC did not terminate gracefully, killing process";
            m_process->kill();
            m_process->waitForFinished(1000);
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

void FujiNetProcessManager::clearOutputBuffers()
{
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
}

bool FujiNetProcessManager::isProcessRunningExternally(const QString& processName)
{
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
        m_lastError = "FujiNet-PC crashed";
        setState(Error);
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

    // Check if general text filtering is enabled
    QString filterString = settings.value("log/filterString", "").toString();
    if (!filterString.isEmpty()) {
        bool useRegex = settings.value("log/useRegex", false).toBool();

        if (useRegex) {
            // Use regular expression matching
            QRegularExpression regex(filterString);
            if (regex.isValid() && regex.match(message).hasMatch()) {
                return true;  // Filter out this message
            }
        } else {
            // Simple string contains (case-sensitive)
            if (message.contains(filterString, Qt::CaseSensitive)) {
                return true;  // Filter out this message
            }
        }
    }

    return false;  // Don't filter this message
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
            if (trimmed.contains("ATR READ")) {
                int drive = m_lastDeviceId - 0x30;
                if (drive >= 1 && drive <= 8) {
                    startDriveOperation(drive, false); // READ
                    m_lastDeviceId = -1;
                }
                continue;
            }
            if (trimmed.contains("ATR WRITE")) {
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
    qDebug() << "FujiNet D" << driveNumber << (state.isWriting ? "WRITE" : "READ")
             << "completed in" << duration << "ms";

    state.isPending = false;

    // Turn LED OFF
    emit diskIOEnd(driveNumber);
}

void FujiNetProcessManager::onDriveOperationTimeout(int driveNumber)
{
    qWarning() << "FujiNet D" << driveNumber << "operation timeout (2000ms) - forcing LED off";
    completeDriveOperation(driveNumber);
}
