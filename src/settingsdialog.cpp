/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "settingsdialog.h"
#include <QDebug>

SettingsDialog::SettingsDialog(AtariEmulator* emulator, QWidget *parent)
    : QDialog(parent)
    , m_emulator(emulator)
    , m_tabWidget(nullptr)
    , m_buttonBox(nullptr)
    , m_defaultsButton(nullptr)
    , m_machineTab(nullptr)
    , m_machineTypeCombo(nullptr)
    , m_videoSystemCombo(nullptr)
    , m_basicEnabledCheck(nullptr)
    , m_altirraOSCheck(nullptr)
{
    setWindowTitle("Settings");
    setModal(true);
    resize(500, 400);
    
    // Store original settings for cancel functionality
    m_originalSettings.machineType = m_emulator->getMachineType();
    m_originalSettings.videoSystem = m_emulator->getVideoSystem();
    m_originalSettings.basicEnabled = m_emulator->isBasicEnabled();
    m_originalSettings.altirraOSEnabled = m_emulator->isAltirraOSEnabled();
    
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget);
    
    // Create tabs
    createMachineConfigTab();
    createHardwareExtensionsTab();
    createAudioConfigTab();
    createVideoDisplayTab();
    
    // Create button box with custom buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_defaultsButton = new QPushButton("Restore Defaults");
    m_buttonBox->addButton(m_defaultsButton, QDialogButtonBox::ResetRole);
    
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(m_defaultsButton, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    
    mainLayout->addWidget(m_buttonBox);
    
    // Load current settings
    loadSettings();
    
    // Connect video system change to update PAL/NTSC controls
    connect(m_videoSystemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updateVideoSystemDependentControls);
}

void SettingsDialog::createMachineConfigTab()
{
    m_machineTab = new QWidget();
    m_tabWidget->addTab(m_machineTab, "Machine Configuration");
    
    QVBoxLayout* tabLayout = new QVBoxLayout(m_machineTab);
    
    // Machine Type Group
    QGroupBox* machineGroup = new QGroupBox("Machine Type");
    QFormLayout* machineLayout = new QFormLayout(machineGroup);
    
    m_machineTypeCombo = new QComboBox();
    m_machineTypeCombo->addItem("Atari 400/800", "-atari");
    m_machineTypeCombo->addItem("Atari 800XL", "-xl");
    m_machineTypeCombo->addItem("Atari 130XE", "-xe");
    m_machineTypeCombo->addItem("Atari 5200", "-5200");
    
    machineLayout->addRow("Model:", m_machineTypeCombo);
    tabLayout->addWidget(machineGroup);
    
    // Video System Group
    QGroupBox* videoGroup = new QGroupBox("Video System");
    QFormLayout* videoLayout = new QFormLayout(videoGroup);
    
    m_videoSystemCombo = new QComboBox();
    m_videoSystemCombo->addItem("PAL (49.86 fps)", "-pal");
    m_videoSystemCombo->addItem("NTSC (59.92 fps)", "-ntsc");
    
    videoLayout->addRow("Standard:", m_videoSystemCombo);
    tabLayout->addWidget(videoGroup);
    
    // System Options Group
    QGroupBox* systemGroup = new QGroupBox("System Options");
    QVBoxLayout* systemLayout = new QVBoxLayout(systemGroup);
    
    m_basicEnabledCheck = new QCheckBox("Enable Atari BASIC");
    m_basicEnabledCheck->setToolTip("Enable or disable the built-in Atari BASIC interpreter");
    systemLayout->addWidget(m_basicEnabledCheck);
    
    m_altirraOSCheck = new QCheckBox("Use Altirra OS (built-in ROMs)");
    m_altirraOSCheck->setToolTip("Use built-in Altirra OS ROMs instead of external ROM files");
    systemLayout->addWidget(m_altirraOSCheck);
    
    tabLayout->addWidget(systemGroup);
    
    // Add stretch to push everything to the top
    tabLayout->addStretch();
}

void SettingsDialog::createHardwareExtensionsTab()
{
    m_hardwareTab = new QWidget();
    m_tabWidget->addTab(m_hardwareTab, "Hardware Extensions");
    
    QVBoxLayout* tabLayout = new QVBoxLayout(m_hardwareTab);
    
    // POKEY Enhancements Group
    QGroupBox* pokeyGroup = new QGroupBox("POKEY Sound Chip");
    QVBoxLayout* pokeyLayout = new QVBoxLayout(pokeyGroup);
    
    m_stereoPokey = new QCheckBox("Enable Stereo POKEY");
    m_stereoPokey->setToolTip("Enable dual POKEY sound chips for stereo audio");
    pokeyLayout->addWidget(m_stereoPokey);
    
    tabLayout->addWidget(pokeyGroup);
    
    // SIO and Device Extensions Group  
    QGroupBox* deviceGroup = new QGroupBox("Device Extensions");
    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceGroup);
    
    m_sioAcceleration = new QCheckBox("Enable SIO Acceleration");
    m_sioAcceleration->setToolTip("Accelerate Serial I/O operations for faster disk access");
    deviceLayout->addWidget(m_sioAcceleration);
    
    m_rDeviceEnabled = new QCheckBox("Enable R: Device (Networking)");
    m_rDeviceEnabled->setToolTip("Enable R: device for network and HTTP access");
    deviceLayout->addWidget(m_rDeviceEnabled);
    
    m_hDeviceEnabled = new QCheckBox("Enable H: Device (Hard Drive)");
    m_hDeviceEnabled->setToolTip("Enable H: device for hard drive directory access");
    deviceLayout->addWidget(m_hDeviceEnabled);
    
    m_pDeviceEnabled = new QCheckBox("Enable P: Device (Printer)");
    m_pDeviceEnabled->setToolTip("Enable P: device for printer output");
    deviceLayout->addWidget(m_pDeviceEnabled);
    
    tabLayout->addWidget(deviceGroup);
    tabLayout->addStretch();
}

void SettingsDialog::createAudioConfigTab()
{
    m_audioTab = new QWidget();
    m_tabWidget->addTab(m_audioTab, "Audio Configuration");
    
    QVBoxLayout* tabLayout = new QVBoxLayout(m_audioTab);
    
    // Audio System Group
    QGroupBox* audioGroup = new QGroupBox("Audio System");
    QFormLayout* audioLayout = new QFormLayout(audioGroup);
    
    m_soundEnabled = new QCheckBox("Enable Sound");
    m_soundEnabled->setToolTip("Enable or disable all audio output");
    audioLayout->addRow("", m_soundEnabled);
    
    m_audioFrequency = new QComboBox();
    m_audioFrequency->addItem("22050 Hz", 22050);
    m_audioFrequency->addItem("44100 Hz", 44100);
    m_audioFrequency->addItem("48000 Hz", 48000);
    m_audioFrequency->setToolTip("Audio sample rate - higher is better quality");
    audioLayout->addRow("Sample Rate:", m_audioFrequency);
    
    m_audioBits = new QComboBox();
    m_audioBits->addItem("8-bit", 8);
    m_audioBits->addItem("16-bit", 16);
    m_audioBits->setToolTip("Audio bit depth - 16-bit recommended");
    audioLayout->addRow("Bit Depth:", m_audioBits);
    
    tabLayout->addWidget(audioGroup);
    
    // Audio Features Group
    QGroupBox* featuresGroup = new QGroupBox("Audio Features");
    QVBoxLayout* featuresLayout = new QVBoxLayout(featuresGroup);
    
    m_consoleSound = new QCheckBox("Enable Console Speaker");
    m_consoleSound->setToolTip("Enable keyboard clicks and system beeps");
    featuresLayout->addWidget(m_consoleSound);
    
    m_serialSound = new QCheckBox("Enable Serial I/O Sounds");
    m_serialSound->setToolTip("Enable disk drive and cassette operation sounds");
    featuresLayout->addWidget(m_serialSound);
    
    tabLayout->addWidget(featuresGroup);
    tabLayout->addStretch();
}

void SettingsDialog::createVideoDisplayTab()
{
    m_videoTab = new QWidget();
    m_tabWidget->addTab(m_videoTab, "Video and Display");
    
    QVBoxLayout* tabLayout = new QVBoxLayout(m_videoTab);
    
    // General Video Group
    QGroupBox* generalGroup = new QGroupBox("General Video");
    QFormLayout* generalLayout = new QFormLayout(generalGroup);
    
    m_artifactingMode = new QComboBox();
    m_artifactingMode->addItem("None", "none");
    m_artifactingMode->addItem("GTIA", "gtia");
    m_artifactingMode->addItem("CTIA", "ctia");
    m_artifactingMode->setToolTip("Color artifacting simulation mode");
    generalLayout->addRow("Artifacting:", m_artifactingMode);
    
    m_showFPS = new QCheckBox("Show FPS Counter");
    m_showFPS->setToolTip("Display frames per second in the corner");
    generalLayout->addRow("", m_showFPS);
    
    m_scalingFilter = new QCheckBox("Enable Scaling Filter");
    m_scalingFilter->setToolTip("Apply smoothing when scaling the display");
    generalLayout->addRow("", m_scalingFilter);
    
    tabLayout->addWidget(generalGroup);
    
    // PAL-specific settings
    m_palGroup = new QGroupBox("PAL Video Options");
    QFormLayout* palLayout = new QFormLayout(m_palGroup);
    
    m_palBlending = new QComboBox();
    m_palBlending->addItem("None", "none");
    m_palBlending->addItem("Simple", "simple");
    m_palBlending->addItem("Linear", "linear");
    m_palBlending->setToolTip("PAL color blending mode");
    palLayout->addRow("Color Blending:", m_palBlending);
    
    m_palScanlines = new QCheckBox("Enable PAL Scanlines");
    m_palScanlines->setToolTip("Simulate PAL CRT scanline effect");
    palLayout->addRow("", m_palScanlines);
    
    tabLayout->addWidget(m_palGroup);
    
    // NTSC-specific settings
    m_ntscGroup = new QGroupBox("NTSC Video Options");
    QFormLayout* ntscLayout = new QFormLayout(m_ntscGroup);
    
    m_ntscArtifacting = new QComboBox();
    m_ntscArtifacting->addItem("Standard", "standard");
    m_ntscArtifacting->addItem("High Quality", "high");
    m_ntscArtifacting->addItem("Composite", "composite");
    m_ntscArtifacting->setToolTip("NTSC color artifacting quality");
    ntscLayout->addRow("Artifacting:", m_ntscArtifacting);
    
    m_ntscSharpness = new QCheckBox("Enable NTSC Sharpness");
    m_ntscSharpness->setToolTip("Enhance NTSC video sharpness");
    ntscLayout->addRow("", m_ntscSharpness);
    
    tabLayout->addWidget(m_ntscGroup);
    tabLayout->addStretch();
}

void SettingsDialog::loadSettings()
{
    QSettings settings("8bitrelics", "Fujisan");
    
    // Load Machine Configuration
    QString machineType = settings.value("machine/type", "-xl").toString();
    for (int i = 0; i < m_machineTypeCombo->count(); ++i) {
        if (m_machineTypeCombo->itemData(i).toString() == machineType) {
            m_machineTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    QString videoSystem = settings.value("machine/videoSystem", "-pal").toString();
    for (int i = 0; i < m_videoSystemCombo->count(); ++i) {
        if (m_videoSystemCombo->itemData(i).toString() == videoSystem) {
            m_videoSystemCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_basicEnabledCheck->setChecked(settings.value("machine/basicEnabled", true).toBool());
    m_altirraOSCheck->setChecked(settings.value("machine/altirraOS", false).toBool());
    
    // Load Hardware Extensions
    m_stereoPokey->setChecked(settings.value("hardware/stereoPokey", false).toBool());
    m_sioAcceleration->setChecked(settings.value("hardware/sioAcceleration", true).toBool());
    m_rDeviceEnabled->setChecked(settings.value("hardware/rDevice", false).toBool());
    m_hDeviceEnabled->setChecked(settings.value("hardware/hDevice", false).toBool());
    m_pDeviceEnabled->setChecked(settings.value("hardware/pDevice", false).toBool());
    
    // Load Audio Configuration
    m_soundEnabled->setChecked(settings.value("audio/enabled", true).toBool());
    
    int audioFreq = settings.value("audio/frequency", 44100).toInt();
    for (int i = 0; i < m_audioFrequency->count(); ++i) {
        if (m_audioFrequency->itemData(i).toInt() == audioFreq) {
            m_audioFrequency->setCurrentIndex(i);
            break;
        }
    }
    
    int audioBits = settings.value("audio/bits", 16).toInt();
    for (int i = 0; i < m_audioBits->count(); ++i) {
        if (m_audioBits->itemData(i).toInt() == audioBits) {
            m_audioBits->setCurrentIndex(i);
            break;
        }
    }
    
    m_consoleSound->setChecked(settings.value("audio/consoleSound", true).toBool());
    m_serialSound->setChecked(settings.value("audio/serialSound", false).toBool());
    
    // Load Video and Display
    QString artifacting = settings.value("video/artifacting", "none").toString();
    for (int i = 0; i < m_artifactingMode->count(); ++i) {
        if (m_artifactingMode->itemData(i).toString() == artifacting) {
            m_artifactingMode->setCurrentIndex(i);
            break;
        }
    }
    
    m_showFPS->setChecked(settings.value("video/showFPS", false).toBool());
    m_scalingFilter->setChecked(settings.value("video/scalingFilter", true).toBool());
    
    QString palBlending = settings.value("video/palBlending", "simple").toString();
    for (int i = 0; i < m_palBlending->count(); ++i) {
        if (m_palBlending->itemData(i).toString() == palBlending) {
            m_palBlending->setCurrentIndex(i);
            break;
        }
    }
    
    m_palScanlines->setChecked(settings.value("video/palScanlines", false).toBool());
    
    QString ntscArtifacting = settings.value("video/ntscArtifacting", "standard").toString();
    for (int i = 0; i < m_ntscArtifacting->count(); ++i) {
        if (m_ntscArtifacting->itemData(i).toString() == ntscArtifacting) {
            m_ntscArtifacting->setCurrentIndex(i);
            break;
        }
    }
    
    m_ntscSharpness->setChecked(settings.value("video/ntscSharpness", true).toBool());
    
    // Update PAL/NTSC dependent controls
    updateVideoSystemDependentControls();
    
    qDebug() << "Settings loaded from persistent storage - Machine:" << machineType 
             << "Video:" << videoSystem << "BASIC:" << m_basicEnabledCheck->isChecked();
}

void SettingsDialog::saveSettings()
{
    QSettings settings("8bitrelics", "Fujisan");
    
    // Save Machine Configuration
    QString machineType = m_machineTypeCombo->currentData().toString();
    QString videoSystem = m_videoSystemCombo->currentData().toString();
    bool basicEnabled = m_basicEnabledCheck->isChecked();
    bool altirraOSEnabled = m_altirraOSCheck->isChecked();
    
    settings.setValue("machine/type", machineType);
    settings.setValue("machine/videoSystem", videoSystem);
    settings.setValue("machine/basicEnabled", basicEnabled);
    settings.setValue("machine/altirraOS", altirraOSEnabled);
    
    // Save Hardware Extensions
    settings.setValue("hardware/stereoPokey", m_stereoPokey->isChecked());
    settings.setValue("hardware/sioAcceleration", m_sioAcceleration->isChecked());
    settings.setValue("hardware/rDevice", m_rDeviceEnabled->isChecked());
    settings.setValue("hardware/hDevice", m_hDeviceEnabled->isChecked());
    settings.setValue("hardware/pDevice", m_pDeviceEnabled->isChecked());
    
    // Save Audio Configuration
    settings.setValue("audio/enabled", m_soundEnabled->isChecked());
    settings.setValue("audio/frequency", m_audioFrequency->currentData().toInt());
    settings.setValue("audio/bits", m_audioBits->currentData().toInt());
    settings.setValue("audio/consoleSound", m_consoleSound->isChecked());
    settings.setValue("audio/serialSound", m_serialSound->isChecked());
    
    // Save Video and Display
    settings.setValue("video/artifacting", m_artifactingMode->currentData().toString());
    settings.setValue("video/showFPS", m_showFPS->isChecked());
    settings.setValue("video/scalingFilter", m_scalingFilter->isChecked());
    settings.setValue("video/palBlending", m_palBlending->currentData().toString());
    settings.setValue("video/palScanlines", m_palScanlines->isChecked());
    settings.setValue("video/ntscArtifacting", m_ntscArtifacting->currentData().toString());
    settings.setValue("video/ntscSharpness", m_ntscSharpness->isChecked());
    
    qDebug() << "Settings saved to persistent storage - Machine:" << machineType 
             << "Video:" << videoSystem << "BASIC:" << basicEnabled;
    
    // Apply settings to emulator
    m_emulator->setMachineType(machineType);
    m_emulator->setVideoSystem(videoSystem);
    m_emulator->setBasicEnabled(basicEnabled);
    m_emulator->setAltirraOSEnabled(altirraOSEnabled);
    m_emulator->enableAudio(m_soundEnabled->isChecked());
}

void SettingsDialog::applySettings()
{
    saveSettings();
    
    // Restart emulator with new settings
    m_emulator->shutdown();
    if (m_emulator->initializeWithConfig(m_emulator->isBasicEnabled(), 
                                       m_emulator->getMachineType(), 
                                       m_emulator->getVideoSystem())) {
        qDebug() << "Emulator restarted with new settings";
        emit settingsChanged();
    } else {
        qDebug() << "Failed to restart emulator with new settings";
    }
}

void SettingsDialog::accept()
{
    applySettings();
    QDialog::accept();
}

void SettingsDialog::reject()
{
    // Restore original settings without applying
    qDebug() << "Settings dialog cancelled - restoring original settings";
    QDialog::reject();
}

void SettingsDialog::updateVideoSystemDependentControls()
{
    bool isPAL = (m_videoSystemCombo->currentData().toString() == "-pal");
    m_palGroup->setEnabled(isPAL);
    m_ntscGroup->setEnabled(!isPAL);
}

void SettingsDialog::restoreDefaults()
{
    qDebug() << "Restoring default settings";
    
    // Machine Configuration defaults
    m_machineTypeCombo->setCurrentIndex(1); // Atari 800XL
    m_videoSystemCombo->setCurrentIndex(0); // PAL
    m_basicEnabledCheck->setChecked(true);   // BASIC enabled
    m_altirraOSCheck->setChecked(false);     // Use external ROMs
    
    // Hardware Extensions defaults
    m_stereoPokey->setChecked(false);
    m_sioAcceleration->setChecked(true);
    m_rDeviceEnabled->setChecked(false);
    m_hDeviceEnabled->setChecked(false);
    m_pDeviceEnabled->setChecked(false);
    
    // Audio Configuration defaults
    m_soundEnabled->setChecked(true);
    m_audioFrequency->setCurrentIndex(1); // 44100 Hz
    m_audioBits->setCurrentIndex(1);       // 16-bit
    m_consoleSound->setChecked(true);
    m_serialSound->setChecked(false);
    
    // Video and Display defaults
    m_artifactingMode->setCurrentIndex(0); // None
    m_showFPS->setChecked(false);
    m_scalingFilter->setChecked(true);
    m_palBlending->setCurrentIndex(1);     // Simple
    m_palScanlines->setChecked(false);
    m_ntscArtifacting->setCurrentIndex(0); // Standard
    m_ntscSharpness->setChecked(true);
    
    // Update PAL/NTSC dependent controls
    updateVideoSystemDependentControls();
}