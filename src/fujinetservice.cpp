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

FujiNetService::FujiNetService(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_serverUrl("http://localhost:8000")
    , m_isConnected(false)
    , m_healthCheckTimer(new QTimer(this))
    , m_drivePollingTimer(new QTimer(this))
{
    connect(m_healthCheckTimer, &QTimer::timeout, this, &FujiNetService::checkConnection);
    connect(m_drivePollingTimer, &QTimer::timeout, this, &FujiNetService::queryDriveStatus);
}

FujiNetService::~FujiNetService()
{
    stopHealthCheck();
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
