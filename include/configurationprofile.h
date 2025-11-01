/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef CONFIGURATIONPROFILE_H
#define CONFIGURATIONPROFILE_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>

struct ConfigurationProfile {
    // Profile metadata
    QString name;
    QString description;
    QDateTime created;
    QDateTime lastUsed;
    QString version = "1.0";
    
    // Machine Configuration
    QString machineType = "-xl";
    QString videoSystem = "-pal";
    bool basicEnabled = true;
    bool altirraOSEnabled = false;
    bool altirraBASICEnabled = false;
    QString osRomPath;
    QString basicRomPath;
    
    // Memory Configuration
    bool enable800Ram = false;
    int mosaicSize = 0;
    int axlonSize = 0;
    bool axlonShadow = false;
    bool enableMapRam = true;
    
    // Performance
    bool turboMode = false;
    int emulationSpeedIndex = 1; // 1x speed
    
    // Audio Configuration
    bool audioEnabled = true;
    int audioFrequency = 44100;
    int audioBits = 16;
    int audioVolume = 80;
    int audioBufferLength = 100;
    int audioLatency = 20;
    bool consoleSound = true;
    bool serialSound = false;
    bool stereoPokey = false;
    
    // Video Configuration
    QString artifactingMode = "none";
    bool showFPS = false;
    bool scalingFilter = true;
    bool integerScaling = false;
    bool keepAspectRatio = true;
    bool fullscreenMode = false;
    
    // Screen Display Options
    QString horizontalArea = "tv";
    QString verticalArea = "tv";
    int horizontalShift = 0;
    int verticalShift = 0;
    QString fitScreen = "both";
    bool show80Column = false;
    bool vSyncEnabled = false;
    
    // PAL Color Settings (slider values -100 to 100, except gamma 10-400)
    int palSaturation = 0;
    int palContrast = 0;
    int palBrightness = 0;
    int palGamma = 100;
    int palTint = 0;
    
    // NTSC Color Settings (slider values -100 to 100, except gamma 10-400)
    int ntscSaturation = 0;
    int ntscContrast = 0;
    int ntscBrightness = 0;
    int ntscGamma = 100;
    int ntscTint = 0;
    
    // Input Configuration
    bool joystickEnabled = true;
    bool joystick0Hat = false;
    bool joystick1Hat = false;
    bool joystick2Hat = false;
    bool joystick3Hat = false;
    bool joyDistinct = false;
    bool kbdJoy0Enabled = false;
    bool kbdJoy1Enabled = false;
    bool swapJoysticks = false;
    bool grabMouse = false;
    QString mouseDevice;
    bool keyboardToggle = false;
    bool keyboardLeds = false;
    
    // Cartridge Configuration
    struct CartridgeConfig {
        bool enabled = false;
        QString path;
        int type = -1; // Auto-detect
    };
    CartridgeConfig primaryCartridge;
    CartridgeConfig piggybackCartridge;
    bool cartridgeAutoReboot = true;
    
    // Disk Configuration (D1-D8)
    struct DiskConfig {
        bool enabled = false;
        QString path;
        bool readOnly = false;
    };
    DiskConfig disks[8];
    
    // Cassette Configuration
    struct CassetteConfig {
        bool enabled = false;
        QString path;
        bool readOnly = false;
        bool bootTape = false;
    } cassette;
    
    // Hard Drive Configuration (H1-H4)
    struct HardDriveConfig {
        bool enabled = false;
        QString path;
    };
    HardDriveConfig hardDrives[4];
    bool hdReadOnly = false;
    QString hdDeviceName = "H";
    
    // Special Devices
    QString rDeviceName = "R";
    bool netSIOEnabled = false;
    bool rtimeEnabled = false;
    
    // Printer Configuration
    struct PrinterConfig {
        bool enabled = false;
        QString outputFormat = "Text";
        QString printerType = "Generic";
    } printer;
    
    // Hardware Extensions
    bool xep80Enabled = false;
    bool af80Enabled = false;
    bool bit3Enabled = false;
    bool atari1400Enabled = false;
    bool atari1450Enabled = false;
    bool proto80Enabled = false;
    bool voiceboxEnabled = false;
    bool sioAcceleration = true;
    
    // Serialization methods
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
    
    // Utility methods
    bool isValid() const;
    QString getDisplayName() const;
    void updateLastUsed();
    
    // Default constructor
    ConfigurationProfile() {
        created = QDateTime::currentDateTime();
        lastUsed = created;
    }
};

class ProfileStorage {
public:
    // Storage management
    static QString getProfileDirectory();
    static QString getProfileFilePath(const QString& name);
    static bool ensureProfileDirectoryExists();
    
    // Profile file operations
    static bool saveProfileToFile(const QString& name, const ConfigurationProfile& profile);
    static ConfigurationProfile loadProfileFromFile(const QString& name);
    static bool deleteProfileFile(const QString& name);
    static bool profileFileExists(const QString& name);
    
    // Profile listing
    static QStringList getAvailableProfiles();
    
    // Validation
    static bool isValidProfileName(const QString& name);
    static QString sanitizeProfileName(const QString& name);
    
private:
    static const QString PROFILE_EXTENSION;
    static const QString PROFILE_SUBDIR;
};

#endif // CONFIGURATIONPROFILE_H