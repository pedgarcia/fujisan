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

private:
    void createMachineConfigTab();
    void createHardwareExtensionsTab();
    void createAudioConfigTab();
    void createVideoDisplayTab();
    void createMediaConfigTab();
    void loadSettings();
    void saveSettings();
    void applySettings();
    void applyMediaSettings();
    void updateVideoSystemDependentControls();
    void setupFilePathTooltip(QLineEdit* lineEdit);
    void populateCartridgeTypes(QComboBox* combo);
    
    AtariEmulator* m_emulator;
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;
    QPushButton* m_defaultsButton;
    
    // Machine Configuration controls
    QWidget* m_machineTab;
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
    
    // Hardware controls
    QWidget* m_hardwareTab;
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
    QCheckBox* m_palScanlines;
    
    // NTSC-specific controls  
    QGroupBox* m_ntscGroup;
    QComboBox* m_ntscArtifacting;
    QCheckBox* m_ntscSharpness;
    
    // Media Configuration controls
    QWidget* m_mediaTab;
    
    // Floppy Disks (D1-D4)
    QCheckBox* m_diskEnabled[4];
    QLineEdit* m_diskPath[4];
    QPushButton* m_diskBrowse[4];
    QCheckBox* m_diskReadOnly[4];
    
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