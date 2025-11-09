/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef FUJINETSERVICE_H
#define FUJINETSERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <functional>

// Structure to hold disk slot information
struct FujiNetDiskSlot {
    int slotNumber;          // 0-7 for D1-D8
    QString filename;        // Mounted image filename
    int hostSlot;           // Which host slot (0-7, 0xFF=empty)
    QString hostname;        // Host name
    int accessMode;         // 1=Read-only, 2=Read-Write
    bool isEmpty;           // True if slot is empty

    FujiNetDiskSlot()
        : slotNumber(-1), hostSlot(0xFF), accessMode(1), isEmpty(true) {}
};

// Structure to hold host slot information
struct FujiNetHostSlot {
    int slotNumber;         // 0-7
    QString hostname;       // Host name

    FujiNetHostSlot()
        : slotNumber(-1) {}
};

class FujiNetService : public QObject
{
    Q_OBJECT

public:
    explicit FujiNetService(QObject *parent = nullptr);
    ~FujiNetService();

    // Connection management
    void setServerUrl(const QString& url);  // e.g. "http://localhost:8000"
    QString getServerUrl() const { return m_serverUrl; }
    bool isConnected() const { return m_isConnected; }

    // Connection health check
    void checkConnection();
    void startHealthCheck(int intervalMs = 5000);
    void stopHealthCheck();
    void abortAllRequests();  // Abort all pending network requests

    // Mount operations
    void mount(int deviceSlot, int hostSlot, const QString& filename, bool readOnly);
    void unmount(int deviceSlot);
    void mountAll();  // Mount all pre-configured images

    // Host configuration
    void getHosts();
    void setHost(int hostSlot, const QString& hostname);

    // Status queries
    void queryMountStatus();  // Query all disk slots D1-D8

    // Browse files on host
    void browseHost(int hostSlot);

signals:
    // Connection signals
    void connected();
    void disconnected();
    void connectionError(const QString& error);

    // Mount operation signals
    void mountSuccess(int deviceSlot);
    void mountFailed(int deviceSlot, const QString& error);
    void unmountSuccess(int deviceSlot);
    void unmountFailed(int deviceSlot, const QString& error);

    // Status update signals
    void mountStatusUpdated(const QVector<FujiNetDiskSlot>& diskSlots);
    void hostsUpdated(const QVector<FujiNetHostSlot>& hosts);

    // File browse signals
    void fileListReceived(int hostSlot, const QStringList& files);

private slots:
    void onHealthCheckReply();
    void onMountReply();
    void onUnmountReply();
    void onHostsReply();
    void onSetHostReply();
    void onMountStatusReply();
    void onBrowseReply();
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    // Helper methods
    void sendRequest(const QString& endpoint, std::function<void()> onSuccess);
    QUrl buildUrl(const QString& endpoint, const QMap<QString, QString>& params = QMap<QString, QString>());
    void parseMountStatus(const QString& html);
    QStringList parseHosts(const QString& response);

    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl;
    bool m_isConnected;
    QTimer* m_healthCheckTimer;

    // Track pending operations for reply handling
    QMap<QNetworkReply*, int> m_pendingMounts;      // reply -> deviceSlot
    QMap<QNetworkReply*, int> m_pendingUnmounts;    // reply -> deviceSlot
    QMap<QNetworkReply*, int> m_pendingBrowse;      // reply -> hostSlot
};

#endif // FUJINETSERVICE_H
