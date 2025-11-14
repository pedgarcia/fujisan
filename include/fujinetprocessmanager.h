/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef FUJINETPROCESSMANAGER_H
#define FUJINETPROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

class FujiNetProcessManager : public QObject
{
    Q_OBJECT

public:
    enum LaunchBehavior {
        AutoLaunch,      // Automatically launch when NetSIO is enabled
        DetectExisting,  // Detect if FujiNet-PC is already running
        Manual           // User manually controls start/stop
    };

    enum ProcessState {
        NotRunning,
        Starting,
        Running,
        Stopping,
        Error
    };

    explicit FujiNetProcessManager(QObject *parent = nullptr);
    ~FujiNetProcessManager();

    // Process control
    bool start(const QString& binaryPath, const QStringList& arguments = QStringList());
    void stop();
    void restart();

    // State queries
    ProcessState getState() const { return m_state; }
    bool isRunning() const { return m_state == Running; }
    QString getLastError() const { return m_lastError; }
    QString getStdout() const { return m_stdoutBuffer; }
    QString getStderr() const { return m_stderrBuffer; }
    int getLastExitCode() const { return m_lastExitCode; }
    QProcess::ExitStatus getLastExitStatus() const { return m_lastExitStatus; }

    // Configuration
    void setLaunchBehavior(LaunchBehavior behavior) { m_launchBehavior = behavior; }
    LaunchBehavior getLaunchBehavior() const { return m_launchBehavior; }

    void setBinaryPath(const QString& path) { m_binaryPath = path; }
    QString getBinaryPath() const { return m_binaryPath; }

    void setArguments(const QStringList& args) { m_arguments = args; }
    QStringList getArguments() const { return m_arguments; }

    // Output buffer management
    void clearOutputBuffers();

    // Process detection
    static bool isProcessRunningExternally(const QString& processName = "fujinet");

signals:
    void stateChanged(ProcessState newState);
    void processStarted();
    void processStopped(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(const QString& error);
    void stdoutReceived(const QString& output);
    void stderrReceived(const QString& output);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onStartTimeout();

private:
    void setState(ProcessState newState);
    QString processErrorToString(QProcess::ProcessError error);

    QProcess* m_process;
    ProcessState m_state;
    LaunchBehavior m_launchBehavior;
    QString m_binaryPath;
    QStringList m_arguments;
    QString m_lastError;
    int m_lastExitCode;
    QProcess::ExitStatus m_lastExitStatus;

    // Output buffers
    QString m_stdoutBuffer;
    QString m_stderrBuffer;
    static const int MAX_BUFFER_SIZE = 100000;  // 100KB max buffer

    // Start timeout
    QTimer* m_startTimer;
    static const int START_TIMEOUT_MS = 10000;  // 10 seconds
};

#endif // FUJINETPROCESSMANAGER_H
