/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "configurationprofile.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>

const QString ProfileStorage::PROFILE_EXTENSION = ".profile";
const QString ProfileStorage::PROFILE_SUBDIR = "profiles";

// ConfigurationProfile serialization methods
QJsonObject ConfigurationProfile::toJson() const {
    QJsonObject json;
    
    // Profile metadata
    QJsonObject profileInfo;
    profileInfo["name"] = name;
    profileInfo["description"] = description;
    profileInfo["created"] = created.toString(Qt::ISODate);
    profileInfo["lastUsed"] = lastUsed.toString(Qt::ISODate);
    profileInfo["version"] = version;
    json["profileInfo"] = profileInfo;
    
    // Machine Configuration
    QJsonObject machineConfig;
    machineConfig["machineType"] = machineType;
    machineConfig["videoSystem"] = videoSystem;
    machineConfig["basicEnabled"] = basicEnabled;
    machineConfig["altirraOSEnabled"] = altirraOSEnabled;
    machineConfig["osRomPath"] = osRomPath;
    machineConfig["basicRomPath"] = basicRomPath;
    json["machineConfig"] = machineConfig;
    
    // Memory Configuration
    QJsonObject memoryConfig;
    memoryConfig["enable800Ram"] = enable800Ram;
    memoryConfig["mosaicSize"] = mosaicSize;
    memoryConfig["axlonSize"] = axlonSize;
    memoryConfig["axlonShadow"] = axlonShadow;
    memoryConfig["enableMapRam"] = enableMapRam;
    json["memoryConfig"] = memoryConfig;
    
    // Performance
    QJsonObject performanceConfig;
    performanceConfig["turboMode"] = turboMode;
    performanceConfig["emulationSpeedIndex"] = emulationSpeedIndex;
    json["performanceConfig"] = performanceConfig;
    
    // Audio Configuration
    QJsonObject audioConfig;
    audioConfig["enabled"] = audioEnabled;
    audioConfig["frequency"] = audioFrequency;
    audioConfig["bits"] = audioBits;
    audioConfig["volume"] = audioVolume;
    audioConfig["bufferLength"] = audioBufferLength;
    audioConfig["latency"] = audioLatency;
    audioConfig["consoleSound"] = consoleSound;
    audioConfig["serialSound"] = serialSound;
    audioConfig["stereoPokey"] = stereoPokey;
    json["audioConfig"] = audioConfig;
    
    // Video Configuration
    QJsonObject videoConfig;
    videoConfig["artifactingMode"] = artifactingMode;
    videoConfig["showFPS"] = showFPS;
    videoConfig["scalingFilter"] = scalingFilter;
    videoConfig["keepAspectRatio"] = keepAspectRatio;
    videoConfig["fullscreenMode"] = fullscreenMode;
    json["videoConfig"] = videoConfig;
    
    // Color Settings
    QJsonObject colorConfig;
    QJsonObject palColors;
    palColors["saturation"] = palSaturation;
    palColors["contrast"] = palContrast;
    palColors["brightness"] = palBrightness;
    palColors["gamma"] = palGamma;
    palColors["tint"] = palTint;
    colorConfig["pal"] = palColors;
    
    QJsonObject ntscColors;
    ntscColors["saturation"] = ntscSaturation;
    ntscColors["contrast"] = ntscContrast;
    ntscColors["brightness"] = ntscBrightness;
    ntscColors["gamma"] = ntscGamma;
    ntscColors["tint"] = ntscTint;
    colorConfig["ntsc"] = ntscColors;
    json["colorConfig"] = colorConfig;
    
    // Input Configuration
    QJsonObject inputConfig;
    inputConfig["joystickEnabled"] = joystickEnabled;
    inputConfig["joystick0Hat"] = joystick0Hat;
    inputConfig["joystick1Hat"] = joystick1Hat;
    inputConfig["joystick2Hat"] = joystick2Hat;
    inputConfig["joystick3Hat"] = joystick3Hat;
    inputConfig["joyDistinct"] = joyDistinct;
    inputConfig["kbdJoy0Enabled"] = kbdJoy0Enabled;
    inputConfig["kbdJoy1Enabled"] = kbdJoy1Enabled;
    inputConfig["swapJoysticks"] = swapJoysticks;
    inputConfig["grabMouse"] = grabMouse;
    inputConfig["mouseDevice"] = mouseDevice;
    inputConfig["keyboardToggle"] = keyboardToggle;
    inputConfig["keyboardLeds"] = keyboardLeds;
    json["inputConfig"] = inputConfig;
    
    // Media Configuration
    QJsonObject mediaConfig;
    
    // Cartridges
    QJsonObject cartridgeConfig;
    QJsonObject primaryCart;
    primaryCart["enabled"] = primaryCartridge.enabled;
    primaryCart["path"] = primaryCartridge.path;
    primaryCart["type"] = primaryCartridge.type;
    cartridgeConfig["primary"] = primaryCart;
    
    QJsonObject piggybackCart;
    piggybackCart["enabled"] = piggybackCartridge.enabled;
    piggybackCart["path"] = piggybackCartridge.path;
    piggybackCart["type"] = piggybackCartridge.type;
    cartridgeConfig["piggyback"] = piggybackCart;
    
    cartridgeConfig["autoReboot"] = cartridgeAutoReboot;
    mediaConfig["cartridges"] = cartridgeConfig;
    
    // Disks
    QJsonArray diskArray;
    for (int i = 0; i < 8; i++) {
        QJsonObject disk;
        disk["enabled"] = disks[i].enabled;
        disk["path"] = disks[i].path;
        disk["readOnly"] = disks[i].readOnly;
        diskArray.append(disk);
    }
    mediaConfig["disks"] = diskArray;
    
    // Cassette
    QJsonObject cassetteConfig;
    cassetteConfig["enabled"] = cassette.enabled;
    cassetteConfig["path"] = cassette.path;
    cassetteConfig["readOnly"] = cassette.readOnly;
    cassetteConfig["bootTape"] = cassette.bootTape;
    mediaConfig["cassette"] = cassetteConfig;
    
    // Hard Drives
    QJsonArray hdArray;
    for (int i = 0; i < 4; i++) {
        QJsonObject hd;
        hd["enabled"] = hardDrives[i].enabled;
        hd["path"] = hardDrives[i].path;
        hdArray.append(hd);
    }
    mediaConfig["hardDrives"] = hdArray;
    mediaConfig["hdReadOnly"] = hdReadOnly;
    mediaConfig["hdDeviceName"] = hdDeviceName;
    
    // Special Devices
    mediaConfig["rDeviceName"] = rDeviceName;
    mediaConfig["netSIOEnabled"] = netSIOEnabled;
    mediaConfig["rtimeEnabled"] = rtimeEnabled;
    
    // Printer Configuration
    QJsonObject printerConfig;
    printerConfig["enabled"] = printer.enabled;
    printerConfig["outputFormat"] = printer.outputFormat;
    printerConfig["printerType"] = printer.printerType;
    mediaConfig["printer"] = printerConfig;
    
    json["mediaConfig"] = mediaConfig;
    
    // Hardware Extensions
    QJsonObject hardwareConfig;
    hardwareConfig["xep80Enabled"] = xep80Enabled;
    hardwareConfig["af80Enabled"] = af80Enabled;
    hardwareConfig["bit3Enabled"] = bit3Enabled;
    hardwareConfig["atari1400Enabled"] = atari1400Enabled;
    hardwareConfig["atari1450Enabled"] = atari1450Enabled;
    hardwareConfig["proto80Enabled"] = proto80Enabled;
    hardwareConfig["voiceboxEnabled"] = voiceboxEnabled;
    hardwareConfig["sioAcceleration"] = sioAcceleration;
    json["hardwareConfig"] = hardwareConfig;
    
    return json;
}

void ConfigurationProfile::fromJson(const QJsonObject& json) {
    // Profile metadata
    if (json.contains("profileInfo") && json["profileInfo"].isObject()) {
        QJsonObject profileInfo = json["profileInfo"].toObject();
        name = profileInfo["name"].toString();
        description = profileInfo["description"].toString();
        created = QDateTime::fromString(profileInfo["created"].toString(), Qt::ISODate);
        lastUsed = QDateTime::fromString(profileInfo["lastUsed"].toString(), Qt::ISODate);
        version = profileInfo["version"].toString("1.0");
    }
    
    // Machine Configuration
    if (json.contains("machineConfig") && json["machineConfig"].isObject()) {
        QJsonObject machineConfig = json["machineConfig"].toObject();
        machineType = machineConfig["machineType"].toString("-xl");
        videoSystem = machineConfig["videoSystem"].toString("-pal");
        basicEnabled = machineConfig["basicEnabled"].toBool(true);
        altirraOSEnabled = machineConfig["altirraOSEnabled"].toBool(false);
        osRomPath = machineConfig["osRomPath"].toString();
        basicRomPath = machineConfig["basicRomPath"].toString();
    }
    
    // Memory Configuration
    if (json.contains("memoryConfig") && json["memoryConfig"].isObject()) {
        QJsonObject memoryConfig = json["memoryConfig"].toObject();
        enable800Ram = memoryConfig["enable800Ram"].toBool(false);
        mosaicSize = memoryConfig["mosaicSize"].toInt(0);
        axlonSize = memoryConfig["axlonSize"].toInt(0);
        axlonShadow = memoryConfig["axlonShadow"].toBool(false);
        enableMapRam = memoryConfig["enableMapRam"].toBool(true);
    }
    
    // Performance
    if (json.contains("performanceConfig") && json["performanceConfig"].isObject()) {
        QJsonObject performanceConfig = json["performanceConfig"].toObject();
        turboMode = performanceConfig["turboMode"].toBool(false);
        emulationSpeedIndex = performanceConfig["emulationSpeedIndex"].toInt(1);
    }
    
    // Audio Configuration
    if (json.contains("audioConfig") && json["audioConfig"].isObject()) {
        QJsonObject audioConfig = json["audioConfig"].toObject();
        audioEnabled = audioConfig["enabled"].toBool(true);
        audioFrequency = audioConfig["frequency"].toInt(44100);
        audioBits = audioConfig["bits"].toInt(16);
        audioVolume = audioConfig["volume"].toInt(80);
        audioBufferLength = audioConfig["bufferLength"].toInt(100);
        audioLatency = audioConfig["latency"].toInt(20);
        consoleSound = audioConfig["consoleSound"].toBool(true);
        serialSound = audioConfig["serialSound"].toBool(false);
        stereoPokey = audioConfig["stereoPokey"].toBool(false);
    }
    
    // Video Configuration
    if (json.contains("videoConfig") && json["videoConfig"].isObject()) {
        QJsonObject videoConfig = json["videoConfig"].toObject();
        artifactingMode = videoConfig["artifactingMode"].toString("none");
        showFPS = videoConfig["showFPS"].toBool(false);
        scalingFilter = videoConfig["scalingFilter"].toBool(true);
        keepAspectRatio = videoConfig["keepAspectRatio"].toBool(true);
        fullscreenMode = videoConfig["fullscreenMode"].toBool(false);
    }
    
    // Color Settings
    if (json.contains("colorConfig") && json["colorConfig"].isObject()) {
        QJsonObject colorConfig = json["colorConfig"].toObject();
        
        if (colorConfig.contains("pal") && colorConfig["pal"].isObject()) {
            QJsonObject palColors = colorConfig["pal"].toObject();
            palSaturation = palColors["saturation"].toInt(0);
            palContrast = palColors["contrast"].toInt(0);
            palBrightness = palColors["brightness"].toInt(0);
            palGamma = palColors["gamma"].toInt(100);
            palTint = palColors["tint"].toInt(0);
        }
        
        if (colorConfig.contains("ntsc") && colorConfig["ntsc"].isObject()) {
            QJsonObject ntscColors = colorConfig["ntsc"].toObject();
            ntscSaturation = ntscColors["saturation"].toInt(0);
            ntscContrast = ntscColors["contrast"].toInt(0);
            ntscBrightness = ntscColors["brightness"].toInt(0);
            ntscGamma = ntscColors["gamma"].toInt(100);
            ntscTint = ntscColors["tint"].toInt(0);
        }
    }
    
    // Input Configuration
    if (json.contains("inputConfig") && json["inputConfig"].isObject()) {
        QJsonObject inputConfig = json["inputConfig"].toObject();
        joystickEnabled = inputConfig["joystickEnabled"].toBool(true);
        joystick0Hat = inputConfig["joystick0Hat"].toBool(false);
        joystick1Hat = inputConfig["joystick1Hat"].toBool(false);
        joystick2Hat = inputConfig["joystick2Hat"].toBool(false);
        joystick3Hat = inputConfig["joystick3Hat"].toBool(false);
        joyDistinct = inputConfig["joyDistinct"].toBool(false);
        kbdJoy0Enabled = inputConfig["kbdJoy0Enabled"].toBool(false);
        kbdJoy1Enabled = inputConfig["kbdJoy1Enabled"].toBool(false);
        swapJoysticks = inputConfig["swapJoysticks"].toBool(false);
        grabMouse = inputConfig["grabMouse"].toBool(false);
        mouseDevice = inputConfig["mouseDevice"].toString();
        keyboardToggle = inputConfig["keyboardToggle"].toBool(false);
        keyboardLeds = inputConfig["keyboardLeds"].toBool(false);
    }
    
    // Media Configuration
    if (json.contains("mediaConfig") && json["mediaConfig"].isObject()) {
        QJsonObject mediaConfig = json["mediaConfig"].toObject();
        
        // Cartridges
        if (mediaConfig.contains("cartridges") && mediaConfig["cartridges"].isObject()) {
            QJsonObject cartridgeConfig = mediaConfig["cartridges"].toObject();
            
            if (cartridgeConfig.contains("primary") && cartridgeConfig["primary"].isObject()) {
                QJsonObject primaryCart = cartridgeConfig["primary"].toObject();
                primaryCartridge.enabled = primaryCart["enabled"].toBool(false);
                primaryCartridge.path = primaryCart["path"].toString();
                primaryCartridge.type = primaryCart["type"].toInt(-1);
            }
            
            if (cartridgeConfig.contains("piggyback") && cartridgeConfig["piggyback"].isObject()) {
                QJsonObject piggybackCart = cartridgeConfig["piggyback"].toObject();
                piggybackCartridge.enabled = piggybackCart["enabled"].toBool(false);
                piggybackCartridge.path = piggybackCart["path"].toString();
                piggybackCartridge.type = piggybackCart["type"].toInt(-1);
            }
            
            cartridgeAutoReboot = cartridgeConfig["autoReboot"].toBool(true);
        }
        
        // Disks
        if (mediaConfig.contains("disks") && mediaConfig["disks"].isArray()) {
            QJsonArray diskArray = mediaConfig["disks"].toArray();
            for (int i = 0; i < qMin(8, diskArray.size()); i++) {
                QJsonObject disk = diskArray[i].toObject();
                disks[i].enabled = disk["enabled"].toBool(false);
                disks[i].path = disk["path"].toString();
                disks[i].readOnly = disk["readOnly"].toBool(false);
            }
        }
        
        // Cassette
        if (mediaConfig.contains("cassette") && mediaConfig["cassette"].isObject()) {
            QJsonObject cassetteConfig = mediaConfig["cassette"].toObject();
            cassette.enabled = cassetteConfig["enabled"].toBool(false);
            cassette.path = cassetteConfig["path"].toString();
            cassette.readOnly = cassetteConfig["readOnly"].toBool(false);
            cassette.bootTape = cassetteConfig["bootTape"].toBool(false);
        }
        
        // Hard Drives
        if (mediaConfig.contains("hardDrives") && mediaConfig["hardDrives"].isArray()) {
            QJsonArray hdArray = mediaConfig["hardDrives"].toArray();
            for (int i = 0; i < qMin(4, hdArray.size()); i++) {
                QJsonObject hd = hdArray[i].toObject();
                hardDrives[i].enabled = hd["enabled"].toBool(false);
                hardDrives[i].path = hd["path"].toString();
            }
        }
        
        hdReadOnly = mediaConfig["hdReadOnly"].toBool(false);
        hdDeviceName = mediaConfig["hdDeviceName"].toString("H");
        rDeviceName = mediaConfig["rDeviceName"].toString("R");
        netSIOEnabled = mediaConfig["netSIOEnabled"].toBool(false);
        rtimeEnabled = mediaConfig["rtimeEnabled"].toBool(false);
        
        // Printer Configuration
        if (mediaConfig.contains("printer") && mediaConfig["printer"].isObject()) {
            QJsonObject printerConfig = mediaConfig["printer"].toObject();
            printer.enabled = printerConfig["enabled"].toBool(false);
            printer.outputFormat = printerConfig["outputFormat"].toString("Text");
            printer.printerType = printerConfig["printerType"].toString("Generic");
        } else {
            // Default values for backward compatibility
            printer.enabled = false;
            printer.outputFormat = "Text";
            printer.printerType = "Generic";
        }
    }
    
    // Hardware Extensions
    if (json.contains("hardwareConfig") && json["hardwareConfig"].isObject()) {
        QJsonObject hardwareConfig = json["hardwareConfig"].toObject();
        xep80Enabled = hardwareConfig["xep80Enabled"].toBool(false);
        af80Enabled = hardwareConfig["af80Enabled"].toBool(false);
        bit3Enabled = hardwareConfig["bit3Enabled"].toBool(false);
        atari1400Enabled = hardwareConfig["atari1400Enabled"].toBool(false);
        atari1450Enabled = hardwareConfig["atari1450Enabled"].toBool(false);
        proto80Enabled = hardwareConfig["proto80Enabled"].toBool(false);
        voiceboxEnabled = hardwareConfig["voiceboxEnabled"].toBool(false);
        sioAcceleration = hardwareConfig["sioAcceleration"].toBool(true);
    }
}

bool ConfigurationProfile::isValid() const {
    return !name.isEmpty() && created.isValid();
}

QString ConfigurationProfile::getDisplayName() const {
    if (description.isEmpty()) {
        return name;
    }
    return QString("%1 - %2").arg(name, description);
}

void ConfigurationProfile::updateLastUsed() {
    lastUsed = QDateTime::currentDateTime();
}

// ProfileStorage implementation
QString ProfileStorage::getProfileDirectory() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dataDir).filePath(PROFILE_SUBDIR);
}

QString ProfileStorage::getProfileFilePath(const QString& name) {
    return QDir(getProfileDirectory()).filePath(name + PROFILE_EXTENSION);
}

bool ProfileStorage::ensureProfileDirectoryExists() {
    QDir dir(getProfileDirectory());
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

bool ProfileStorage::saveProfileToFile(const QString& name, const ConfigurationProfile& profile) {
    if (!isValidProfileName(name) || !profile.isValid()) {
        qWarning() << "Invalid profile name or profile data";
        return false;
    }
    
    if (!ensureProfileDirectoryExists()) {
        qWarning() << "Failed to create profile directory";
        return false;
    }
    
    QString filePath = getProfileFilePath(name);
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open profile file for writing:" << filePath;
        return false;
    }
    
    QJsonDocument doc(profile.toJson());
    file.write(doc.toJson());
    file.close();
    
    qDebug() << "Profile saved successfully:" << filePath;
    return true;
}

ConfigurationProfile ProfileStorage::loadProfileFromFile(const QString& name) {
    ConfigurationProfile profile;
    
    if (!isValidProfileName(name)) {
        qWarning() << "Invalid profile name:" << name;
        return profile;
    }
    
    QString filePath = getProfileFilePath(name);
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open profile file for reading:" << filePath;
        return profile;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse profile JSON:" << error.errorString();
        return profile;
    }
    
    profile.fromJson(doc.object());
    qDebug() << "Profile loaded successfully:" << filePath;
    return profile;
}

bool ProfileStorage::deleteProfileFile(const QString& name) {
    if (!isValidProfileName(name)) {
        return false;
    }
    
    QString filePath = getProfileFilePath(name);
    return QFile::remove(filePath);
}

bool ProfileStorage::profileFileExists(const QString& name) {
    if (!isValidProfileName(name)) {
        return false;
    }
    
    QString filePath = getProfileFilePath(name);
    return QFile::exists(filePath);
}

QStringList ProfileStorage::getAvailableProfiles() {
    QStringList profiles;
    
    QDir dir(getProfileDirectory());
    if (!dir.exists()) {
        return profiles;
    }
    
    QStringList filters;
    filters << "*" + PROFILE_EXTENSION;
    
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
    for (const QString& file : files) {
        QString profileName = file;
        profileName.chop(PROFILE_EXTENSION.length());
        profiles.append(profileName);
    }
    
    return profiles;
}

bool ProfileStorage::isValidProfileName(const QString& name) {
    if (name.isEmpty() || name.length() > 100) {
        return false;
    }
    
    // Check for invalid characters
    QRegularExpression invalidChars("[<>:\"/\\|?*]");
    if (invalidChars.match(name).hasMatch()) {
        return false;
    }
    
    // Check for reserved names
    QStringList reserved = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", 
                           "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", 
                           "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    
    if (reserved.contains(name.toUpper())) {
        return false;
    }
    
    return true;
}

QString ProfileStorage::sanitizeProfileName(const QString& name) {
    QString sanitized = name;
    
    // Replace invalid characters with underscores
    QRegularExpression invalidChars("[<>:\"/\\|?*]");
    sanitized.replace(invalidChars, "_");
    
    // Trim whitespace and limit length
    sanitized = sanitized.trimmed();
    if (sanitized.length() > 100) {
        sanitized = sanitized.left(100);
    }
    
    // Ensure it's not empty
    if (sanitized.isEmpty()) {
        sanitized = "Profile";
    }
    
    return sanitized;
}