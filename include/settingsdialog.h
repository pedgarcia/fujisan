/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QProgressBar>
#include "atariemulator.h"
#include "configurationprofilemanager.h"
#include "profileselectionwidget.h"

#ifndef Q_OS_WIN
// FujiNet support (not available on Windows)
class FujiNetService;
class FujiNetProcessManager;
class FujiNetBinaryManager;
#endif

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AtariEmulator* emulator, ConfigurationProfileManager* profileManager, QWidget *parent = nullptr);

    // Public methods for external synchronization
    void loadSettings();

#ifndef Q_OS_WIN
    // Set shared FujiNet managers (must be called before showing dialog)
    void setFujiNetManagers(FujiNetProcessManager* processManager,
                           FujiNetBinaryManager* binaryManager);
#endif

signals:
    void settingsChanged();
    void syncPrinterStateRequested();
    void netSIOEnabledChanged(bool enabled);

private slots:
    void accept() override;
    void reject() override;
    void restoreDefaults();

private slots:
    void browseDiskImage(int diskNumber);
    void browseCassetteImage();
    void browseHardDriveDirectory(int driveNumber);
    void browseOSROM();
    void browseBasicROM();
    void browseCartridge();
    void browseCartridge2();
    void onMachineTypeChanged();
    void onAltirraOSChanged();
    void onAltirraBASICChanged();
    void onNetSIOToggled(bool enabled);
    
    // Profile management slots
    void onProfileChangeRequested(const QString& profileName);
    void onSaveCurrentProfile(const QString& profileName);
    void onLoadProfile(const QString& profileName);
    void onJoystickDeviceChanged();

#ifndef Q_OS_WIN
    // FujiNet slots
    void onFujiNetBrowseBinary();
    void onFujiNetBrowseSDFolder();
    void onFujiNetBrowseConfig();
    void onFujiNetOpenConfigFolder();
    void onFujiNetCustomConfigToggled(bool enabled);
    void onFujiNetStart();
    void onFujiNetStop();
    void onFujiNetConnectionChanged(bool connected);
    void onFujiNetProcessStateChanged(int state);
#endif

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createHardwareTab();
    void createAudioConfigTab();
    void createVideoDisplayTab();
    void createInputConfigTab();
    void createMediaConfigTab();
    void createEmulatorTab();
#ifndef Q_OS_WIN
    void createFujiNetTab();
#endif
    void saveSettings();
    void applySettings();
    void applyMediaSettings();
    void triggerNetSIORestart(bool netSIOEnabled);
    void updateVideoSystemDependentControls();
    void setupFilePathTooltip(QLineEdit* lineEdit);
    void populateCartridgeTypes(QComboBox* combo);
    void populateJoystickDevices();
    void updateKeyboardMappingLabels();
    
    // Profile management
    ConfigurationProfile getCurrentUIState() const;
    void loadProfileToUI(const ConfigurationProfile& profile);
    void createProfileSection();
    
    AtariEmulator* m_emulator;
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;
    QPushButton* m_defaultsButton;
    
    // Profile management
    ConfigurationProfileManager* m_profileManager;
    ProfileSelectionWidget* m_profileWidget;
    
    // Hardware Configuration controls
    QWidget* m_hardwareTab;
    QComboBox* m_machineTypeCombo;
    QComboBox* m_videoSystemCombo;
    QCheckBox* m_basicEnabledCheck;
    QCheckBox* m_altirraOSCheck;
    QCheckBox* m_altirraBASICCheck;
    
    // Memory Configuration controls
    QCheckBox* m_enable800RamCheck;
    QCheckBox* m_enableMosaicCheck;
    QSpinBox* m_mosaicSizeSpinBox;
    QCheckBox* m_enableAxlonCheck;
    QComboBox* m_axlonSizeCombo;  // Changed from QSpinBox to QComboBox for valid values only
    QCheckBox* m_axlonShadowCheck;
    QCheckBox* m_enableMapRamCheck;
    
    // Performance controls
    QCheckBox* m_turboModeCheck;
    QSlider* m_speedSlider;
    QLabel* m_speedLabel;
    
    // Cartridge Configuration controls
    QCheckBox* m_cartridgeEnabledCheck;
    QLineEdit* m_cartridgePath;
    QPushButton* m_cartridgeBrowse;
    QComboBox* m_cartridgeTypeCombo;
    QCheckBox* m_cartridge2EnabledCheck;
    QLineEdit* m_cartridge2Path;
    QPushButton* m_cartridge2Browse;
    QComboBox* m_cartridge2TypeCombo;
    QCheckBox* m_cartridgeAutoRebootCheck;
    
    // ROM Configuration controls
    QLabel* m_osRomLabel;
    QLineEdit* m_osRomPath;
    QPushButton* m_osRomBrowse;
    QLabel* m_basicRomLabel;
    QLineEdit* m_basicRomPath;
    QPushButton* m_basicRomBrowse;
    
    QCheckBox* m_stereoPokey;
    QCheckBox* m_sioAcceleration;
    
    // 80-Column Cards
    QCheckBox* m_xep80Enabled;
    QCheckBox* m_af80Enabled;
    QCheckBox* m_bit3Enabled;
    
    // PBI Extensions
    QCheckBox* m_atari1400Enabled;
    QCheckBox* m_atari1450Enabled;
    QCheckBox* m_proto80Enabled;
    
    // Voice Synthesis
    QCheckBox* m_voiceboxEnabled;
    
    // Audio Configuration controls  
    QWidget* m_audioTab;
    QCheckBox* m_soundEnabled;
    QComboBox* m_audioFrequency;
    QComboBox* m_audioBits;
    QSlider* m_volumeSlider;
    QLabel* m_volumeLabel;
    QSpinBox* m_bufferLengthSpinBox;
    QSpinBox* m_audioLatencySpinBox;
    QCheckBox* m_consoleSound;
    QCheckBox* m_serialSound;
    
    // Video and Display controls
    QWidget* m_videoTab;
    QComboBox* m_artifactingMode;
    QCheckBox* m_showFPS;
    QCheckBox* m_scalingFilter;
    QCheckBox* m_integerScaling;
    QCheckBox* m_keepAspectRatio;
    QCheckBox* m_fullscreenMode;
    
    // Screen Display Options
    QComboBox* m_horizontalArea;
    QComboBox* m_verticalArea;
    QSpinBox* m_horizontalShift;
    QSpinBox* m_verticalShift;
    QComboBox* m_fitScreen;
    QCheckBox* m_show80Column;
    QCheckBox* m_vSyncEnabled;
    
    // PAL-specific controls
    QGroupBox* m_palGroup;
    QComboBox* m_palBlending;
    // FUTURE: Universal scanlines controls (commented out - not working)
    // QSlider* m_scanlinesSlider;
    // QLabel* m_scanlinesLabel;
    // QCheckBox* m_scanlinesInterpolation;
    QSlider* m_palSaturationSlider;
    QLabel* m_palSaturationLabel;
    QSlider* m_palContrastSlider;
    QLabel* m_palContrastLabel;
    QSlider* m_palBrightnessSlider;
    QLabel* m_palBrightnessLabel;
    QSlider* m_palGammaSlider;
    QLabel* m_palGammaLabel;
    QSlider* m_palTintSlider;
    QLabel* m_palTintLabel;
    
    // NTSC-specific controls  
    QGroupBox* m_ntscGroup;
    // QComboBox* m_ntscArtifacting;  // Now handled by main artifacting dropdown
    QCheckBox* m_ntscSharpness;
    QSlider* m_ntscSaturationSlider;
    QLabel* m_ntscSaturationLabel;
    QSlider* m_ntscContrastSlider;
    QLabel* m_ntscContrastLabel;
    QSlider* m_ntscBrightnessSlider;
    QLabel* m_ntscBrightnessLabel;
    QSlider* m_ntscGammaSlider;
    QLabel* m_ntscGammaLabel;
    QSlider* m_ntscTintSlider;
    QLabel* m_ntscTintLabel;
    
    // Input Configuration controls
    QWidget* m_inputTab;
    
    // Joystick Configuration
    QCheckBox* m_joystickEnabled;
    QComboBox* m_joystick1Device;
    QComboBox* m_joystick2Device;
    QCheckBox* m_swapJoysticks;

    // Keyboard mapping labels (shown when "Keyboard" is selected)
    QLabel* m_joystick1KeysLabel;
    QLabel* m_joystick2KeysLabel;

    // Mouse Configuration (hidden for future re-implementation)
    QGroupBox* m_mouseGroup;
    QCheckBox* m_grabMouse;
    QLineEdit* m_mouseDevice;
    
    // Media Configuration controls
    QWidget* m_mediaTab;
    
    // Floppy Disks (D1-D8)
    QCheckBox* m_diskEnabled[8];
    QLineEdit* m_diskPath[8];
    QPushButton* m_diskBrowse[8];
    QCheckBox* m_diskReadOnly[8];
    
    // Cassette
    QCheckBox* m_cassetteEnabled;
    QLineEdit* m_cassettePath;
    QPushButton* m_cassetteBrowse;
    QCheckBox* m_cassetteReadOnly;
    QCheckBox* m_cassetteBootTape;
    
    // Hard Drives (H1-H4)
    QCheckBox* m_hdEnabled[4];
    QLineEdit* m_hdPath[4];
    QPushButton* m_hdBrowse[4];
    QCheckBox* m_hdReadOnly;
    QLineEdit* m_hdDeviceName;
    
    // Special Devices
    QLineEdit* m_rDeviceName;
    QCheckBox* m_netSIOEnabled;
    QCheckBox* m_rtimeEnabled;
    
    // Printer Configuration
    QCheckBox* m_printerEnabled;
    QComboBox* m_printerOutputFormat;
    QComboBox* m_printerType;
    
    // Emulator Configuration controls
    QWidget* m_emulatorTab;
    QCheckBox* m_tcpServerEnabled;
    QSpinBox* m_tcpServerPort;

    // Log Filtering controls
    QCheckBox* m_hideFujiNetLogs;
    QLineEdit* m_logFilterString;
    QCheckBox* m_logFilterRegex;

#ifndef Q_OS_WIN
    // FujiNet Configuration controls
    QWidget* m_fujinetTab;
    QSpinBox* m_fujinetApiPort;
    QSpinBox* m_fujinetNetsioPort;
    QComboBox* m_fujinetLaunchBehavior;
    QLabel* m_fujinetStatusLabel;
    QLineEdit* m_fujinetBinaryPath;
    QPushButton* m_fujinetBrowseButton;
    QLabel* m_fujinetVersionLabel;
    QPushButton* m_fujinetStartButton;
    QPushButton* m_fujinetStopButton;

    // SD Card Folder configuration
    QLineEdit* m_fujinetSDPath;
    QPushButton* m_fujinetBrowseSDButton;

    // Config File configuration
    QLabel* m_fujinetDefaultConfigLabel;
    QPushButton* m_fujinetOpenConfigFolderButton;
    QCheckBox* m_fujinetUseCustomConfig;
    QLineEdit* m_fujinetCustomConfigPath;
    QPushButton* m_fujinetBrowseConfigButton;

    // Track original values for change detection
    int m_originalHttpPort;
    int m_originalNetsioPort;
    QString m_originalSDPath;
    bool m_originalUseCustomConfig;
    QString m_originalCustomConfigPath;

    // Restart warning label
    QLabel* m_fujinetRestartWarningLabel;

    // FujiNet service classes (shared with MainWindow - not owned)
    FujiNetService* m_fujinetService;  // Dialog-only (owned)
    FujiNetProcessManager* m_fujinetProcessManager;  // Shared pointer (not owned)
    FujiNetBinaryManager* m_fujinetBinaryManager;    // Shared pointer (not owned)

    // Helper functions
    void updateFujiNetConfigFile(const QString& configPath, int netsioPort);
    void checkFujiNetRestartRequired();
#endif

    // Store original settings for cancel functionality
    struct OriginalSettings {
        QString machineType;
        QString videoSystem;
        bool basicEnabled;
        bool altirraOSEnabled;
        bool altirraBASICEnabled;
        bool netSIOEnabled;  // Track NetSIO state for restart detection
        bool printerEnabled; // Track printer state for restart detection
        bool tcpServerEnabled; // Track TCP server state
        int tcpServerPort;   // Track TCP server port
    } m_originalSettings;
};

#endif // SETTINGSDIALOG_H