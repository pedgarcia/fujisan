/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QList>
#include <QMap>

// Forward declarations
class AtariEmulator;
class DebuggerWidget;
class MainWindow;

class TCPServer : public QObject
{
    Q_OBJECT

public:
    explicit TCPServer(AtariEmulator* emulator, MainWindow* mainWindow, QObject* parent = nullptr);
    ~TCPServer();
    
    // Server control
    bool startServer(quint16 port = 8080);
    void stopServer();
    bool isRunning() const;
    quint16 serverPort() const;
    
    // Client management
    int connectedClientCount() const;
    void setDebuggerWidget(DebuggerWidget* debugger);

public slots:
    void sendEventToAllClients(const QString& eventType, const QJsonObject& data);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientDataReady();
    void processCommand(QTcpSocket* client, const QJsonObject& request);

private:
    // Command handlers
    void handleMediaCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand);
    void handleSystemCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand);
    void handleInputCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand);
    void handleDebugCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand);
    void handleConfigCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand);
    void handleStatusCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand);
    void handleScreenCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand);
    
    // Utility methods
    void sendResponse(QTcpSocket* client, const QJsonValue& requestId, bool success, 
                     const QJsonValue& result = QJsonValue(), const QString& error = QString());
    void sendEvent(QTcpSocket* client, const QString& eventType, const QJsonObject& data);
    QJsonObject parseJsonMessage(const QByteArray& data, bool& success);
    QString validateAndNormalizePath(const QString& path);
    
    // Member variables
    QTcpServer* m_server;
    QList<QTcpSocket*> m_clients;
    QMap<QTcpSocket*, QByteArray> m_clientBuffers; // For handling partial JSON messages
    
    AtariEmulator* m_emulator;
    DebuggerWidget* m_debugger;
    MainWindow* m_mainWindow;
    
    quint16 m_port;
    bool m_isRunning;
    
    // Command statistics (for debugging/monitoring)
    QMap<QString, int> m_commandStats;
};

#endif // TCPSERVER_H