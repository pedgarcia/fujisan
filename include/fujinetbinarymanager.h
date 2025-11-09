/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef FUJINETBINARYMANAGER_H
#define FUJINETBINARYMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QFile>

class FujiNetBinaryManager : public QObject
{
    Q_OBJECT

public:
    explicit FujiNetBinaryManager(QObject *parent = nullptr);
    ~FujiNetBinaryManager();

    // Platform detection
    static QString getCurrentPlatform();  // Returns: "macos-arm64", "macos-x86_64", "linux-amd64", or empty on Windows
    static bool isPlatformSupported();     // Returns false on Windows

    // Binary management
    QString getBinaryPath() const;         // Get path to installed binary
    bool isBinaryInstalled() const;        // Check if binary is installed
    QString getInstalledVersion() const;   // Get version of installed binary

    // Download management
    void checkForUpdates(const QString& releaseUrl = QString());
    void downloadBinary(const QString& version, const QString& downloadUrl);
    void setReleaseUrl(const QString& url) { m_releaseUrl = url; }
    QString getReleaseUrl() const { return m_releaseUrl; }

    // Storage paths
    static QString getStoragePath();       // Get directory where FujiNet-PC is stored
    static QString getDefaultReleaseUrl(); // Default GitHub releases URL

    // Version detection (public for manual binary configuration)
    QString parseVersionFromBinary(const QString& binaryPath) const;

signals:
    void updateCheckComplete(const QString& latestVersion, const QString& downloadUrl, bool updateAvailable);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadComplete(const QString& binaryPath);
    void downloadFailed(const QString& error);
    void extractionProgress(int percentage);

private slots:
    void onUpdateCheckReply();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    // Helper methods
    QString getExpectedAssetName(const QString& version) const;
    bool extractArchive(const QString& archivePath, const QString& destDir);
    bool extractTarGz(const QString& archivePath, const QString& destDir);
    bool extractZip(const QString& archivePath, const QString& destDir);
    void saveBinaryInfo(const QString& version);
    QString loadBinaryInfo() const;

    QNetworkAccessManager* m_networkManager;
    QString m_releaseUrl;
    QFile* m_downloadFile;
    QString m_downloadVersion;

    static const QString DEFAULT_RELEASE_URL;
    static const QString FUJINET_TARGET;  // "ATARI" for Atari version
};

#endif // FUJINETBINARYMANAGER_H
