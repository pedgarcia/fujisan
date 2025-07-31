/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "tcpserver.h"
#include "atariemulator.h"
#include "debuggerwidget.h"
#include "mainwindow.h"
#include "configurationprofile.h"
#include "configurationprofilemanager.h"
#include "disasm6502.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>
#include <QBuffer>
#include <QImageReader>
#include <QDateTime>

extern "C" {
    // Access to CPU registers for debug commands
    extern unsigned short CPU_regPC;
    // Access to screen functions for screen capture
    #include "../../src/screen.h"
    extern unsigned char CPU_regA;
    extern unsigned char CPU_regX;
    extern unsigned char CPU_regY;
    extern unsigned char CPU_regS;
    extern unsigned char CPU_regP;
    
    // Access to memory for debug commands
    extern unsigned char MEMORY_mem[65536];
    
    // AKEY constants for input commands
    #include "akey.h"
}

TCPServer::TCPServer(AtariEmulator* emulator, MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_emulator(emulator)
    , m_debugger(nullptr)
    , m_mainWindow(mainWindow)
    , m_port(8080)
    , m_isRunning(false)
    , m_joystickStreamTimer(nullptr)
    , m_lastJoy0State(0x0f ^ 0xff)  // Center position
    , m_lastJoy1State(0x0f ^ 0xff)  // Center position
    , m_lastTrig0(false)
    , m_lastTrig1(false)
{
    // Connect server signals
    connect(m_server, &QTcpServer::newConnection, this, &TCPServer::onNewConnection);
    
    qDebug() << "TCP Server initialized - ready to start on port" << m_port;
}

TCPServer::~TCPServer()
{
    stopServer();
}

bool TCPServer::startServer(quint16 port)
{
    if (m_isRunning) {
        qDebug() << "TCP Server already running on port" << m_port;
        return true;
    }
    
    m_port = port;
    
    // Listen only on localhost for security
    if (!m_server->listen(QHostAddress::LocalHost, m_port)) {
        qDebug() << "Failed to start TCP Server on port" << m_port 
                 << "Error:" << m_server->errorString();
        return false;
    }
    
    m_isRunning = true;
    qDebug() << "TCP Server started successfully on localhost:" << m_port;
    qDebug() << "Clients can connect to: http://localhost:" << m_port;
    
    return true;
}

void TCPServer::stopServer()
{
    if (!m_isRunning) {
        return;
    }
    
    // Disconnect all clients
    for (QTcpSocket* client : m_clients) {
        client->disconnectFromHost();
        if (client->state() != QAbstractSocket::UnconnectedState) {
            client->waitForDisconnected(1000);
        }
    }
    m_clients.clear();
    m_clientBuffers.clear();
    
    // Stop server
    m_server->close();
    m_isRunning = false;
    
    qDebug() << "TCP Server stopped";
}

bool TCPServer::isRunning() const
{
    return m_isRunning;
}

quint16 TCPServer::serverPort() const
{
    return m_port;
}

int TCPServer::connectedClientCount() const
{
    return m_clients.count();
}

void TCPServer::setDebuggerWidget(DebuggerWidget* debugger)
{
    m_debugger = debugger;
    qDebug() << "TCP Server: Debugger widget" << (debugger ? "connected" : "disconnected");
}

void TCPServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* client = m_server->nextPendingConnection();
        
        // Set up client connection
        connect(client, &QTcpSocket::disconnected, this, &TCPServer::onClientDisconnected);
        connect(client, &QTcpSocket::readyRead, this, &TCPServer::onClientDataReady);
        
        m_clients.append(client);
        m_clientBuffers[client] = QByteArray();
        
        QString clientAddress = client->peerAddress().toString();
        qDebug() << "TCP Server: New client connected from" << clientAddress 
                 << "Total clients:" << m_clients.count();
        
        // Send welcome message
        QJsonObject welcomeData;
        welcomeData["version"] = "1.0";
        welcomeData["emulator"] = "Fujisan";
        welcomeData["capabilities"] = QJsonArray{"media", "system", "input", "debug", "config", "status", "screen"};
        sendEvent(client, "connected", welcomeData);
    }
}

void TCPServer::onClientDisconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    QString clientAddress = client->peerAddress().toString();
    m_clients.removeAll(client);
    m_clientBuffers.remove(client);
    
    // Remove from joystick streaming if subscribed
    m_joystickStreamClients.remove(client);
    if (m_joystickStreamClients.isEmpty() && m_joystickStreamTimer) {
        m_joystickStreamTimer->stop();
    }
    
    qDebug() << "TCP Server: Client disconnected from" << clientAddress
             << "Remaining clients:" << m_clients.count();
    
    client->deleteLater();
}

void TCPServer::onClientDataReady()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    // Read all available data and append to client buffer
    QByteArray newData = client->readAll();
    m_clientBuffers[client].append(newData);
    
    // Process complete JSON messages (delimited by newlines)
    QByteArray& buffer = m_clientBuffers[client];
    int newlineIndex;
    
    while ((newlineIndex = buffer.indexOf('\n')) != -1) {
        QByteArray jsonData = buffer.left(newlineIndex);
        buffer.remove(0, newlineIndex + 1);
        
        // Skip empty lines
        if (jsonData.trimmed().isEmpty()) {
            continue;
        }
        
        // Parse and process JSON command
        bool parseSuccess;
        QJsonObject request = parseJsonMessage(jsonData, parseSuccess);
        
        if (parseSuccess) {
            processCommand(client, request);
        } else {
            // Send error response for invalid JSON
            sendResponse(client, QJsonValue(), false, QJsonValue(), 
                        "Invalid JSON format: " + QString::fromUtf8(jsonData));
        }
    }
}

QJsonObject TCPServer::parseJsonMessage(const QByteArray& data, bool& success)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "TCP Server: JSON parse error:" << error.errorString();
        success = false;
        return QJsonObject();
    }
    
    if (!doc.isObject()) {
        qDebug() << "TCP Server: JSON is not an object";
        success = false;
        return QJsonObject();
    }
    
    success = true;
    return doc.object();
}

void TCPServer::processCommand(QTcpSocket* client, const QJsonObject& request)
{
    qDebug() << "TCP Server: processCommand called with request:" << request;
    
    // Extract command and validate request format
    QString command = request["command"].toString();
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    
    qDebug() << "TCP Server: Extracted command:" << command;
    
    if (command.isEmpty()) {
        qDebug() << "TCP Server: Command is empty!";
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Missing or invalid 'command' field");
        return;
    }
    
    // Update command statistics
    m_commandStats[command]++;
    
    qDebug() << "TCP Server: Processing command:" << command;
    
    // Route command to appropriate handler
    QStringList parts = command.split('.');
    if (parts.size() != 2) {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Invalid command format. Expected: 'category.action'");
        return;
    }
    
    QString category = parts[0];
    QString subCommand = parts[1];
    
    qDebug() << "TCP Server: Routing command - category:" << category << "subCommand:" << subCommand;
    
    if (category == "media") {
        qDebug() << "TCP Server: Calling handleMediaCommand";
        handleMediaCommand(client, request, subCommand);
    } else if (category == "system") {
        handleSystemCommand(client, request, subCommand);
    } else if (category == "input") {
        handleInputCommand(client, request, subCommand);
    } else if (category == "debug") {
        handleDebugCommand(client, request, subCommand);
    } else if (category == "config") {
        handleConfigCommand(client, request, subCommand);
    } else if (category == "status") {
        handleStatusCommand(client, request, subCommand);
    } else if (category == "screen") {
        handleScreenCommand(client, request, subCommand);
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown command category: " + category);
    }
}

void TCPServer::sendResponse(QTcpSocket* client, const QJsonValue& requestId, bool success,
                           const QJsonValue& result, const QString& error)
{
    QJsonObject response;
    response["type"] = "response";
    response["status"] = success ? "success" : "error";
    
    if (!requestId.isNull()) {
        response["id"] = requestId;
    }
    
    if (success && !result.isNull()) {
        response["result"] = result;
    }
    
    if (!success && !error.isEmpty()) {
        response["error"] = error;
    }
    
    // Send response as JSON + newline
    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    client->write(data);
    client->flush();
}

void TCPServer::sendEvent(QTcpSocket* client, const QString& eventType, const QJsonObject& data)
{
    QJsonObject event;
    event["type"] = "event";
    event["event"] = eventType;
    event["data"] = data;
    
    QJsonDocument doc(event);
    QByteArray eventData = doc.toJson(QJsonDocument::Compact) + "\n";
    client->write(eventData);
    client->flush();
}

void TCPServer::sendEventToAllClients(const QString& eventType, const QJsonObject& data)
{
    for (QTcpSocket* client : m_clients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            sendEvent(client, eventType, data);
        }
    }
}

QString TCPServer::validateAndNormalizePath(const QString& path)
{
    if (path.isEmpty()) {
        return QString();
    }
    
    // Convert to absolute path and check if file exists
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        return QString();
    }
    
    // Return canonical path (resolves symlinks, removes . and ..)
    return fileInfo.canonicalFilePath();
}

// Command handler stubs - to be implemented in the next steps
void TCPServer::handleMediaCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand)
{
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    QJsonObject params = request["params"].toObject();
    
    qDebug() << "TCP Server: handleMediaCommand called with subCommand:" << subCommand;
    
    if (subCommand == "insert_disk") {
        // Insert disk into specified drive
        int drive = params["drive"].toInt();
        QString path = params["path"].toString();
        
        qDebug() << "TCP Server: insert_disk - drive:" << drive << "path:" << path;
        
        if (drive < 1 || drive > 8) {
            qDebug() << "TCP Server: Invalid drive number:" << drive;
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid drive number. Must be 1-8");
            return;
        }
        
        qDebug() << "TCP Server: Skipping path validation for debugging";
        QString validatedPath = path; // Temporarily skip validation
        qDebug() << "TCP Server: Using path directly:" << validatedPath;
        
        qDebug() << "TCP Server: Attempting to insert disk via MainWindow:" << validatedPath;
        
        // Use MainWindow's insertDiskViaTCP method to properly enable drive and update GUI
        bool success = false;
        try {
            success = m_mainWindow->insertDiskViaTCP(drive, validatedPath);
            qDebug() << "TCP Server: insertDiskViaTCP returned:" << success;
        } catch (...) {
            qDebug() << "TCP Server: Exception in insertDiskViaTCP";
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Exception occurred while inserting disk");
            return;
        }
        
        if (success) {
            QJsonObject result;
            result["drive"] = drive;
            result["path"] = validatedPath;
            result["mounted"] = true;
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["drive"] = drive;
            eventData["path"] = validatedPath;
            sendEventToAllClients("disk_inserted", eventData);
            
            // Note: GUI signal emission handled automatically by DiskDriveWidget::insertDisk()
        } else {
            qDebug() << "TCP Server: mountDiskImage failed for:" << validatedPath;
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Failed to mount disk image: " + validatedPath);
        }
        
    } else if (subCommand == "eject_disk") {
        // Eject disk from specified drive
        int drive = params["drive"].toInt();
        
        if (drive < 1 || drive > 8) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid drive number. Must be 1-8");
            return;
        }
        
        bool success = m_mainWindow->ejectDiskViaTCP(drive);
        
        QJsonObject result;
        result["drive"] = drive;
        result["mounted"] = false;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["drive"] = drive;
        sendEventToAllClients("disk_ejected", eventData);
        
        // Note: GUI signal emission handled automatically by DiskDriveWidget::ejectDisk()
        
    } else if (subCommand == "insert_cartridge") {
        // Insert cartridge
        QString path = params["path"].toString();
        
        QString validatedPath = validateAndNormalizePath(path);
        if (validatedPath.isEmpty()) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "File not found or invalid path: " + path);
            return;
        }
        
        // Use MainWindow's insertCartridgeViaTCP method to properly update GUI
        bool success = m_mainWindow->insertCartridgeViaTCP(validatedPath);
        
        if (success) {
            QJsonObject result;
            result["path"] = validatedPath;
            result["loaded"] = true;
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["path"] = validatedPath;
            sendEventToAllClients("cartridge_inserted", eventData);
            
            // Note: GUI signal emission handled automatically by CartridgeWidget::loadCartridge()
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Failed to load cartridge: " + validatedPath);
        }
        
    } else if (subCommand == "eject_cartridge") {
        // Eject cartridge using CartridgeWidget
        bool success = m_mainWindow->ejectCartridgeViaTCP();
        
        if (success) {
            QJsonObject result;
            result["ejected"] = true;
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            sendEventToAllClients("cartridge_ejected", eventData);
            
            // Note: GUI signal emission handled automatically by CartridgeWidget::ejectCartridge()
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Failed to eject cartridge");
        }
        
    } else if (subCommand == "load_xex") {
        // Load XEX executable file
        QString path = params["path"].toString();
        
        QString validatedPath = validateAndNormalizePath(path);
        if (validatedPath.isEmpty()) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "File not found or invalid path: " + path);
            return;
        }
        
        // Use the generic loadFile method for XEX files
        bool success = m_emulator->loadFile(validatedPath);
        
        if (success) {
            QJsonObject result;
            result["path"] = validatedPath;
            result["loaded"] = true;
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["path"] = validatedPath;
            sendEventToAllClients("xex_loaded", eventData);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Failed to load XEX file: " + validatedPath);
        }
        
    } else if (subCommand == "enable_drive") {
        // Enable specified drive
        int drive = params["drive"].toInt();
        
        if (drive < 1 || drive > 8) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid drive number. Must be 1-8");
            return;
        }
        
        bool success = m_mainWindow->enableDriveViaTCP(drive, true);
        
        QJsonObject result;
        result["drive"] = drive;
        result["enabled"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["drive"] = drive;
        eventData["enabled"] = true;
        sendEventToAllClients("drive_state_changed", eventData);
        
    } else if (subCommand == "disable_drive") {
        // Disable specified drive
        int drive = params["drive"].toInt();
        
        if (drive < 1 || drive > 8) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid drive number. Must be 1-8");
            return;
        }
        
        bool success = m_mainWindow->enableDriveViaTCP(drive, false);
        
        QJsonObject result;
        result["drive"] = drive;
        result["enabled"] = false;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["drive"] = drive;
        eventData["enabled"] = false;
        sendEventToAllClients("drive_state_changed", eventData);
        
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown media command: " + subCommand);
    }
}

void TCPServer::handleSystemCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand)
{
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    QJsonObject params = request["params"].toObject();
    
    if (subCommand == "cold_boot") {
        // Perform cold boot (complete restart)
        m_emulator->coldBoot();
        
        QJsonObject result;
        result["boot_type"] = "cold";
        result["restarted"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["boot_type"] = "cold";
        sendEventToAllClients("system_restarted", eventData);
        
    } else if (subCommand == "warm_boot") {
        // Perform warm boot (soft restart)
        m_emulator->warmBoot();
        
        QJsonObject result;
        result["boot_type"] = "warm";
        result["restarted"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["boot_type"] = "warm";
        sendEventToAllClients("system_restarted", eventData);
        
    } else if (subCommand == "restart") {
        // Generic restart - use cold restart for reliability
        m_emulator->coldRestart();
        
        QJsonObject result;
        result["restarted"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        sendEventToAllClients("system_restarted", eventData);
        
    } else if (subCommand == "pause") {
        // Pause emulation
        if (!m_emulator->isEmulationPaused()) {
            m_emulator->pauseEmulation();
            
            QJsonObject result;
            result["paused"] = true;
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["paused"] = true;
            sendEventToAllClients("emulation_paused", eventData);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Emulation is already paused");
        }
        
    } else if (subCommand == "resume") {
        // Resume emulation
        if (m_emulator->isEmulationPaused()) {
            m_emulator->resumeEmulation();
            
            QJsonObject result;
            result["paused"] = false;
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["paused"] = false;
            sendEventToAllClients("emulation_resumed", eventData);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Emulation is not paused");
        }
        
    } else if (subCommand == "set_speed") {
        // Set emulation speed
        int percentage = params["percentage"].toInt();
        
        if (percentage < 10 || percentage > 1000) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid speed percentage. Must be between 10-1000");
            return;
        }
        
        m_emulator->setEmulationSpeed(percentage);
        
        QJsonObject result;
        result["speed_percentage"] = percentage;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["speed_percentage"] = percentage;
        sendEventToAllClients("speed_changed", eventData);
        
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown system command: " + subCommand);
    }
}

void TCPServer::handleInputCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand)
{
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    QJsonObject params = request["params"].toObject();
    
    if (subCommand == "send_text") {
        // Send text string to emulator
        QString text = params["text"].toString();
        
        if (text.isEmpty()) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Text parameter is required and cannot be empty");
            return;
        }
        
        // Inject each character using the emulator's character injection
        for (int i = 0; i < text.length(); ++i) {
            QChar ch = text.at(i);
            m_emulator->injectCharacter(ch.toLatin1());
        }
        
        QJsonObject result;
        result["text"] = text;
        result["characters_sent"] = text.length();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "send_key") {
        // Send specific key to emulator with full AKEY support
        QString keyName = params["key"].toString(); // Don't convert to uppercase - preserve case!
        QJsonArray modifiers = params["modifiers"].toArray();
        
        int akeyCode = AKEY_NONE;
        
        // Handle special keys first (case-insensitive)
        QString upperKeyName = keyName.toUpper();
        if (upperKeyName == "RETURN" || upperKeyName == "ENTER") {
            akeyCode = AKEY_RETURN;
        } else if (upperKeyName == "SPACE") {
            akeyCode = AKEY_SPACE;
        } else if (upperKeyName == "TAB") {
            akeyCode = AKEY_TAB;
        } else if (upperKeyName == "ESC" || upperKeyName == "ESCAPE") {
            akeyCode = AKEY_ESCAPE;
        } else if (upperKeyName == "BACKSPACE") {
            akeyCode = AKEY_BACKSPACE;
        } else if (upperKeyName == "DELETE") {
            akeyCode = AKEY_DELETE_CHAR;
        } else if (upperKeyName == "UP") {
            akeyCode = AKEY_UP;
        } else if (upperKeyName == "DOWN") {
            akeyCode = AKEY_DOWN;
        } else if (upperKeyName == "LEFT") {
            akeyCode = AKEY_LEFT;
        } else if (upperKeyName == "RIGHT") {
            akeyCode = AKEY_RIGHT;
        } else if (upperKeyName == "HELP") {
            akeyCode = AKEY_HELP;
        } else if (upperKeyName == "ATARI") {
            akeyCode = AKEY_ATARI;
        } else if (keyName.length() == 1) {
            // Single character key - map to proper AKEY
            QChar ch = keyName.at(0);
            
            if (ch >= 'A' && ch <= 'Z') {
                // Uppercase letters - get base AKEY and add SHIFT
                char lowerCh = ch.toLatin1() + ('a' - 'A'); // Convert to lowercase
                
                // Map lowercase letter to AKEY value
                int akey_letters[] = {
                    AKEY_a, AKEY_b, AKEY_c, AKEY_d, AKEY_e, AKEY_f, AKEY_g, AKEY_h,
                    AKEY_i, AKEY_j, AKEY_k, AKEY_l, AKEY_m, AKEY_n, AKEY_o, AKEY_p,
                    AKEY_q, AKEY_r, AKEY_s, AKEY_t, AKEY_u, AKEY_v, AKEY_w, AKEY_x,
                    AKEY_y, AKEY_z
                };
                akeyCode = akey_letters[lowerCh - 'a'];
                // Uppercase is achieved with SHIFT modifier
                akeyCode |= AKEY_SHFT;
            } else if (ch >= 'a' && ch <= 'z') {
                // Lowercase letters - use lookup table
                int akey_letters[] = {
                    AKEY_a, AKEY_b, AKEY_c, AKEY_d, AKEY_e, AKEY_f, AKEY_g, AKEY_h,
                    AKEY_i, AKEY_j, AKEY_k, AKEY_l, AKEY_m, AKEY_n, AKEY_o, AKEY_p,
                    AKEY_q, AKEY_r, AKEY_s, AKEY_t, AKEY_u, AKEY_v, AKEY_w, AKEY_x,
                    AKEY_y, AKEY_z
                };
                akeyCode = akey_letters[ch.toLatin1() - 'a'];
            } else if (ch >= '0' && ch <= '9') {
                // Numbers
                int digit = ch.toLatin1() - '0';
                int akey_numbers[] = {AKEY_0, AKEY_1, AKEY_2, AKEY_3, AKEY_4, 
                                    AKEY_5, AKEY_6, AKEY_7, AKEY_8, AKEY_9};
                akeyCode = akey_numbers[digit];
            } else {
                sendResponse(client, requestId, false, QJsonValue(), 
                            "Unsupported character: " + keyName);
                return;
            }
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Unknown key name: " + keyName);
            return;
        }
        
        // Apply modifiers
        for (const QJsonValue& mod : modifiers) {
            QString modifier = mod.toString().toUpper();
            if (modifier == "CTRL") {
                akeyCode |= AKEY_CTRL;
            } else if (modifier == "SHIFT") {
                akeyCode |= AKEY_SHFT;
            } else if (modifier == "SHIFTCTRL" || modifier == "CTRLSHIFT") {
                akeyCode |= AKEY_SHFTCTRL;
            }
        }
        
        m_emulator->injectAKey(akeyCode);
        
        QJsonObject result;
        result["key"] = keyName;
        result["modifiers"] = modifiers;
        result["akey_code"] = akeyCode;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "console_key") {
        // Send Atari console keys (START, SELECT, OPTION, RESET)
        QString keyName = params["key"].toString().toUpper();
        
        int akeyCode = AKEY_NONE;
        if (keyName == "START") {
            akeyCode = AKEY_START;
        } else if (keyName == "SELECT") {
            akeyCode = AKEY_SELECT;
        } else if (keyName == "OPTION") {
            akeyCode = AKEY_OPTION;
        } else if (keyName == "RESET") {
            akeyCode = AKEY_COLDSTART; // Cold restart for RESET
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Unknown console key: " + keyName + ". Valid keys: START, SELECT, OPTION, RESET");
            return;
        }
        
        // Inject the special key code
        // Note: This may need special handling depending on how libatari800 processes these
        // For now, we'll use character injection and may need to enhance this
        m_emulator->injectCharacter(akeyCode);
        
        QJsonObject result;
        result["console_key"] = keyName;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "break") {
        // Send break signal (Ctrl+C equivalent)
        m_emulator->injectCharacter(AKEY_BREAK);
        
        QJsonObject result;
        result["break_sent"] = true;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "function_key") {
        // Send function keys F1-F4
        QString keyName = params["key"].toString().toUpper();
        
        char keyChar = 0;
        if (keyName == "F1") {
            keyChar = 0x01; // F1 character code
        } else if (keyName == "F2") {
            keyChar = 0x02; // F2 character code
        } else if (keyName == "F3") {
            keyChar = 0x03; // F3 character code
        } else if (keyName == "F4") {
            keyChar = 0x04; // F4 character code
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Unknown function key: " + keyName + ". Valid keys: F1, F2, F3, F4");
            return;
        }
        
        m_emulator->injectCharacter(keyChar);
        
        QJsonObject result;
        result["function_key"] = keyName;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "joystick") {
        // Control joystick input
        int player = params["player"].toInt(1); // Default to player 1
        bool fire = params["fire"].toBool(false);
        
        if (player < 1 || player > 2) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid player number. Must be 1 or 2");
            return;
        }
        
        // Handle direction - can be string name or numeric value
        int directionValue = -1;
        if (params.contains("direction")) {
            QString direction = params["direction"].toString().toUpper();
            
            // Map direction names to values (non-inverted Atari values)
            if (direction == "CENTER") directionValue = 15;
            else if (direction == "UP") directionValue = 14;
            else if (direction == "DOWN") directionValue = 13;
            else if (direction == "LEFT") directionValue = 11;
            else if (direction == "RIGHT") directionValue = 7;
            else if (direction == "UP_LEFT") directionValue = 10;
            else if (direction == "UP_RIGHT") directionValue = 6;
            else if (direction == "DOWN_LEFT") directionValue = 9;
            else if (direction == "DOWN_RIGHT") directionValue = 5;
            else {
                sendResponse(client, requestId, false, QJsonValue(), 
                            "Invalid direction. Use: CENTER, UP, DOWN, LEFT, RIGHT, UP_LEFT, UP_RIGHT, DOWN_LEFT, DOWN_RIGHT");
                return;
            }
        } else if (params.contains("value")) {
            // Accept numeric value directly (0-15)
            directionValue = params["value"].toInt(-1);
            if (directionValue < 0 || directionValue > 15) {
                sendResponse(client, requestId, false, QJsonValue(), 
                            "Invalid direction value. Must be 0-15");
                return;
            }
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Missing direction or value parameter");
            return;
        }
        
        // Set joystick state - AtariEmulator will handle the inversion
        m_emulator->setJoystickState(player, directionValue, fire);
        
        QJsonObject result;
        result["player"] = player;
        result["direction_value"] = directionValue;
        result["fire"] = fire;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "joystick_release") {
        // Release joystick to center position
        int player = params["player"].toInt(1);
        
        if (player < 1 || player > 2) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid player number. Must be 1 or 2");
            return;
        }
        
        m_emulator->releaseJoystick(player);
        
        QJsonObject result;
        result["player"] = player;
        result["released"] = true;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "get_joystick") {
        // Get specific joystick state
        int player = params["player"].toInt(1);
        
        if (player < 1 || player > 2) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid player number. Must be 1 or 2");
            return;
        }
        
        int state = m_emulator->getJoystickState(player);
        bool fire = m_emulator->getJoystickFire(player);
        
        // Convert inverted value back to direction name
        auto getDirectionInfo = [](int value) -> QPair<QString, int> {
            // Map inverted values to direction names and original values
            const int CENTER = 0x0f ^ 0xff;
            const int UP = 0x0e ^ 0xff;
            const int DOWN = 0x0d ^ 0xff;
            const int LEFT = 0x0b ^ 0xff;
            const int RIGHT = 0x07 ^ 0xff;
            const int UP_LEFT = 0x0a ^ 0xff;
            const int UP_RIGHT = 0x06 ^ 0xff;
            const int DOWN_LEFT = 0x09 ^ 0xff;
            const int DOWN_RIGHT = 0x05 ^ 0xff;
            
            if (value == CENTER) return qMakePair(QString("CENTER"), 15);
            else if (value == UP) return qMakePair(QString("UP"), 14);
            else if (value == DOWN) return qMakePair(QString("DOWN"), 13);
            else if (value == LEFT) return qMakePair(QString("LEFT"), 11);
            else if (value == RIGHT) return qMakePair(QString("RIGHT"), 7);
            else if (value == UP_LEFT) return qMakePair(QString("UP_LEFT"), 10);
            else if (value == UP_RIGHT) return qMakePair(QString("UP_RIGHT"), 6);
            else if (value == DOWN_LEFT) return qMakePair(QString("DOWN_LEFT"), 9);
            else if (value == DOWN_RIGHT) return qMakePair(QString("DOWN_RIGHT"), 5);
            else return qMakePair(QString("UNKNOWN"), -1);
        };
        
        auto dirInfo = getDirectionInfo(state);
        
        QJsonObject result;
        result["player"] = player;
        result["direction"] = dirInfo.first;
        result["direction_value"] = state;
        result["fire"] = fire;
        result["keyboard_enabled"] = (player == 1) ? m_emulator->isKbdJoy0Enabled() : m_emulator->isKbdJoy1Enabled();
        if (player == 1) {
            result["keyboard_keys"] = m_emulator->isJoysticksSwapped() ? "wasd" : "numpad";
        } else {
            result["keyboard_keys"] = m_emulator->isJoysticksSwapped() ? "numpad" : "wasd";
        }
        
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "get_all_joysticks") {
        // Get all joystick states
        QJsonObject result = m_emulator->getAllJoystickStates();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "start_joystick_stream") {
        // Subscribe to joystick state changes
        int rate = params["rate"].toInt(60);  // Default 60Hz
        rate = qBound(10, rate, 120);  // Limit to 10-120Hz
        
        if (!m_joystickStreamTimer) {
            m_joystickStreamTimer = new QTimer(this);
            connect(m_joystickStreamTimer, &QTimer::timeout, this, &TCPServer::streamJoystickStates);
        }
        
        m_joystickStreamClients.insert(client);
        
        if (!m_joystickStreamTimer->isActive()) {
            int interval = 1000 / rate;  // Convert Hz to milliseconds
            m_joystickStreamTimer->start(interval);
            
            // Initialize last known states
            m_lastJoy0State = m_emulator->getJoystickState(1);
            m_lastJoy1State = m_emulator->getJoystickState(2);
            m_lastTrig0 = m_emulator->getJoystickFire(1);
            m_lastTrig1 = m_emulator->getJoystickFire(2);
        }
        
        QJsonObject result;
        result["streaming"] = true;
        result["rate"] = rate;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "stop_joystick_stream") {
        // Unsubscribe from joystick state changes
        m_joystickStreamClients.remove(client);
        
        if (m_joystickStreamClients.isEmpty() && m_joystickStreamTimer) {
            m_joystickStreamTimer->stop();
        }
        
        QJsonObject result;
        result["streaming"] = false;
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "get_joystick_stream_status") {
        // Check if client is subscribed to joystick streaming
        bool isStreaming = m_joystickStreamClients.contains(client);
        
        QJsonObject result;
        result["streaming"] = isStreaming;
        result["total_streaming_clients"] = m_joystickStreamClients.size();
        if (m_joystickStreamTimer && m_joystickStreamTimer->isActive()) {
            result["stream_rate"] = 1000 / m_joystickStreamTimer->interval();
        }
        
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "caps_lock") {
        // Toggle caps lock or set specific state
        QString action = params["action"].toString().toLower();
        
        int akeyCode = AKEY_NONE;
        if (action == "toggle" || action.isEmpty()) {
            // Toggle caps lock state
            akeyCode = AKEY_CAPSTOGGLE;
        } else if (action == "on") {
            // Turn caps lock on
            akeyCode = AKEY_CAPSLOCK;
        } else if (action == "off") {
            // Turn caps lock off (send caps lock again if it's on)
            akeyCode = AKEY_CAPSLOCK;
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid caps lock action. Use 'toggle', 'on', or 'off'");
            return;
        }
        
        m_emulator->injectCharacter(akeyCode);
        
        QJsonObject result;
        result["caps_lock_action"] = action.isEmpty() ? "toggle" : action;
        result["akey_code"] = akeyCode;
        sendResponse(client, requestId, true, result);
        
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown input command: " + subCommand);
    }
}

void TCPServer::handleDebugCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand)
{
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    QJsonObject params = request["params"].toObject();
    
    if (!m_debugger) {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Debugger not available. Enable debugger window first.");
        return;
    }
    
    if (subCommand == "get_registers") {
        // Return current CPU register values
        QJsonObject registers;
        registers["pc"] = QString("$%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
        registers["a"] = QString("$%1").arg(CPU_regA, 2, 16, QChar('0')).toUpper();
        registers["x"] = QString("$%1").arg(CPU_regX, 2, 16, QChar('0')).toUpper();
        registers["y"] = QString("$%1").arg(CPU_regY, 2, 16, QChar('0')).toUpper();
        registers["s"] = QString("$%1").arg(CPU_regS, 2, 16, QChar('0')).toUpper();
        registers["p"] = QString("$%1").arg(CPU_regP, 2, 16, QChar('0')).toUpper();
        
        sendResponse(client, requestId, true, registers);
        
    } else if (subCommand == "read_memory") {
        // Read memory at specified address
        int address = params["address"].toInt();
        int length = params["length"].toInt(1);
        
        if (address < 0 || address > 65535) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid memory address. Must be 0-65535");
            return;
        }
        
        if (length < 1 || length > 256 || (address + length) > 65536) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid memory length or address range");
            return;
        }
        
        QJsonObject result;
        result["address"] = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        result["length"] = length;
        
        QJsonArray data;
        for (int i = 0; i < length; i++) {
            data.append(QString("$%1").arg(MEMORY_mem[address + i], 2, 16, QChar('0')).toUpper());
        }
        result["data"] = data;
        
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "write_memory") {
        // Write memory at specified address
        int address = params["address"].toInt();
        int value = params["value"].toInt();
        
        if (address < 0 || address > 65535) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid memory address. Must be 0-65535");
            return;
        }
        
        if (value < 0 || value > 255) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid memory value. Must be 0-255");
            return;
        }
        
        MEMORY_mem[address] = (unsigned char)value;
        
        QJsonObject result;
        result["address"] = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        result["value"] = QString("$%1").arg(value, 2, 16, QChar('0')).toUpper();
        result["written"] = true;
        
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "add_breakpoint") {
        // Add breakpoint at specified address
        int address = params["address"].toInt();
        
        if (address < 0 || address > 65535) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid breakpoint address. Must be 0-65535");
            return;
        }
        
        m_debugger->addBreakpoint((unsigned short)address);
        
        QJsonObject result;
        result["address"] = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        result["added"] = true;
        
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["address"] = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        sendEventToAllClients("breakpoint_added", eventData);
        
    } else if (subCommand == "remove_breakpoint") {
        // Remove breakpoint at specified address
        int address = params["address"].toInt();
        
        if (address < 0 || address > 65535) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid breakpoint address. Must be 0-65535");
            return;
        }
        
        m_debugger->removeBreakpoint((unsigned short)address);
        
        QJsonObject result;
        result["address"] = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        result["removed"] = true;
        
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["address"] = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        sendEventToAllClients("breakpoint_removed", eventData);
        
    } else if (subCommand == "list_breakpoints") {
        // List all current breakpoints
        QJsonArray breakpoints;
        
        // Note: This would require adding a method to DebuggerWidget to expose breakpoints
        // For now, we'll provide a basic response
        QJsonObject result;
        result["breakpoints"] = breakpoints;
        result["count"] = breakpoints.size();
        
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "clear_breakpoints") {
        // Clear all breakpoints
        m_debugger->clearAllBreakpoints();
        
        QJsonObject result;
        result["cleared"] = true;
        
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        sendEventToAllClients("breakpoints_cleared", eventData);
        
    } else if (subCommand == "pause") {
        // Pause emulation for debugging
        if (!m_emulator->isEmulationPaused()) {
            m_emulator->pauseEmulation();
            
            QJsonObject result;
            result["paused"] = true;
            result["pc"] = QString("$%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
            
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["paused"] = true;
            eventData["pc"] = QString("$%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
            sendEventToAllClients("debug_paused", eventData);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Emulation is already paused");
        }
        
    } else if (subCommand == "resume") {
        // Resume emulation from debugging
        if (m_emulator->isEmulationPaused()) {
            m_emulator->resumeEmulation();
            
            QJsonObject result;
            result["resumed"] = true;
            
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["resumed"] = true;
            sendEventToAllClients("debug_resumed", eventData);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Emulation is not paused");
        }
        
    } else if (subCommand == "step") {
        // Single step execution (step one frame)
        if (m_emulator->isEmulationPaused()) {
            m_emulator->stepOneFrame();
            
            QJsonObject result;
            result["stepped"] = true;
            result["pc"] = QString("$%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
            
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["stepped"] = true;
            eventData["pc"] = QString("$%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
            sendEventToAllClients("debug_stepped", eventData);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Emulation must be paused to step");
        }
        
    } else if (subCommand == "disassemble") {
        // Disassemble memory at specified address
        int address = params["address"].toInt(CPU_regPC);
        int lines = params["lines"].toInt(10);
        
        if (address < 0 || address > 65535) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid disassembly address. Must be 0-65535");
            return;
        }
        
        if (lines < 1 || lines > 100) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid line count. Must be 1-100");
            return;
        }
        
        QJsonObject result;
        result["address"] = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        result["lines"] = lines;
        
        // Use the proper 6502 disassembler
        QJsonArray disassembly;
        unsigned short currentAddr = (unsigned short)address;
        
        for (int i = 0; i < lines && currentAddr <= 65535; i++) {
            DisassembledInstruction inst = disassemble6502(MEMORY_mem, currentAddr);
            
            QJsonObject line;
            line["address"] = QString("$%1").arg(currentAddr, 4, 16, QChar('0')).toUpper();
            
            // Build hex bytes string
            QString hexBytes;
            for (int j = 0; j < inst.bytes && (currentAddr + j) <= 65535; j++) {
                hexBytes += QString("%1 ").arg(MEMORY_mem[currentAddr + j], 2, 16, QChar('0')).toUpper();
            }
            line["hex"] = hexBytes.trimmed();
            
            // Build instruction string
            QString instruction = inst.mnemonic;
            if (!inst.operand.isEmpty()) {
                instruction += " " + inst.operand;
            }
            line["instruction"] = instruction;
            
            disassembly.append(line);
            
            // Advance to next instruction
            currentAddr += inst.bytes;
            
            // Prevent infinite loop if we hit a bad instruction
            if (inst.bytes == 0) {
                currentAddr++;
            }
        }
        
        result["disassembly"] = disassembly;
        sendResponse(client, requestId, true, result);
        
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown debug command: " + subCommand);
    }
}

void TCPServer::handleConfigCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand)
{
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    QJsonObject params = request["params"].toObject();
    
    if (subCommand == "get_machine_type") {
        // Get current machine type
        QJsonObject result;
        result["machine_type"] = m_emulator->getMachineType();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "set_machine_type") {
        // Set machine type (requires restart)
        QString machineType = params["machine_type"].toString();
        
        QStringList validTypes = {"-400", "-800", "-xl", "-xe", "-xegs", "-5200"};
        if (!validTypes.contains(machineType)) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid machine type. Valid types: " + validTypes.join(", "));
            return;
        }
        
        m_emulator->setMachineType(machineType);
        
        QJsonObject result;
        result["machine_type"] = machineType;
        result["restart_required"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["machine_type"] = machineType;
        eventData["restart_required"] = true;
        sendEventToAllClients("machine_type_changed", eventData);
        
    } else if (subCommand == "get_video_system") {
        // Get current video system
        QJsonObject result;
        result["video_system"] = m_emulator->getVideoSystem();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "set_video_system") {
        // Set video system (requires restart)
        QString videoSystem = params["video_system"].toString();
        
        QStringList validSystems = {"-ntsc", "-pal"};
        if (!validSystems.contains(videoSystem)) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid video system. Valid systems: " + validSystems.join(", "));
            return;
        }
        
        m_emulator->setVideoSystem(videoSystem);
        
        QJsonObject result;
        result["video_system"] = videoSystem;
        result["restart_required"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["video_system"] = videoSystem;
        eventData["restart_required"] = true;
        sendEventToAllClients("video_system_changed", eventData);
        
    } else if (subCommand == "get_basic_enabled") {
        // Get BASIC ROM status
        QJsonObject result;
        result["basic_enabled"] = m_emulator->isBasicEnabled();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "set_basic_enabled") {
        // Enable/disable BASIC ROM (requires restart)
        bool enabled = params["enabled"].toBool();
        
        m_emulator->setBasicEnabled(enabled);
        
        QJsonObject result;
        result["basic_enabled"] = enabled;
        result["restart_required"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["basic_enabled"] = enabled;
        eventData["restart_required"] = true;
        sendEventToAllClients("basic_setting_changed", eventData);
        
    } else if (subCommand == "get_joystick_config") {
        // Get joystick configuration
        QJsonObject result;
        result["kbd_joy0_enabled"] = m_emulator->isKbdJoy0Enabled();
        result["kbd_joy1_enabled"] = m_emulator->isKbdJoy1Enabled();
        result["joysticks_swapped"] = m_emulator->isJoysticksSwapped();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "set_joystick_config") {
        // Set joystick configuration
        bool kbdJoy0 = params["kbd_joy0_enabled"].toBool();
        bool kbdJoy1 = params["kbd_joy1_enabled"].toBool();
        bool swapped = params["joysticks_swapped"].toBool();
        
        m_emulator->setKbdJoy0Enabled(kbdJoy0);
        m_emulator->setKbdJoy1Enabled(kbdJoy1);
        m_emulator->setJoysticksSwapped(swapped);
        
        QJsonObject result;
        result["kbd_joy0_enabled"] = kbdJoy0;
        result["kbd_joy1_enabled"] = kbdJoy1;
        result["joysticks_swapped"] = swapped;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["kbd_joy0_enabled"] = kbdJoy0; 
        eventData["kbd_joy1_enabled"] = kbdJoy1;
        eventData["joysticks_swapped"] = swapped;
        sendEventToAllClients("joystick_config_changed", eventData);
        
    } else if (subCommand == "get_audio_config") {
        // Get audio configuration
        QJsonObject result;
        result["audio_enabled"] = m_emulator->isAudioEnabled();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "set_audio_enabled") {
        // Enable/disable audio
        bool enabled = params["enabled"].toBool();
        
        m_emulator->enableAudio(enabled);
        
        QJsonObject result;
        result["audio_enabled"] = enabled;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["audio_enabled"] = enabled;
        sendEventToAllClients("audio_config_changed", eventData);
        
    } else if (subCommand == "set_volume") {
        // Set audio volume
        float volume = params["volume"].toDouble(1.0f);
        
        if (volume < 0.0f || volume > 1.0f) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Invalid volume level. Must be between 0.0 and 1.0");
            return;
        }
        
        m_emulator->setVolume(volume);
        
        QJsonObject result;
        result["volume"] = volume;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["volume"] = volume;
        sendEventToAllClients("volume_changed", eventData);
        
    } else if (subCommand == "get_sio_patch") {
        // Get SIO patch status (fast disk access)
        QJsonObject result;
        result["sio_patch_enabled"] = m_emulator->getSIOPatchEnabled();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "set_sio_patch") {
        // Enable/disable SIO patch
        bool enabled = params["enabled"].toBool();
        
        bool success = m_emulator->setSIOPatchEnabled(enabled);
        
        if (success) {
            QJsonObject result;
            result["sio_patch_enabled"] = enabled;
            sendResponse(client, requestId, true, result);
            
            // Send event to all clients
            QJsonObject eventData;
            eventData["sio_patch_enabled"] = enabled;
            sendEventToAllClients("sio_patch_changed", eventData);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Failed to change SIO patch setting");
        }
        
    } else if (subCommand == "get_rom_paths") {
        // Get ROM file paths
        QJsonObject result;
        result["os_rom_path"] = m_emulator->getOSRomPath();
        result["basic_rom_path"] = m_emulator->getBasicRomPath();
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "set_rom_paths") {
        // Set ROM file paths (requires restart)
        QString osRomPath = params["os_rom_path"].toString();
        QString basicRomPath = params["basic_rom_path"].toString();
        
        if (!osRomPath.isEmpty()) {
            QString validatedOSPath = validateAndNormalizePath(osRomPath);
            if (validatedOSPath.isEmpty()) {
                sendResponse(client, requestId, false, QJsonValue(), 
                            "OS ROM file not found: " + osRomPath);
                return;
            }
            m_emulator->setOSRomPath(validatedOSPath);
        }
        
        if (!basicRomPath.isEmpty()) {
            QString validatedBasicPath = validateAndNormalizePath(basicRomPath);
            if (validatedBasicPath.isEmpty()) {
                sendResponse(client, requestId, false, QJsonValue(), 
                            "BASIC ROM file not found: " + basicRomPath);
                return;
            }
            m_emulator->setBasicRomPath(validatedBasicPath);
        }
        
        QJsonObject result;
        result["os_rom_path"] = m_emulator->getOSRomPath();
        result["basic_rom_path"] = m_emulator->getBasicRomPath();
        result["restart_required"] = true;
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["os_rom_path"] = m_emulator->getOSRomPath();
        eventData["basic_rom_path"] = m_emulator->getBasicRomPath();
        eventData["restart_required"] = true;
        sendEventToAllClients("rom_paths_changed", eventData);
        
    } else if (subCommand == "apply_restart") {
        // Apply configuration changes that require restart
        // Use the proper restart method that reinitializes with new settings
        if (m_mainWindow) {
            m_mainWindow->requestEmulatorRestart();
        } else {
            // Fallback if MainWindow is not available
            m_emulator->coldRestart();
        }
        
        QJsonObject result;
        result["restarted"] = true;
        result["reason"] = "configuration_changes";
        sendResponse(client, requestId, true, result);
        
        // Send event to all clients
        QJsonObject eventData;
        eventData["reason"] = "configuration_changes";
        sendEventToAllClients("emulator_restarted", eventData);
        
    } else if (subCommand == "get_profiles") {
        // Get list of available configuration profiles
        if (m_mainWindow && m_mainWindow->getProfileManager()) {
            QJsonObject result;
            QJsonArray profileArray;
            
            QStringList profiles = m_mainWindow->getProfileManager()->getProfileNames();
            for (const QString& profile : profiles) {
                profileArray.append(profile);
            }
            
            result["profiles"] = profileArray;
            result["current_profile"] = m_mainWindow->getProfileManager()->getCurrentProfileName();
            sendResponse(client, requestId, true, result);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Profile manager not available");
        }
        
    } else if (subCommand == "get_current_profile") {
        // Get current profile name
        if (m_mainWindow && m_mainWindow->getProfileManager()) {
            QJsonObject result;
            result["current_profile"] = m_mainWindow->getProfileManager()->getCurrentProfileName();
            sendResponse(client, requestId, true, result);
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Profile manager not available");
        }
        
    } else if (subCommand == "load_profile") {
        // Load a configuration profile
        QString profileName = params["profile_name"].toString();
        
        if (profileName.isEmpty()) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Profile name is required");
            return;
        }
        
        if (m_mainWindow && m_mainWindow->getProfileManager()) {
            ConfigurationProfile profile = m_mainWindow->getProfileManager()->loadProfile(profileName);
            if (profile.isValid()) {
                // Apply profile through MainWindow
                m_mainWindow->applyProfileViaTCP(profile);
                m_mainWindow->getProfileManager()->setCurrentProfileName(profileName);
                
                QJsonObject result;
                result["profile_loaded"] = profileName;
                result["restart_required"] = true;
                sendResponse(client, requestId, true, result);
                
                // Send event to all clients
                QJsonObject eventData;
                eventData["profile_name"] = profileName;
                sendEventToAllClients("profile_loaded", eventData);
            } else {
                sendResponse(client, requestId, false, QJsonValue(), 
                            "Failed to load profile: " + profileName);
            }
        } else {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Profile manager not available");
        }
        
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown config command: " + subCommand);
    }
}

void TCPServer::handleStatusCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand)
{
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    
    if (subCommand == "get_state") {
        // Return basic emulator state information
        QJsonObject state;
        state["running"] = !m_emulator->isEmulationPaused();
        state["connected_clients"] = m_clients.count();
        state["server_port"] = m_port;
        
        // Add basic CPU state if available
        state["pc"] = QString("$%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
        state["a"] = QString("$%1").arg(CPU_regA, 2, 16, QChar('0')).toUpper();
        
        sendResponse(client, requestId, true, state);
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown status command: " + subCommand);
    }
}

void TCPServer::handleScreenCommand(QTcpSocket* client, const QJsonObject& request, const QString& subCommand)
{
    QJsonValue requestId = request.contains("id") ? request["id"] : QJsonValue();
    QJsonObject params = request["params"].toObject();
    
    if (subCommand == "capture") {
        // Save screenshot to file
        QString filename = params["filename"].toString();
        bool interlaced = params["interlaced"].toBool(false);
        
        if (filename.isEmpty()) {
            // Generate default filename if not provided
            filename = QString("screenshot_%1.pcx").arg(QDateTime::currentMSecsSinceEpoch());
        }
        
        // Ensure PCX extension since PNG might not be compiled in
        if (!filename.endsWith(".pcx", Qt::CaseInsensitive)) {
            // Change extension to .pcx
            int lastDot = filename.lastIndexOf('.');
            if (lastDot != -1) {
                filename = filename.left(lastDot) + ".pcx";
            } else {
                filename += ".pcx";
            }
            qDebug() << "TCP Server: Changed filename extension to PCX:" << filename;
        }
        
        // Use absolute path in current directory
        QFileInfo fileInfo(filename);
        if (fileInfo.isRelative()) {
            filename = QDir::currentPath() + "/" + filename;
        }
        
        // Call atari800 screen capture function with error handling
        try {
            qDebug() << "TCP Server: Attempting to save screenshot to:" << filename;
            
#ifdef SCREENSHOTS
            bool success = Screen_SaveScreenshot(filename.toUtf8().constData(), interlaced ? 1 : 0);
            
            if (success) {
                QJsonObject result;
                result["filename"] = filename;
                result["interlaced"] = interlaced;
                result["timestamp"] = QDateTime::currentMSecsSinceEpoch();
                result["size_bytes"] = QFileInfo(filename).size();
                sendResponse(client, requestId, true, result);
                qDebug() << "TCP Server: Screenshot saved successfully";
            } else {
                sendResponse(client, requestId, false, QJsonValue(), 
                            "Screen_SaveScreenshot failed for: " + filename);
                qDebug() << "TCP Server: Screen_SaveScreenshot returned false";
            }
#else
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Screenshot support not compiled in (SCREENSHOTS not defined)");
            qDebug() << "TCP Server: Screenshot support not available - SCREENSHOTS not defined";
#endif
        } catch (const std::exception& e) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        QString("Exception during screenshot: %1").arg(e.what()));
            qDebug() << "TCP Server: Exception during screenshot:" << e.what();
        } catch (...) {
            sendResponse(client, requestId, false, QJsonValue(), 
                        "Unknown exception during screenshot");
            qDebug() << "TCP Server: Unknown exception during screenshot";
        }
        
    } else if (subCommand == "get_buffer") {
        // Return raw screen buffer as base64
        // This requires access to Screen_atari buffer
        QJsonObject result;
        result["width"] = 320;  // Standard Atari screen width
        result["height"] = 240; // Standard Atari screen height
        result["format"] = "ARGB32";
        result["buffer_available"] = false; // Will implement this next
        result["message"] = "Raw buffer access not yet implemented";
        sendResponse(client, requestId, true, result);
        
    } else if (subCommand == "get_text") {
        // Extract text from current screen (for text mode)
        QJsonObject result;
        result["text"] = ""; // Will implement text extraction
        result["mode"] = "unknown";
        result["message"] = "Text extraction not yet implemented";
        sendResponse(client, requestId, true, result);
        
    } else {
        sendResponse(client, requestId, false, QJsonValue(), 
                    "Unknown screen command: " + subCommand);
    }
}

void TCPServer::streamJoystickStates()
{
    if (m_joystickStreamClients.isEmpty() || !m_emulator) {
        return;
    }
    
    // Get current joystick states
    int joy0State = m_emulator->getJoystickState(1);
    int joy1State = m_emulator->getJoystickState(2);
    bool trig0 = m_emulator->getJoystickFire(1);
    bool trig1 = m_emulator->getJoystickFire(2);
    
    // Check for changes in joystick 1
    if (joy0State != m_lastJoy0State || trig0 != m_lastTrig0) {
        // Convert inverted value back to direction name
        auto getDirectionInfo = [](int value) -> QPair<QString, int> {
            const int CENTER = 0x0f ^ 0xff;
            const int UP = 0x0e ^ 0xff;
            const int DOWN = 0x0d ^ 0xff;
            const int LEFT = 0x0b ^ 0xff;
            const int RIGHT = 0x07 ^ 0xff;
            const int UP_LEFT = 0x0a ^ 0xff;
            const int UP_RIGHT = 0x06 ^ 0xff;
            const int DOWN_LEFT = 0x09 ^ 0xff;
            const int DOWN_RIGHT = 0x05 ^ 0xff;
            
            if (value == CENTER) return qMakePair(QString("CENTER"), 15);
            else if (value == UP) return qMakePair(QString("UP"), 14);
            else if (value == DOWN) return qMakePair(QString("DOWN"), 13);
            else if (value == LEFT) return qMakePair(QString("LEFT"), 11);
            else if (value == RIGHT) return qMakePair(QString("RIGHT"), 7);
            else if (value == UP_LEFT) return qMakePair(QString("UP_LEFT"), 10);
            else if (value == UP_RIGHT) return qMakePair(QString("UP_RIGHT"), 6);
            else if (value == DOWN_LEFT) return qMakePair(QString("DOWN_LEFT"), 9);
            else if (value == DOWN_RIGHT) return qMakePair(QString("DOWN_RIGHT"), 5);
            else return qMakePair(QString("UNKNOWN"), -1);
        };
        
        auto dirInfo = getDirectionInfo(joy0State);
        auto prevDirInfo = getDirectionInfo(m_lastJoy0State);
        
        QJsonObject data;
        data["player"] = 1;
        data["direction"] = dirInfo.first;
        data["direction_value"] = joy0State;
        data["fire"] = trig0;
        data["previous_direction"] = prevDirInfo.first;
        data["previous_fire"] = m_lastTrig0;
        data["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        
        // Send to all subscribed clients
        for (QTcpSocket* client : m_joystickStreamClients) {
            sendEvent(client, "joystick_changed", data);
        }
        
        m_lastJoy0State = joy0State;
        m_lastTrig0 = trig0;
    }
    
    // Check for changes in joystick 2
    if (joy1State != m_lastJoy1State || trig1 != m_lastTrig1) {
        auto getDirectionInfo = [](int value) -> QPair<QString, int> {
            const int CENTER = 0x0f ^ 0xff;
            const int UP = 0x0e ^ 0xff;
            const int DOWN = 0x0d ^ 0xff;
            const int LEFT = 0x0b ^ 0xff;
            const int RIGHT = 0x07 ^ 0xff;
            const int UP_LEFT = 0x0a ^ 0xff;
            const int UP_RIGHT = 0x06 ^ 0xff;
            const int DOWN_LEFT = 0x09 ^ 0xff;
            const int DOWN_RIGHT = 0x05 ^ 0xff;
            
            if (value == CENTER) return qMakePair(QString("CENTER"), 15);
            else if (value == UP) return qMakePair(QString("UP"), 14);
            else if (value == DOWN) return qMakePair(QString("DOWN"), 13);
            else if (value == LEFT) return qMakePair(QString("LEFT"), 11);
            else if (value == RIGHT) return qMakePair(QString("RIGHT"), 7);
            else if (value == UP_LEFT) return qMakePair(QString("UP_LEFT"), 10);
            else if (value == UP_RIGHT) return qMakePair(QString("UP_RIGHT"), 6);
            else if (value == DOWN_LEFT) return qMakePair(QString("DOWN_LEFT"), 9);
            else if (value == DOWN_RIGHT) return qMakePair(QString("DOWN_RIGHT"), 5);
            else return qMakePair(QString("UNKNOWN"), -1);
        };
        
        auto dirInfo = getDirectionInfo(joy1State);
        auto prevDirInfo = getDirectionInfo(m_lastJoy1State);
        
        QJsonObject data;
        data["player"] = 2;
        data["direction"] = dirInfo.first;
        data["direction_value"] = joy1State;
        data["fire"] = trig1;
        data["previous_direction"] = prevDirInfo.first;
        data["previous_fire"] = m_lastTrig1;
        data["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        
        // Send to all subscribed clients
        for (QTcpSocket* client : m_joystickStreamClients) {
            sendEvent(client, "joystick_changed", data);
        }
        
        m_lastJoy1State = joy1State;
        m_lastTrig1 = trig1;
    }
}