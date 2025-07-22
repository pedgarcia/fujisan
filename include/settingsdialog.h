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
#include "atariemulator.h"
#include "configurationprofilemanager.h"
#include "profileselectionwidget.h"

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AtariEmulator* emulator, QWidget *parent = nullptr);

signals:
    void settingsChanged();

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
    
    // Profile management slots
    void onProfileChangeRequested(const QString& profileName);
    void onSaveCurrentProfile(const QString& profileName);
    void onLoadProfile(const QString& profileName);

private:
    void createHardwareTab();
    void createAudioConfigTab();
    void createVideoDisplayTab();
    void createInputConfigTab();
    void createMediaConfigTab();
    void loadSettings();
    void saveSettings();
    void applySettings();
    void applyMediaSettings();
    void updateVideoSystemDependentControls();
    void setupFilePathTooltip(QLineEdit* lineEdit);
    void populateCartridgeTypes(QComboBox* combo);
    
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
    
    // Memory Configuration controls
    QCheckBox* m_enable800RamCheck;
    QCheckBox* m_enableMosaicCheck;
    QSpinBox* m_mosaicSizeSpinBox;
    QCheckBox* m_enableAxlonCheck;
    QSpinBox* m_axlonSizeSpinBox;
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
    QCheckBox* m_keepAspectRatio;
    QCheckBox* m_fullscreenMode;
    
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
    QCheckBox* m_joystick0Hat;
    QCheckBox* m_joystick1Hat;
    QCheckBox* m_joystick2Hat;
    QCheckBox* m_joystick3Hat;
    QCheckBox* m_joyDistinct;
    
    // Keyboard Joystick Emulation
    QCheckBox* m_kbdJoy0Enabled;
    QCheckBox* m_kbdJoy1Enabled;
    
    // Mouse Configuration
    QCheckBox* m_grabMouse;
    QLineEdit* m_mouseDevice;
    
    // Keyboard Configuration
    QCheckBox* m_keyboardToggle;
    QCheckBox* m_keyboardLeds;
    
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
    
    // Store original settings for cancel functionality
    struct OriginalSettings {
        QString machineType;
        QString videoSystem;
        bool basicEnabled;
        bool altirraOSEnabled;
    } m_originalSettings;
};

#endif // SETTINGSDIALOG_H