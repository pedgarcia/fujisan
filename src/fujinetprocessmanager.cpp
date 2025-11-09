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

FujiNetProcessManager::FujiNetProcessManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_state(NotRunning)
    , m_launchBehavior(AutoLaunch)
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

    // Try graceful termination first
    m_process->terminate();

    // Wait up to 5 seconds for graceful shutdown
    if (!m_process->waitForFinished(5000)) {
        qWarning() << "FujiNet-PC did not terminate gracefully, killing process";
        m_process->kill();
        m_process->waitForFinished(1000);
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

        // Log FujiNet output for debugging
        qDebug() << "[FujiNet-PC stdout]" << output.trimmed();

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

        // Log FujiNet errors for debugging
        qWarning() << "[FujiNet-PC stderr]" << output.trimmed();

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
