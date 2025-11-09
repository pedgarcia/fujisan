/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "fujinetbinarymanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QRegularExpression>

const QString FujiNetBinaryManager::DEFAULT_RELEASE_URL = "https://api.github.com/repos/FujiNetWIFI/fujinet-firmware/releases";
const QString FujiNetBinaryManager::FUJINET_TARGET = "APPLE";

FujiNetBinaryManager::FujiNetBinaryManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_releaseUrl(DEFAULT_RELEASE_URL)
    , m_downloadFile(nullptr)
{
}

FujiNetBinaryManager::~FujiNetBinaryManager()
{
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }
}

QString FujiNetBinaryManager::getCurrentPlatform()
{
#ifdef Q_OS_WIN
    // Windows is not supported
    return QString();
#elif defined(Q_OS_MAC)
    // Detect macOS architecture
    #ifdef Q_PROCESSOR_ARM_64
        return "macos-arm64";
    #else
        return "macos-x86_64";
    #endif
#elif defined(Q_OS_LINUX)
    // Linux x86_64
    return "linux-amd64";
#else
    return QString();
#endif
}

bool FujiNetBinaryManager::isPlatformSupported()
{
    return !getCurrentPlatform().isEmpty();
}

QString FujiNetBinaryManager::getStoragePath()
{
#ifdef Q_OS_MAC
    // macOS: Store in app bundle Resources
    return QCoreApplication::applicationDirPath() + "/../Resources/fujinet-pc";
#else
    // Linux: Store in user data directory
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataPath + "/fujinet-pc";
#endif
}

QString FujiNetBinaryManager::getBinaryPath() const
{
    QString storagePath = getStoragePath();
    QString binaryName = "fujinet";

#ifdef Q_OS_WIN
    binaryName += ".exe";
#endif

    return storagePath + "/" + binaryName;
}

bool FujiNetBinaryManager::isBinaryInstalled() const
{
    return QFile::exists(getBinaryPath());
}

QString FujiNetBinaryManager::getInstalledVersion() const
{
    if (!isBinaryInstalled()) {
        return QString();
    }

    // Try to load saved version info first
    QString savedVersion = loadBinaryInfo();
    if (!savedVersion.isEmpty()) {
        return savedVersion;
    }

    // Fall back to querying binary with -V flag
    return parseVersionFromBinary(getBinaryPath());
}

QString FujiNetBinaryManager::getDefaultReleaseUrl()
{
    return DEFAULT_RELEASE_URL;
}

void FujiNetBinaryManager::checkForUpdates(const QString& releaseUrl)
{
    if (!isPlatformSupported()) {
        emit downloadFailed("Platform not supported");
        return;
    }

    if (!releaseUrl.isEmpty()) {
        m_releaseUrl = releaseUrl;
    }

    qDebug() << "Checking for FujiNet-PC updates at:" << m_releaseUrl;

    QNetworkRequest request(m_releaseUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FujiNetBinaryManager::onUpdateCheckReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetBinaryManager::onNetworkError);
}

void FujiNetBinaryManager::downloadBinary(const QString& version, const QString& downloadUrl)
{
    if (!isPlatformSupported()) {
        emit downloadFailed("Platform not supported");
        return;
    }

    qDebug() << "Downloading FujiNet-PC" << version << "from:" << downloadUrl;

    m_downloadVersion = version;

    // Create storage directory
    QString storagePath = getStoragePath();
    QDir dir;
    if (!dir.mkpath(storagePath)) {
        emit downloadFailed("Failed to create storage directory: " + storagePath);
        return;
    }

    // Prepare download file
    QString downloadPath = storagePath + "/download." + (downloadUrl.endsWith(".zip") ? "zip" : "tar.gz");
    m_downloadFile = new QFile(downloadPath);

    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit downloadFailed("Failed to open file for writing: " + downloadPath);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    // Start download
    QNetworkRequest request(downloadUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Fujisan");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &FujiNetBinaryManager::onDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, &FujiNetBinaryManager::onDownloadFinished);
    connect(reply, &QNetworkReply::readyRead, [this, reply]() {
        if (m_downloadFile) {
            m_downloadFile->write(reply->readAll());
        }
    });
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FujiNetBinaryManager::onNetworkError);
}

// Private slots

void FujiNetBinaryManager::onUpdateCheckReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit downloadFailed("Failed to check for updates: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        emit downloadFailed("Invalid response from GitHub API");
        return;
    }

    QJsonArray releases = doc.array();
    if (releases.isEmpty()) {
        emit downloadFailed("No releases found");
        return;
    }

    // Find latest release (first in array is usually latest)
    QString latestVersion;
    QString downloadUrl;
    QString platform = getCurrentPlatform();

    for (const QJsonValue& releaseValue : releases) {
        QJsonObject release = releaseValue.toObject();
        QString tagName = release["tag_name"].toString();
        bool isDraft = release["draft"].toBool();
        bool isPrerelease = release["prerelease"].toBool();

        // Skip drafts and prereleases
        if (isDraft || isPrerelease) {
            continue;
        }

        // Look for appropriate asset
        QJsonArray assets = release["assets"].toArray();
        for (const QJsonValue& assetValue : assets) {
            QJsonObject asset = assetValue.toObject();
            QString assetName = asset["name"].toString();

            // Check if this asset matches our platform and target
            QString expectedPattern = QString("fujinet-pc-%1_%2").arg(FUJINET_TARGET).arg(platform);
            if (assetName.contains(expectedPattern)) {
                latestVersion = tagName;
                downloadUrl = asset["browser_download_url"].toString();
                break;
            }
        }

        if (!latestVersion.isEmpty()) {
            break;  // Found a matching release
        }
    }

    if (latestVersion.isEmpty()) {
        emit downloadFailed("No compatible binary found for platform: " + platform);
        return;
    }

    // Check if update is available
    QString installedVersion = getInstalledVersion();
    bool updateAvailable = installedVersion.isEmpty() || (installedVersion != latestVersion);

    qDebug() << "Installed version:" << installedVersion << "Latest version:" << latestVersion;
    emit updateCheckComplete(latestVersion, downloadUrl, updateAvailable);
}

void FujiNetBinaryManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void FujiNetBinaryManager::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (m_downloadFile) {
        m_downloadFile->close();
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString error = "Download failed: " + reply->errorString();
        qWarning() << error;
        if (m_downloadFile) {
            m_downloadFile->remove();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        emit downloadFailed(error);
        reply->deleteLater();
        return;
    }

    QString archivePath = m_downloadFile->fileName();
    delete m_downloadFile;
    m_downloadFile = nullptr;

    qDebug() << "Download complete, extracting:" << archivePath;
    emit extractionProgress(0);

    // Extract archive
    QString storagePath = getStoragePath();
    if (!extractArchive(archivePath, storagePath)) {
        emit downloadFailed("Failed to extract archive");
        QFile::remove(archivePath);
        reply->deleteLater();
        return;
    }

    emit extractionProgress(100);

    // Clean up archive
    QFile::remove(archivePath);

    // Verify binary exists
    QString binaryPath = getBinaryPath();
    if (!QFile::exists(binaryPath)) {
        emit downloadFailed("Binary not found after extraction: " + binaryPath);
        reply->deleteLater();
        return;
    }

    // Make binary executable on Unix-like systems
#ifndef _WIN32
    QFile::setPermissions(binaryPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                      QFile::ReadGroup | QFile::ExeGroup |
                                      QFile::ReadOther | QFile::ExeOther);
#endif

    // Save version info
    saveBinaryInfo(m_downloadVersion);

    qDebug() << "FujiNet-PC installation complete:" << binaryPath;
    emit downloadComplete(binaryPath);

    reply->deleteLater();
}

void FujiNetBinaryManager::onNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString errorMsg = reply->errorString();
    qWarning() << "Network error:" << errorMsg;
    emit downloadFailed(errorMsg);
}

// Private helper methods

QString FujiNetBinaryManager::getExpectedAssetName(const QString& version) const
{
    QString platform = getCurrentPlatform();
    QString extension = platform.startsWith("macos") || platform.startsWith("linux") ? ".tar.gz" : ".zip";
    return QString("fujinet-pc-%1_%2_%3%4").arg(FUJINET_TARGET, version, platform, extension);
}

bool FujiNetBinaryManager::extractArchive(const QString& archivePath, const QString& destDir)
{
    if (archivePath.endsWith(".tar.gz")) {
        return extractTarGz(archivePath, destDir);
    } else if (archivePath.endsWith(".zip")) {
        return extractZip(archivePath, destDir);
    }
    return false;
}

bool FujiNetBinaryManager::extractTarGz(const QString& archivePath, const QString& destDir)
{
    // Use tar command to extract
    QProcess process;
    process.start("tar", QStringList() << "-xzf" << archivePath << "-C" << destDir);

    if (!process.waitForFinished(30000)) {  // 30 second timeout
        qWarning() << "Tar extraction timed out";
        return false;
    }

    if (process.exitCode() != 0) {
        qWarning() << "Tar extraction failed:" << process.readAllStandardError();
        return false;
    }

    return true;
}

bool FujiNetBinaryManager::extractZip(const QString& archivePath, const QString& destDir)
{
    // Use unzip command to extract
    QProcess process;
    process.start("unzip", QStringList() << "-o" << archivePath << "-d" << destDir);

    if (!process.waitForFinished(30000)) {  // 30 second timeout
        qWarning() << "Unzip extraction timed out";
        return false;
    }

    if (process.exitCode() != 0) {
        qWarning() << "Unzip extraction failed:" << process.readAllStandardError();
        return false;
    }

    return true;
}

QString FujiNetBinaryManager::parseVersionFromBinary(const QString& binaryPath) const
{
    QProcess process;
    process.start(binaryPath, QStringList() << "-V");

    if (!process.waitForFinished(5000)) {
        return QString();
    }

    QString output = process.readAllStandardOutput();
    // Parse version from output (format may vary)
    // Expected format: "fujinet-pc version X.Y.Z"
    QRegularExpression versionRegex("version\\s+(\\S+)");
    QRegularExpressionMatch match = versionRegex.match(output);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return QString();
}

void FujiNetBinaryManager::saveBinaryInfo(const QString& version)
{
    QString infoPath = getStoragePath() + "/version.txt";
    QFile file(infoPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(version.toUtf8());
        file.close();
    }
}

QString FujiNetBinaryManager::loadBinaryInfo() const
{
    QString infoPath = getStoragePath() + "/version.txt";
    QFile file(infoPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString version = file.readAll().trimmed();
        file.close();
        return version;
    }
    return QString();
}
