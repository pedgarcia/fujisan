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

FujiNetService::FujiNetService(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_serverUrl("http://localhost:8000")
    , m_isConnected(false)
    , m_healthCheckTimer(new QTimer(this))
{
    connect(m_healthCheckTimer, &QTimer::timeout, this, &FujiNetService::checkConnection);
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
    QMap<QString, QString> params;
    params["hostslot"] = QString::number(hostSlot);
    params["deviceslot"] = QString::number(deviceSlot);
    params["filename"] = filename;
    params["mode"] = readOnly ? "1" : "2";

    QUrl url = buildUrl("/mount", params);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    m_pendingMounts[reply] = deviceSlot;

    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onMountReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetService::onNetworkError);
}

void FujiNetService::unmount(int deviceSlot)
{
    QMap<QString, QString> params;
    params["deviceslot"] = QString::number(deviceSlot);

    QUrl url = buildUrl("/unmount", params);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

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
    // Use the /slot endpoint to query all mount status
    QUrl url(m_serverUrl + "/slot");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FujiNetService::onMountStatusReply);
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
