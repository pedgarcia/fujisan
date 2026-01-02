/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "fujinetservice.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

FujiNetService::FujiNetService(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_serverUrl("http://localhost:8000")
    , m_isConnected(false)
    , m_healthCheckTimer(new QTimer(this))
    , m_drivePollingTimer(new QTimer(this))
    , m_printerPollTimer(nullptr)
    , m_printerEnabled(false)
    , m_currentPrinterType("Atari 825")
    , m_sseConnection(nullptr)
{
    connect(m_healthCheckTimer, &QTimer::timeout, this, &FujiNetService::checkConnection);
    connect(m_drivePollingTimer, &QTimer::timeout, this, &FujiNetService::queryDriveStatus);
}

FujiNetService::~FujiNetService()
{
    stopHealthCheck();
    disconnectFromPrinterEvents();
}

void FujiNetService::setServerUrl(const QString& url)
{
    m_serverUrl = url;
    if (!m_serverUrl.startsWith("http://") && !m_serverUrl.startsWith("https://")) {
        m_serverUrl = "http://" + m_serverUrl;
    }
    // Remove trailing slash
    if (m_serverUrl.endsWith('/')) {
        m_serverUrl.chop(1);
    }
}

void FujiNetService::checkConnection()
{
    QUrl url(m_serverUrl + "/test");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onHealthCheckReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::startHealthCheck(int intervalMs)
{
    m_healthCheckTimer->start(intervalMs);
    checkConnection();  // Check immediately
}

void FujiNetService::stopHealthCheck()
{
    m_healthCheckTimer->stop();
    abortAllRequests();
}

void FujiNetService::startDrivePolling(int intervalMs)
{
    m_drivePollingTimer->start(intervalMs);
    queryDriveStatus();  // Query immediately
}

void FujiNetService::stopDrivePolling()
{
    m_drivePollingTimer->stop();
}

void FujiNetService::abortAllRequests()
{
    // Abort all pending mount requests
    for (auto reply : m_pendingMounts.keys()) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_pendingMounts.clear();

    // Abort all pending unmount requests
    for (auto reply : m_pendingUnmounts.keys()) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_pendingUnmounts.clear();

    // Abort all pending browse requests
    for (auto reply : m_pendingBrowse.keys()) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_pendingBrowse.clear();

    qDebug() << "All pending FujiNet requests aborted";
}

void FujiNetService::mount(int deviceSlot, int hostSlot, const QString& filename, bool readOnly)
{
    // Use stock FujiNet-PC browse API: GET /browse/host/{H}/path?action=newmount&slot={D}&mode=r|w
    // Note: Browse API uses 1-indexed host and slot numbers
    int hostNum = hostSlot + 1;  // Convert 0-indexed to 1-indexed
    int slotNum = deviceSlot + 1;  // Convert 0-indexed to 1-indexed

    // Build the browse URL path: /browse/host/N/filename
    QString urlPath = QString("/browse/host/%1%2").arg(hostNum).arg(filename);

    // Build query parameters
    QMap<QString, QString> params;
    params["action"] = "newmount";
    params["slot"] = QString::number(slotNum);
    params["mode"] = readOnly ? "r" : "w";

    QUrl url = buildUrl(urlPath, params);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    qDebug() << "FujiNet mount request (browse API):" << url.toString();

    QNetworkReply* reply = m_networkManager->get(request);
    m_pendingMounts[reply] = deviceSlot;

    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onMountReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::unmount(int deviceSlot)
{
    // Use stock FujiNet-PC browse API: GET /browse/host/1/?action=eject&slot={D}
    // Note: Browse API uses 1-indexed slot numbers
    // Host doesn't matter for eject, but we need a valid host path
    int slotNum = deviceSlot + 1;  // Convert 0-indexed to 1-indexed

    // Build query parameters
    QMap<QString, QString> params;
    params["action"] = "eject";
    params["slot"] = QString::number(slotNum);

    // Use host 1 (SD card) as the base path for eject
    QUrl url = buildUrl("/browse/host/1/", params);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    qDebug() << "FujiNet unmount request (browse API):" << url.toString();

    QNetworkReply* reply = m_networkManager->get(request);
    m_pendingUnmounts[reply] = deviceSlot;

    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onUnmountReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::mountAll()
{
    QMap<QString, QString> params;
    params["mountall"] = "1";

    QUrl url = buildUrl("/mount", params);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onMountReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

QString FujiNetService::copyToSD(const QString& localPath, const QString& sdFolderPath)
{
    // Copy local disk image file to FujiNet SD folder
    QFileInfo localFile(localPath);

    if (!localFile.exists()) {
        QString error = QString("Source file does not exist: %1").arg(localPath);
        qDebug() << "FujiNet copy failed:" << error;
        emit copyFailed(error);
        return QString();
    }

    // Ensure SD folder exists
    QDir sdDir(sdFolderPath);
    if (!sdDir.exists()) {
        QString error = QString("SD folder does not exist: %1").arg(sdFolderPath);
        qDebug() << "FujiNet copy failed:" << error;
        emit copyFailed(error);
        return QString();
    }

    // Determine destination filename
    QString baseFilename = localFile.fileName();
    QString destPath = sdDir.filePath(baseFilename);

    // Check if file is already in the SD folder (same canonical path)
    QString srcCanonical = localFile.canonicalFilePath();
    QString destCanonical = QFileInfo(destPath).canonicalFilePath();

    // If canonical paths match, or source is already in SD folder, skip copy
    if (srcCanonical == destCanonical || localFile.canonicalPath() == sdDir.canonicalPath()) {
        qDebug() << "File is already in SD folder, skipping copy:" << localPath;
        QString sdRelativePath = "/" + baseFilename;
        emit copySuccess(sdRelativePath);
        return sdRelativePath;
    }

    // If file already exists at destination, remove it (overwrite behavior)
    if (QFile::exists(destPath)) {
        qDebug() << "File already exists, removing old version:" << destPath;
        if (!QFile::remove(destPath)) {
            QString error = QString("Failed to remove existing file: %1").arg(destPath);
            qDebug() << "FujiNet copy failed:" << error;
            emit copyFailed(error);
            return QString();
        }
    }

    // Get file size for progress tracking
    qint64 fileSize = localFile.size();
    QString filename = QFileInfo(destPath).fileName();

    // Emit initial progress
    emit copyProgress(0, filename);

    // Copy the file
    qDebug() << "Copying" << localPath << "to" << destPath;

    bool success = QFile::copy(localPath, destPath);

    if (success) {
        // Emit completion
        emit copyProgress(100, filename);

        // Return SD-relative path (with leading /)
        QString sdRelativePath = "/" + filename;
        qDebug() << "Copy successful, SD path:" << sdRelativePath;
        emit copySuccess(sdRelativePath);
        return sdRelativePath;
    } else {
        QString error = QString("Failed to copy file to SD folder: %1").arg(destPath);
        qDebug() << "FujiNet copy failed:" << error;
        emit copyFailed(error);
        return QString();
    }
}

void FujiNetService::getHosts()
{
    QUrl url(m_serverUrl + "/hosts");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onHostsReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::setHost(int hostSlot, const QString& hostname)
{
    QMap<QString, QString> params;
    params["hostslot"] = QString::number(hostSlot);
    params["hostname"] = hostname;

    QUrl url = buildUrl("/hosts", params);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onSetHostReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::queryMountStatus()
{
    // Use the /slot endpoint to query all mount status (legacy endpoint)
    QUrl url(m_serverUrl + "/slot");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onMountStatusReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::queryDriveStatus()
{
    // Use the root / endpoint to query drive status (new format)
    QUrl url(m_serverUrl + "/");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onDriveStatusReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::browseHost(int hostSlot)
{
    QString endpoint = QString("/browse/%1").arg(hostSlot);
    QUrl url(m_serverUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    m_pendingBrowse[reply] = hostSlot;

    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onBrowseReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

// Private slots

void FujiNetService::onHealthCheckReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    bool wasConnected = m_isConnected;

    if (reply->error() == QNetworkReply::NoError) {
        QString response = reply->readAll();
        // Check for valid response (should be JSON with "result": 1)
        if (response.contains("\"result\"") && response.contains("1")) {
            m_isConnected = true;
            if (!wasConnected) {
                qDebug() << "FujiNet-PC connected at" << m_serverUrl;
                emit connected();
            }
        } else {
            m_isConnected = false;
            if (wasConnected) {
                emit disconnected();
            }
        }
    } else {
        m_isConnected = false;
        if (wasConnected) {
            qDebug() << "FujiNet-PC disconnected:" << reply->errorString();
            emit disconnected();
        }
    }

    reply->deleteLater();
}

void FujiNetService::onMountReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int deviceSlot = m_pendingMounts.value(reply, -1);
    m_pendingMounts.remove(reply);

    if (reply->error() == QNetworkReply::NoError) {
        if (deviceSlot != -1) {
            emit mountSuccess(deviceSlot);
        }
        // After successful mount, query status to update UI
        queryMountStatus();
    } else {
        if (deviceSlot != -1) {
            emit mountFailed(deviceSlot, reply->errorString());
        }
    }

    reply->deleteLater();
}

void FujiNetService::onUnmountReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int deviceSlot = m_pendingUnmounts.value(reply, -1);
    m_pendingUnmounts.remove(reply);

    if (reply->error() == QNetworkReply::NoError) {
        if (deviceSlot != -1) {
            emit unmountSuccess(deviceSlot);
        }
        // After successful unmount, query status to update UI
        queryMountStatus();
    } else {
        if (deviceSlot != -1) {
            emit unmountFailed(deviceSlot, reply->errorString());
        }
    }

    reply->deleteLater();
}

void FujiNetService::onHostsReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QString response = reply->readAll();
        QStringList hostnames = parseHosts(response);

        QVector<FujiNetHostSlot> hosts;
        for (int i = 0; i < hostnames.size() && i < 8; i++) {
            FujiNetHostSlot host;
            host.slotNumber = i;
            host.hostname = hostnames[i];
            hosts.append(host);
        }

        emit hostsUpdated(hosts);
    }

    reply->deleteLater();
}

void FujiNetService::onSetHostReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        // Refresh hosts list after setting
        getHosts();
    }

    reply->deleteLater();
}

void FujiNetService::onMountStatusReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QString html = reply->readAll();
        parseMountStatus(html);
    }

    reply->deleteLater();
}

void FujiNetService::onDriveStatusReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QString html = reply->readAll();
        parseDriveStatus(html);
    } else {
        qDebug() << "Drive status query failed:" << reply->errorString();
    }

    reply->deleteLater();
}

void FujiNetService::onBrowseReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int hostSlot = m_pendingBrowse.value(reply, -1);
    m_pendingBrowse.remove(reply);

    if (reply->error() == QNetworkReply::NoError && hostSlot != -1) {
        QString html = reply->readAll();

        // Parse HTML for file list (basic parsing, may need refinement)
        QStringList files;
        QRegularExpression fileRegex("<a[^>]*>([^<]+)</a>");
        QRegularExpressionMatchIterator i = fileRegex.globalMatch(html);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            QString filename = match.captured(1);
            if (!filename.isEmpty() && filename != ".." && filename != ".") {
                files.append(filename);
            }
        }

        emit fileListReceived(hostSlot, files);
    }

    reply->deleteLater();
}

void FujiNetService::onNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString errorMsg = reply->errorString();
    qDebug() << "FujiNet network error:" << errorMsg;
    emit connectionError(errorMsg);
}

// Helper methods

QUrl FujiNetService::buildUrl(const QString& endpoint, const QMap<QString, QString>& params)
{
    QUrl url(m_serverUrl + endpoint);

    if (!params.isEmpty()) {
        QUrlQuery query;
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            query.addQueryItem(it.key(), it.value());
        }
        url.setQuery(query);
    }

    return url;
}

QStringList FujiNetService::parseHosts(const QString& response)
{
    // Hosts are returned as newline-separated list
    return response.split('\n', Qt::SkipEmptyParts);
}

void FujiNetService::parseMountStatus(const QString& html)
{
    // Parse the HTML returned by /slot endpoint
    // This is a simplified parser - may need refinement based on actual HTML structure

    QVector<FujiNetDiskSlot> diskSlots;

    // Look for patterns in the HTML that indicate slot information
    // Based on the code research, the HTML contains information about each slot
    QRegularExpression slotRegex("Slot (\\d+).*?host_slot=(\\d+|0xFF).*?filename=([^&\"]+).*?mode=(\\d)",
                                 QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatchIterator i = slotRegex.globalMatch(html);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        FujiNetDiskSlot slot;
        slot.slotNumber = match.captured(1).toInt();

        QString hostSlotStr = match.captured(2);
        if (hostSlotStr == "0xFF") {
            slot.hostSlot = 0xFF;
            slot.isEmpty = true;
        } else {
            slot.hostSlot = hostSlotStr.toInt();
            slot.isEmpty = false;
        }

        slot.filename = match.captured(3);
        slot.accessMode = match.captured(4).toInt();

        diskSlots.append(slot);
    }

    // If no slots parsed, create empty slots for D1-D8
    if (diskSlots.isEmpty()) {
        for (int i = 0; i < 8; i++) {
            FujiNetDiskSlot slot;
            slot.slotNumber = i;
            slot.isEmpty = true;
            diskSlots.append(slot);
        }
    }

    emit mountStatusUpdated(diskSlots);
}

void FujiNetService::parseDriveStatus(const QString& html)
{
    // Parse the HTML returned by / (root) endpoint
    // Format: <div data-mountslot="N" data-mount="/filename (R/W)" or data-mount="(Empty)">

    QVector<FujiNetDrive> drives;

    // Parse data-mount attributes for each drive slot (0-7)
    // Example: data-mountslot="1" data-mount="/ANIMALS.XEX (R)"
    QRegularExpression slotRegex("data-mountslot=\"(\\d+)\"\\s+data-mountslotname=\"[^\"]*\"\\s+data-mount=\"([^\"]*)\"",
                                 QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator i = slotRegex.globalMatch(html);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();

        FujiNetDrive drive;
        drive.slotNumber = match.captured(1).toInt();
        QString mountStr = match.captured(2);  // e.g., "(Empty)" or "/ANIMALS.XEX (R)" or "/DISK.ATR (W)"

        if (mountStr == "(Empty)" || mountStr.isEmpty()) {
            drive.isEmpty = true;
            drive.filename = QString();
            drive.isReadOnly = true;
        } else {
            drive.isEmpty = false;

            // Parse the mount string: "/filename (R)" or "/filename (W)"
            QRegularExpression mountRegex("^(.+?)\\s+\\((R|W)\\)$");
            QRegularExpressionMatch mountMatch = mountRegex.match(mountStr);

            if (mountMatch.hasMatch()) {
                drive.filename = mountMatch.captured(1);  // e.g., "/ANIMALS.XEX"
                QString mode = mountMatch.captured(2);
                drive.isReadOnly = (mode == "R");
            } else {
                // Fallback: just use the whole string as filename
                drive.filename = mountStr;
                drive.isReadOnly = true;
            }
        }

        drives.append(drive);
    }

    // Ensure we have all 8 drives (D1-D8)
    // Sort by slot number and fill in missing slots
    QMap<int, FujiNetDrive> driveMap;
    for (const auto& drive : drives) {
        driveMap[drive.slotNumber] = drive;
    }

    QVector<FujiNetDrive> completeDrives;
    for (int i = 0; i < 8; i++) {
        if (driveMap.contains(i)) {
            completeDrives.append(driveMap[i]);
        } else {
            // Create empty drive for missing slots
            FujiNetDrive emptyDrive;
            emptyDrive.slotNumber = i;
            emptyDrive.isEmpty = true;
            completeDrives.append(emptyDrive);
        }
    }

    // Drive status parsed - emit signal (debug logging removed to reduce noise)

    emit driveStatusUpdated(completeDrives);
}

// Printer operations

void FujiNetService::configurePrinter(const QString& printerType, bool enabled)
{
    if (!m_networkManager) return;

    QUrl url(m_serverUrl + "/config");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                     "application/x-www-form-urlencoded");

    // Map Fujisan type to FujiNet API string
    QString apiPrinterType = mapPrinterTypeToAPI(printerType);

    QString postData = QString("printermodel1=%1&printer_enabled=%2")
        .arg(QString(QUrl::toPercentEncoding(apiPrinterType)))
        .arg(enabled ? "1" : "0");

    qDebug() << "Configuring printer:" << apiPrinterType << "enabled:" << enabled;

    QNetworkReply* reply = m_networkManager->post(request, postData.toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply, enabled]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Printer configured successfully";

            if (enabled) {
                stopPrinterPolling();
                connectToPrinterEvents();
            } else {
                disconnectFromPrinterEvents();
                stopPrinterPolling();
            }
        } else {
            qDebug() << "Printer config failed:" << reply->errorString();
            emit printerError("Failed to configure printer: " + reply->errorString());
        }
        reply->deleteLater();
    });

    m_printerEnabled = enabled;
    m_currentPrinterType = printerType;
}

void FujiNetService::checkPrinterStatus()
{
    if (!m_networkManager || !m_printerEnabled) return;

    QUrl url(m_serverUrl + "/printer/status");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();

            bool hasOutput = obj["has_output"].toBool();
            bool ready = obj["ready"].toBool();

            if (hasOutput && ready) {
                getPrinterOutput();
            }
        }
        reply->deleteLater();
    });
}

void FujiNetService::getPrinterOutput()
{
    if (!m_networkManager || !m_printerEnabled) return;

    QUrl url(m_serverUrl + "/print");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();

            if (data.isEmpty()) {
                reply->deleteLater();
                return;
            }

            QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
            emit printerOutputReceived(data, contentType);
        }
        else if (reply->errorString().contains("busy", Qt::CaseInsensitive)) {
            emit printerBusy();
        }
        else {
            qDebug() << "Printer output error:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void FujiNetService::startPrinterPolling()
{
    if (!m_printerPollTimer) {
        m_printerPollTimer = new QTimer(this);
        connect(m_printerPollTimer, &QTimer::timeout, this, &FujiNetService::checkPrinterStatus);
    }

    qDebug() << "Starting printer polling every" << PRINTER_POLL_INTERVAL_MS << "ms";
    m_printerPollTimer->start(PRINTER_POLL_INTERVAL_MS);

    checkPrinterStatus();
}

void FujiNetService::stopPrinterPolling()
{
    if (m_printerPollTimer) {
        qDebug() << "Stopping printer polling";
        m_printerPollTimer->stop();
    }
}

void FujiNetService::clearPrinterBuffer()
{
    if (!m_networkManager) return;

    QUrl url(m_serverUrl + "/printer/clear");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentLengthHeader, "0");

    QNetworkReply* reply = m_networkManager->post(request, QByteArray());

    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Printer buffer cleared successfully";
        } else {
            qDebug() << "Failed to clear printer buffer:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void FujiNetService::connectToPrinterEvents()
{
    if (!m_networkManager) {
        qDebug() << "Cannot connect to printer events: no network manager";
        return;
    }

    if (m_sseConnection) {
        qDebug() << "SSE connection already active";
        return;
    }

    QUrl url(m_serverUrl + "/printer/events");
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Connection", "keep-alive");

    qDebug() << "Connecting to printer SSE:" << url.toString();

    m_sseConnection = m_networkManager->get(request);
    m_sseBuffer.clear();

    connect(m_sseConnection, &QIODevice::readyRead, this, [this]() {
        if (!m_sseConnection) return;

        QByteArray data = m_sseConnection->readAll();
        m_sseBuffer.append(data);
        parseSSEData(m_sseBuffer);
    });

    connect(m_sseConnection, &QNetworkReply::finished, this, [this]() {
        qDebug() << "SSE connection closed, reconnecting in 2s";
        m_sseConnection = nullptr;

        if (m_printerEnabled) {
            QTimer::singleShot(2000, this, &FujiNetService::connectToPrinterEvents);
        }
    });

    connect(m_sseConnection, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, [this](QNetworkReply::NetworkError error) {
        qDebug() << "SSE connection error:" << error << "- falling back to polling";

        if (error != QNetworkReply::OperationCanceledError) {
            disconnectFromPrinterEvents();
            startPrinterPolling();
        }
    });
}

void FujiNetService::disconnectFromPrinterEvents()
{
    if (m_sseConnection) {
        qDebug() << "Disconnecting from printer SSE events";
        m_sseConnection->abort();
        m_sseConnection->deleteLater();
        m_sseConnection = nullptr;
        m_sseBuffer.clear();
    }
}

void FujiNetService::parseSSEData(QByteArray& buffer)
{
    // SSE messages end with double newline: either \n\n or \r\n\r\n
    QString delimiter = "\n\n";
    if (!buffer.contains(delimiter.toLatin1())) {
        delimiter = "\r\n\r\n";
    }

    while (buffer.contains(delimiter.toLatin1())) {
        int endPos = buffer.indexOf(delimiter.toLatin1());
        QByteArray message = buffer.left(endPos);
        buffer.remove(0, endPos + delimiter.length());

        if (message.startsWith("data: ")) {
            QByteArray jsonData = message.mid(6).trimmed();

            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            if (doc.isNull()) {
                qDebug() << "Failed to parse SSE JSON:" << jsonData;
                continue;
            }

            QJsonObject obj = doc.object();
            QString event = obj["event"].toString();

            if (event == "printer_ready") {
                getPrinterOutput();
            } else if (event == "printer_cleared") {
                emit printerBufferCleared();
            }
        }
    }
}

QString FujiNetService::mapPrinterTypeToAPI(const QString& type)
{
    // Map from display name to API string
    static QMap<QString, QString> typeMap = {
        {"file printer (TRIM)", "file printer (TRIM)"},
        {"file printer (ASCII)", "file printer (ASCII)"},
        {"Atari 820", "Atari 820"},
        {"Atari 822", "Atari 822"},
        {"Atari 825", "Atari 825"},
        {"Atari 1020", "Atari 1020"},
        {"Atari 1025", "Atari 1025"},
        {"Atari 1027", "Atari 1027"},
        {"Atari 1029", "Atari 1029"},
        {"Atari XMM801", "Atari XMM801"},
        {"Atari XDM121", "Atari XDM121"},
        {"Epson 80", "Epson 80"},
        {"Epson PrintShop", "Epson PrintShop"},
        {"Okimate 10", "Okimate 10"},
        {"GRANTIC", "GRANTIC"},
        {"HTML printer", "HTML printer"},
        {"HTML ATASCII printer", "HTML ATASCII printer"}
    };

    return typeMap.value(type, "Atari 825");  // Default
}
