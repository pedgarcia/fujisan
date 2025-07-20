/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "settingsdialog.h"
#include <QDebug>
#include <QFileDialog>

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
    resize(850, 500);
    
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
    createMediaConfigTab();
    
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
    QVBoxLayout* machineVLayout = new QVBoxLayout(machineGroup);
    
    // Machine selection and ROM path row
    QHBoxLayout* machineRow = new QHBoxLayout();
    
    // Left side - Machine dropdown
    QVBoxLayout* machineLeft = new QVBoxLayout();
    QLabel* modelLabel = new QLabel("Model:");
    machineLeft->addWidget(modelLabel);
    
    m_machineTypeCombo = new QComboBox();
    m_machineTypeCombo->addItem("Atari 400/800", "-atari");
    m_machineTypeCombo->addItem("Atari 1200XL", "-1200");
    m_machineTypeCombo->addItem("Atari 800XL", "-xl");
    m_machineTypeCombo->addItem("Atari 130XE", "-xe");
    m_machineTypeCombo->addItem("Atari 320XE (Compy-Shop)", "-320xe");
    m_machineTypeCombo->addItem("Atari 320XE (Rambo XL)", "-rambo");
    m_machineTypeCombo->addItem("Atari 576XE", "-576xe");
    m_machineTypeCombo->addItem("Atari 1088XE", "-1088xe");
    m_machineTypeCombo->addItem("Atari XEGS", "-xegs");
    m_machineTypeCombo->addItem("Atari 5200", "-5200");
    
    connect(m_machineTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onMachineTypeChanged);
    
    machineLeft->addWidget(m_machineTypeCombo);
    machineRow->addLayout(machineLeft, 1);
    
    // Right side - OS ROM selector
    QVBoxLayout* romRight = new QVBoxLayout();
    m_osRomLabel = new QLabel("OS ROM:");
    romRight->addWidget(m_osRomLabel);
    
    QHBoxLayout* osRomLayout = new QHBoxLayout();
    m_osRomPath = new QLineEdit();
    m_osRomPath->setPlaceholderText("Select OS ROM file");
    setupFilePathTooltip(m_osRomPath);
    osRomLayout->addWidget(m_osRomPath, 1);
    
    m_osRomBrowse = new QPushButton("Browse...");
    connect(m_osRomBrowse, &QPushButton::clicked, this, &SettingsDialog::browseOSROM);
    osRomLayout->addWidget(m_osRomBrowse);
    
    romRight->addLayout(osRomLayout);
    machineRow->addLayout(romRight, 2);
    
    machineVLayout->addLayout(machineRow);
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
    
    // BASIC configuration row
    QHBoxLayout* basicRow = new QHBoxLayout();
    m_basicEnabledCheck = new QCheckBox("Enable BASIC");
    m_basicEnabledCheck->setToolTip("Enable or disable the Atari BASIC interpreter");
    basicRow->addWidget(m_basicEnabledCheck);
    
    // BASIC ROM selector (inline)
    m_basicRomLabel = new QLabel("ROM:");
    basicRow->addWidget(m_basicRomLabel);
    
    m_basicRomPath = new QLineEdit();
    m_basicRomPath->setPlaceholderText("Select BASIC ROM file");
    setupFilePathTooltip(m_basicRomPath);
    basicRow->addWidget(m_basicRomPath, 1);
    
    m_basicRomBrowse = new QPushButton("Browse...");
    connect(m_basicRomBrowse, &QPushButton::clicked, this, &SettingsDialog::browseBasicROM);
    basicRow->addWidget(m_basicRomBrowse);
    
    systemLayout->addLayout(basicRow);
    
    m_altirraOSCheck = new QCheckBox("Use Altirra OS (built-in ROMs)");
    m_altirraOSCheck->setToolTip("Use built-in Altirra OS ROMs instead of external ROM files");
    connect(m_altirraOSCheck, &QCheckBox::toggled, this, &SettingsDialog::onAltirraOSChanged);
    systemLayout->addWidget(m_altirraOSCheck);
    
    tabLayout->addWidget(systemGroup);
    
    // Add stretch to push everything to the top
    tabLayout->addStretch();
}

void SettingsDialog::createHardwareExtensionsTab()
{
    m_hardwareTab = new QWidget();
    m_tabWidget->addTab(m_hardwareTab, "Hardware");
    
    QVBoxLayout* tabLayout = new QVBoxLayout(m_hardwareTab);
    
    // POKEY Enhancements Group
    QGroupBox* pokeyGroup = new QGroupBox("POKEY Sound Chip");
    QVBoxLayout* pokeyLayout = new QVBoxLayout(pokeyGroup);
    
    m_stereoPokey = new QCheckBox("Enable Stereo POKEY");
    m_stereoPokey->setToolTip("Enable dual POKEY sound chips for stereo audio");
    pokeyLayout->addWidget(m_stereoPokey);
    
    tabLayout->addWidget(pokeyGroup);
    
    // SIO Performance Group  
    QGroupBox* sioGroup = new QGroupBox("SIO Performance");
    QVBoxLayout* sioLayout = new QVBoxLayout(sioGroup);
    
    m_sioAcceleration = new QCheckBox("Enable SIO Acceleration");
    m_sioAcceleration->setToolTip("Accelerate Serial I/O operations for faster disk access");
    sioLayout->addWidget(m_sioAcceleration);
    
    tabLayout->addWidget(sioGroup);
    
    // 80-Column Cards Group
    QGroupBox* column80Group = new QGroupBox("80-Column Display Cards");
    QVBoxLayout* column80Layout = new QVBoxLayout(column80Group);
    
    m_xep80Enabled = new QCheckBox("Enable XEP80 Emulation");
    m_xep80Enabled->setToolTip("Enable Atari XEP80 80-column interface");
    column80Layout->addWidget(m_xep80Enabled);
    
    m_af80Enabled = new QCheckBox("Enable Austin Franklin 80-Column Board");
    m_af80Enabled->setToolTip("Enable Austin Franklin 80-column display board");
    column80Layout->addWidget(m_af80Enabled);
    
    m_bit3Enabled = new QCheckBox("Enable Bit3 Full View 80-Column Board");
    m_bit3Enabled->setToolTip("Enable Bit3 Full View 80-column display board");
    column80Layout->addWidget(m_bit3Enabled);
    
    tabLayout->addWidget(column80Group);
    
    // PBI Extensions Group
    QGroupBox* pbiGroup = new QGroupBox("PBI Extensions");
    QVBoxLayout* pbiLayout = new QVBoxLayout(pbiGroup);
    
    m_atari1400Enabled = new QCheckBox("Emulate Atari 1400XL");
    m_atari1400Enabled->setToolTip("Enable Atari 1400XL PBI system emulation");
    pbiLayout->addWidget(m_atari1400Enabled);
    
    m_atari1450Enabled = new QCheckBox("Emulate Atari 1450XLD");
    m_atari1450Enabled->setToolTip("Enable Atari 1450XLD system emulation");
    pbiLayout->addWidget(m_atari1450Enabled);
    
    m_proto80Enabled = new QCheckBox("Enable Prototype 80-Column Board");
    m_proto80Enabled->setToolTip("Enable prototype 80-column board for 1090");
    pbiLayout->addWidget(m_proto80Enabled);
    
    tabLayout->addWidget(pbiGroup);
    
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
    
    // Voice Synthesis Group
    QGroupBox* voiceGroup = new QGroupBox("Voice Synthesis");
    QVBoxLayout* voiceLayout = new QVBoxLayout(voiceGroup);
    
    m_voiceboxEnabled = new QCheckBox("Enable Voicebox Speech Synthesis");
    m_voiceboxEnabled->setToolTip("Enable Voicebox speech synthesis cartridge for text-to-speech");
    voiceLayout->addWidget(m_voiceboxEnabled);
    
    tabLayout->addWidget(voiceGroup);
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
    
    m_keepAspectRatio = new QCheckBox("Keep 4:3 Aspect Ratio");
    m_keepAspectRatio->setToolTip("Maintain authentic 4:3 display proportions when resizing window");
    generalLayout->addRow("", m_keepAspectRatio);
    
    m_fullscreenMode = new QCheckBox("Start in Fullscreen Mode");
    m_fullscreenMode->setToolTip("Launch application in fullscreen mode for immersive retro experience");
    generalLayout->addRow("", m_fullscreenMode);
    
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

void SettingsDialog::createMediaConfigTab()
{
    m_mediaTab = new QWidget();
    m_tabWidget->addTab(m_mediaTab, "Media Configuration");
    
    // Create main horizontal layout
    QHBoxLayout* mainLayout = new QHBoxLayout(m_mediaTab);
    
    // Left column - Floppy and Cassette
    QVBoxLayout* leftColumn = new QVBoxLayout();
    
    // Floppy Disks Group
    QGroupBox* floppyGroup = new QGroupBox("Floppy Disk Drives");
    QVBoxLayout* floppyLayout = new QVBoxLayout(floppyGroup);
    
    for (int i = 0; i < 4; i++) {
        QString diskLabel = QString("D%1:").arg(i + 1);
        
        QHBoxLayout* diskLayout = new QHBoxLayout();
        
        m_diskEnabled[i] = new QCheckBox(diskLabel);
        m_diskEnabled[i]->setMinimumWidth(40);
        diskLayout->addWidget(m_diskEnabled[i]);
        
        m_diskPath[i] = new QLineEdit();
        m_diskPath[i]->setPlaceholderText("Select disk image (.atr, .xfd, .dcm, .pro, .atx)");
        setupFilePathTooltip(m_diskPath[i]);
        diskLayout->addWidget(m_diskPath[i], 1);
        
        m_diskBrowse[i] = new QPushButton("Browse...");
        connect(m_diskBrowse[i], &QPushButton::clicked, [this, i]() { browseDiskImage(i); });
        diskLayout->addWidget(m_diskBrowse[i]);
        
        m_diskReadOnly[i] = new QCheckBox("Read-Only");
        diskLayout->addWidget(m_diskReadOnly[i]);
        
        floppyLayout->addLayout(diskLayout);
    }
    
    leftColumn->addWidget(floppyGroup);
    
    // Cassette Group
    QGroupBox* cassetteGroup = new QGroupBox("Cassette Tape");
    QVBoxLayout* cassetteLayout = new QVBoxLayout(cassetteGroup);
    
    QHBoxLayout* cassettePathLayout = new QHBoxLayout();
    m_cassetteEnabled = new QCheckBox("Enable Cassette");
    cassettePathLayout->addWidget(m_cassetteEnabled);
    
    m_cassettePath = new QLineEdit();
    m_cassettePath->setPlaceholderText("Select cassette image (.cas)");
    setupFilePathTooltip(m_cassettePath);
    cassettePathLayout->addWidget(m_cassettePath, 1);
    
    m_cassetteBrowse = new QPushButton("Browse...");
    connect(m_cassetteBrowse, &QPushButton::clicked, this, &SettingsDialog::browseCassetteImage);
    cassettePathLayout->addWidget(m_cassetteBrowse);
    
    cassetteLayout->addLayout(cassettePathLayout);
    
    QHBoxLayout* cassetteOptionsLayout = new QHBoxLayout();
    m_cassetteReadOnly = new QCheckBox("Read-Only");
    m_cassetteReadOnly->setToolTip("Mark cassette as read-only to prevent writes");
    cassetteOptionsLayout->addWidget(m_cassetteReadOnly);
    
    m_cassetteBootTape = new QCheckBox("Boot from Tape");
    m_cassetteBootTape->setToolTip("Automatically boot from cassette on startup");
    cassetteOptionsLayout->addWidget(m_cassetteBootTape);
    cassetteOptionsLayout->addStretch();
    
    cassetteLayout->addLayout(cassetteOptionsLayout);
    leftColumn->addWidget(cassetteGroup);
    leftColumn->addStretch();
    
    // Right column - Hard Drive and Special Devices
    QVBoxLayout* rightColumn = new QVBoxLayout();
    
    // Hard Drive Group
    QGroupBox* hdGroup = new QGroupBox("Hard Drive Emulation");
    QVBoxLayout* hdLayout = new QVBoxLayout(hdGroup);
    
    for (int i = 0; i < 4; i++) {
        QString hdLabel = QString("H%1:").arg(i + 1);
        
        QHBoxLayout* hdDriveLayout = new QHBoxLayout();
        
        m_hdEnabled[i] = new QCheckBox(hdLabel);
        m_hdEnabled[i]->setMinimumWidth(40);
        hdDriveLayout->addWidget(m_hdEnabled[i]);
        
        m_hdPath[i] = new QLineEdit();
        m_hdPath[i]->setPlaceholderText("Select directory path for hard drive emulation");
        setupFilePathTooltip(m_hdPath[i]);
        hdDriveLayout->addWidget(m_hdPath[i], 1);
        
        m_hdBrowse[i] = new QPushButton("Browse...");
        connect(m_hdBrowse[i], &QPushButton::clicked, [this, i]() { browseHardDriveDirectory(i); });
        hdDriveLayout->addWidget(m_hdBrowse[i]);
        
        hdLayout->addLayout(hdDriveLayout);
    }
    
    // Hard Drive Options
    QHBoxLayout* hdOptionsLayout = new QHBoxLayout();
    m_hdReadOnly = new QCheckBox("Read-Only Mode");
    m_hdReadOnly->setToolTip("Enable read-only mode for all H: devices");
    hdOptionsLayout->addWidget(m_hdReadOnly);
    
    QLabel* deviceNameLabel = new QLabel("Device Name:");
    hdOptionsLayout->addWidget(deviceNameLabel);
    m_hdDeviceName = new QLineEdit();
    m_hdDeviceName->setMaxLength(1);
    m_hdDeviceName->setPlaceholderText("H");
    m_hdDeviceName->setToolTip("Use this letter for host device instead of H:");
    m_hdDeviceName->setMaximumWidth(30);
    hdOptionsLayout->addWidget(m_hdDeviceName);
    hdOptionsLayout->addStretch();
    
    hdLayout->addLayout(hdOptionsLayout);
    rightColumn->addWidget(hdGroup);
    
    // Special Devices Group
    QGroupBox* specialGroup = new QGroupBox("Special Devices");
    QVBoxLayout* specialLayout = new QVBoxLayout(specialGroup);
    
    QHBoxLayout* rDeviceLayout = new QHBoxLayout();
    QLabel* rDeviceLabel = new QLabel("R: Device Configuration:");
    rDeviceLayout->addWidget(rDeviceLabel);
    
    QLabel* rDeviceNameLabel = new QLabel("Device Name:");
    rDeviceLayout->addWidget(rDeviceNameLabel);
    m_rDeviceName = new QLineEdit();
    m_rDeviceName->setPlaceholderText("R");
    m_rDeviceName->setMaxLength(1);
    m_rDeviceName->setMaximumWidth(30);
    m_rDeviceName->setToolTip("Custom device name for R: device");
    rDeviceLayout->addWidget(m_rDeviceName);
    rDeviceLayout->addStretch();
    
    specialLayout->addLayout(rDeviceLayout);
    
    m_netSIOEnabled = new QCheckBox("Enable NetSIO (FujiNet-PC Support)");
    m_netSIOEnabled->setToolTip("Enable NetSIO for FujiNet-PC network functionality");
    specialLayout->addWidget(m_netSIOEnabled);
    
    m_rtimeEnabled = new QCheckBox("Enable R-Time 8 Real-Time Clock");
    m_rtimeEnabled->setToolTip("Enable R-Time 8 cartridge emulation for real-time clock");
    specialLayout->addWidget(m_rtimeEnabled);
    
    rightColumn->addWidget(specialGroup);
    rightColumn->addStretch();
    
    // Add both columns to main layout
    mainLayout->addLayout(leftColumn, 1);
    mainLayout->addLayout(rightColumn, 1);
}

void SettingsDialog::browseDiskImage(int diskNumber)
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        QString("Select Disk Image for D%1:").arg(diskNumber + 1),
        QString(),
        "Disk Images (*.atr *.xfd *.dcm *.pro *.atx *.atr.gz *.xfd.gz);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        m_diskPath[diskNumber]->setText(fileName);
        m_diskEnabled[diskNumber]->setChecked(true);
    }
}

void SettingsDialog::browseCassetteImage()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Cassette Image",
        QString(),
        "Cassette Images (*.cas);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        m_cassettePath->setText(fileName);
        m_cassetteEnabled->setChecked(true);
    }
}

void SettingsDialog::browseHardDriveDirectory(int driveNumber)
{
    QString dirPath = QFileDialog::getExistingDirectory(
        this,
        QString("Select Directory for H%1:").arg(driveNumber + 1),
        QString()
    );
    
    if (!dirPath.isEmpty()) {
        m_hdPath[driveNumber]->setText(dirPath);
        m_hdEnabled[driveNumber]->setChecked(true);
    }
}

void SettingsDialog::browseOSROM()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select OS ROM File",
        QString(),
        "ROM Files (*.rom *.bin);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        m_osRomPath->setText(fileName);
    }
}

void SettingsDialog::browseBasicROM()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select BASIC ROM File",
        QString(),
        "ROM Files (*.rom *.bin);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        m_basicRomPath->setText(fileName);
    }
}

void SettingsDialog::onMachineTypeChanged()
{
    QString machineType = m_machineTypeCombo->currentData().toString();
    
    // Update OS ROM label based on machine type
    if (machineType == "-atari") {
        m_osRomLabel->setText("400/800 OS ROM:");
        m_osRomPath->setPlaceholderText("Select 400/800 OS ROM file (OSA or OSB)");
    } else if (machineType == "-5200") {
        m_osRomLabel->setText("5200 BIOS ROM:");
        m_osRomPath->setPlaceholderText("Select 5200 BIOS ROM file");
    } else {
        // XL/XE variants (1200XL, 800XL, 130XE, 320XE, etc.)
        m_osRomLabel->setText("XL/XE OS ROM:");
        m_osRomPath->setPlaceholderText("Select XL/XE OS ROM file");
    }
    
    // Load the appropriate ROM path from settings
    QSettings settings("8bitrelics", "Fujisan");
    QString osRomKey = QString("machine/osRom_%1").arg(machineType.mid(1)); // Remove the '-' prefix
    m_osRomPath->setText(settings.value(osRomKey, "").toString());
}

void SettingsDialog::onAltirraOSChanged()
{
    bool altirraOSEnabled = m_altirraOSCheck->isChecked();
    
    // Disable BASIC ROM controls when Altirra OS is enabled
    // (since Altirra OS includes its own BASIC)
    m_basicRomLabel->setEnabled(!altirraOSEnabled);
    m_basicRomPath->setEnabled(!altirraOSEnabled);
    m_basicRomBrowse->setEnabled(!altirraOSEnabled);
}

void SettingsDialog::setupFilePathTooltip(QLineEdit* lineEdit)
{
    // Connect to textChanged signal to update tooltip with full path
    connect(lineEdit, &QLineEdit::textChanged, [lineEdit](const QString& text) {
        if (text.isEmpty()) {
            lineEdit->setToolTip("No file selected");
        } else {
            lineEdit->setToolTip(text); // Show full path in tooltip
        }
    });
    
    // Set initial tooltip
    if (lineEdit->text().isEmpty()) {
        lineEdit->setToolTip("No file selected");
    } else {
        lineEdit->setToolTip(lineEdit->text());
    }
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
    
    // Load ROM paths
    QString currentMachineType = m_machineTypeCombo->currentData().toString();
    QString osRomKey = QString("machine/osRom_%1").arg(currentMachineType.mid(1)); // Remove the '-' prefix
    m_osRomPath->setText(settings.value(osRomKey, "").toString());
    m_basicRomPath->setText(settings.value("machine/basicRom", "").toString());
    
    // Update ROM labels based on machine type
    onMachineTypeChanged();
    
    // Update BASIC ROM controls based on Altirra OS setting
    onAltirraOSChanged();
    
    // Load Hardware Extensions
    m_stereoPokey->setChecked(settings.value("hardware/stereoPokey", false).toBool());
    m_sioAcceleration->setChecked(settings.value("hardware/sioAcceleration", true).toBool());
    
    // 80-Column Cards
    m_xep80Enabled->setChecked(settings.value("hardware/xep80", false).toBool());
    m_af80Enabled->setChecked(settings.value("hardware/af80", false).toBool());
    m_bit3Enabled->setChecked(settings.value("hardware/bit3", false).toBool());
    
    // PBI Extensions
    m_atari1400Enabled->setChecked(settings.value("hardware/atari1400", false).toBool());
    m_atari1450Enabled->setChecked(settings.value("hardware/atari1450", false).toBool());
    m_proto80Enabled->setChecked(settings.value("hardware/proto80", false).toBool());
    
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
    
    // Voice Synthesis
    m_voiceboxEnabled->setChecked(settings.value("audio/voicebox", false).toBool());
    
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
    m_keepAspectRatio->setChecked(settings.value("video/keepAspectRatio", true).toBool());
    m_fullscreenMode->setChecked(settings.value("video/fullscreenMode", false).toBool());
    
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
    
    // Load Media Configuration
    // Floppy Disks
    for (int i = 0; i < 4; i++) {
        QString diskKey = QString("media/disk%1").arg(i + 1);
        m_diskEnabled[i]->setChecked(settings.value(diskKey + "Enabled", false).toBool());
        m_diskPath[i]->setText(settings.value(diskKey + "Path", "").toString());
        m_diskReadOnly[i]->setChecked(settings.value(diskKey + "ReadOnly", false).toBool());
    }
    
    // Cassette
    m_cassetteEnabled->setChecked(settings.value("media/cassetteEnabled", false).toBool());
    m_cassettePath->setText(settings.value("media/cassettePath", "").toString());
    m_cassetteReadOnly->setChecked(settings.value("media/cassetteReadOnly", false).toBool());
    m_cassetteBootTape->setChecked(settings.value("media/cassetteBootTape", false).toBool());
    
    // Hard Drives
    for (int i = 0; i < 4; i++) {
        QString hdKey = QString("media/hd%1").arg(i + 1);
        m_hdEnabled[i]->setChecked(settings.value(hdKey + "Enabled", false).toBool());
        m_hdPath[i]->setText(settings.value(hdKey + "Path", "").toString());
    }
    m_hdReadOnly->setChecked(settings.value("media/hdReadOnly", false).toBool());
    m_hdDeviceName->setText(settings.value("media/hdDeviceName", "H").toString());
    
    // Special Devices (R: device is already loaded in Hardware Extensions)
    m_rDeviceName->setText(settings.value("media/rDeviceName", "R").toString());
    m_netSIOEnabled->setChecked(settings.value("media/netSIOEnabled", false).toBool());
    m_rtimeEnabled->setChecked(settings.value("media/rtimeEnabled", false).toBool());
    
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
    
    // Save ROM paths
    QString osRomKey = QString("machine/osRom_%1").arg(machineType.mid(1)); // Remove the '-' prefix
    settings.setValue(osRomKey, m_osRomPath->text());
    settings.setValue("machine/basicRom", m_basicRomPath->text());
    
    // Save Hardware Extensions
    settings.setValue("hardware/stereoPokey", m_stereoPokey->isChecked());
    settings.setValue("hardware/sioAcceleration", m_sioAcceleration->isChecked());
    
    // 80-Column Cards
    settings.setValue("hardware/xep80", m_xep80Enabled->isChecked());
    settings.setValue("hardware/af80", m_af80Enabled->isChecked());
    settings.setValue("hardware/bit3", m_bit3Enabled->isChecked());
    
    // PBI Extensions
    settings.setValue("hardware/atari1400", m_atari1400Enabled->isChecked());
    settings.setValue("hardware/atari1450", m_atari1450Enabled->isChecked());
    settings.setValue("hardware/proto80", m_proto80Enabled->isChecked());
    
    // Save Audio Configuration
    settings.setValue("audio/enabled", m_soundEnabled->isChecked());
    settings.setValue("audio/frequency", m_audioFrequency->currentData().toInt());
    settings.setValue("audio/bits", m_audioBits->currentData().toInt());
    settings.setValue("audio/consoleSound", m_consoleSound->isChecked());
    settings.setValue("audio/serialSound", m_serialSound->isChecked());
    
    // Voice Synthesis
    settings.setValue("audio/voicebox", m_voiceboxEnabled->isChecked());
    
    // Save Video and Display
    settings.setValue("video/artifacting", m_artifactingMode->currentData().toString());
    settings.setValue("video/showFPS", m_showFPS->isChecked());
    settings.setValue("video/scalingFilter", m_scalingFilter->isChecked());
    settings.setValue("video/keepAspectRatio", m_keepAspectRatio->isChecked());
    settings.setValue("video/fullscreenMode", m_fullscreenMode->isChecked());
    settings.setValue("video/palBlending", m_palBlending->currentData().toString());
    settings.setValue("video/palScanlines", m_palScanlines->isChecked());
    settings.setValue("video/ntscArtifacting", m_ntscArtifacting->currentData().toString());
    settings.setValue("video/ntscSharpness", m_ntscSharpness->isChecked());
    
    // Save Media Configuration
    // Floppy Disks
    for (int i = 0; i < 4; i++) {
        QString diskKey = QString("media/disk%1").arg(i + 1);
        settings.setValue(diskKey + "Enabled", m_diskEnabled[i]->isChecked());
        settings.setValue(diskKey + "Path", m_diskPath[i]->text());
        settings.setValue(diskKey + "ReadOnly", m_diskReadOnly[i]->isChecked());
    }
    
    // Cassette
    settings.setValue("media/cassetteEnabled", m_cassetteEnabled->isChecked());
    settings.setValue("media/cassettePath", m_cassettePath->text());
    settings.setValue("media/cassetteReadOnly", m_cassetteReadOnly->isChecked());
    settings.setValue("media/cassetteBootTape", m_cassetteBootTape->isChecked());
    
    // Hard Drives
    for (int i = 0; i < 4; i++) {
        QString hdKey = QString("media/hd%1").arg(i + 1);
        settings.setValue(hdKey + "Enabled", m_hdEnabled[i]->isChecked());
        settings.setValue(hdKey + "Path", m_hdPath[i]->text());
    }
    settings.setValue("media/hdReadOnly", m_hdReadOnly->isChecked());
    settings.setValue("media/hdDeviceName", m_hdDeviceName->text());
    
    // Special Devices (R: device enabled state is saved in Hardware Extensions)
    settings.setValue("media/rDeviceName", m_rDeviceName->text());
    settings.setValue("media/netSIOEnabled", m_netSIOEnabled->isChecked());
    settings.setValue("media/rtimeEnabled", m_rtimeEnabled->isChecked());
    
    qDebug() << "Settings saved to persistent storage - Machine:" << machineType 
             << "Video:" << videoSystem << "BASIC:" << basicEnabled;
    
    // Apply settings to emulator
    m_emulator->setMachineType(machineType);
    m_emulator->setVideoSystem(videoSystem);
    m_emulator->setBasicEnabled(basicEnabled);
    m_emulator->setAltirraOSEnabled(altirraOSEnabled);
    m_emulator->enableAudio(m_soundEnabled->isChecked());
    
    // Apply media configuration to emulator
    applyMediaSettings();
}

void SettingsDialog::applyMediaSettings()
{
    qDebug() << "Applying media settings to emulator...";
    
    // Mount/unmount disk images for D1-D4
    for (int i = 0; i < 4; i++) {
        if (m_diskEnabled[i]->isChecked() && !m_diskPath[i]->text().isEmpty()) {
            QString diskPath = m_diskPath[i]->text();
            bool readOnly = m_diskReadOnly[i]->isChecked();
            
            qDebug() << QString("Mounting D%1: %2 (read-only: %3)")
                        .arg(i + 1).arg(diskPath).arg(readOnly);
            
            if (m_emulator->mountDiskImage(i + 1, diskPath, readOnly)) {
                qDebug() << QString("Successfully mounted D%1:").arg(i + 1);
            } else {
                qDebug() << QString("Failed to mount D%1:").arg(i + 1);
            }
        } else {
            // TODO: Unmount disk if it was previously mounted
            // We might need to add an unmount function to AtariEmulator
            qDebug() << QString("D%1: disabled or no path specified").arg(i + 1);
        }
    }
    
    // TODO: Apply cassette settings
    if (m_cassetteEnabled->isChecked() && !m_cassettePath->text().isEmpty()) {
        qDebug() << "Cassette support not yet implemented in emulator backend";
    }
    
    // TODO: Apply hard drive settings
    for (int i = 0; i < 4; i++) {
        if (m_hdEnabled[i]->isChecked() && !m_hdPath[i]->text().isEmpty()) {
            qDebug() << QString("H%1: hard drive support not yet implemented").arg(i + 1);
        }
    }
    
    qDebug() << "Media settings applied";
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
        
        // Reapply media settings after restart
        applyMediaSettings();
        
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
    m_machineTypeCombo->setCurrentIndex(2); // Atari 800XL (updated index due to new machines)
    m_videoSystemCombo->setCurrentIndex(0); // PAL
    m_basicEnabledCheck->setChecked(true);   // BASIC enabled
    m_altirraOSCheck->setChecked(false);     // Use external ROMs
    
    // ROM Configuration defaults
    m_osRomPath->clear();
    m_basicRomPath->clear();
    onMachineTypeChanged(); // Update ROM labels
    onAltirraOSChanged(); // Update BASIC ROM controls
    
    // Hardware Extensions defaults
    m_stereoPokey->setChecked(false);
    m_sioAcceleration->setChecked(true);
    
    // 80-Column Cards defaults
    m_xep80Enabled->setChecked(false);
    m_af80Enabled->setChecked(false);
    m_bit3Enabled->setChecked(false);
    
    // PBI Extensions defaults
    m_atari1400Enabled->setChecked(false);
    m_atari1450Enabled->setChecked(false);
    m_proto80Enabled->setChecked(false);
    
    // Audio Configuration defaults
    m_soundEnabled->setChecked(true);
    m_audioFrequency->setCurrentIndex(1); // 44100 Hz
    m_audioBits->setCurrentIndex(1);       // 16-bit
    m_consoleSound->setChecked(true);
    m_serialSound->setChecked(false);
    
    // Voice Synthesis defaults
    m_voiceboxEnabled->setChecked(false);
    
    // Video and Display defaults
    m_artifactingMode->setCurrentIndex(0); // None
    m_showFPS->setChecked(false);
    m_scalingFilter->setChecked(true);
    m_keepAspectRatio->setChecked(true);
    m_fullscreenMode->setChecked(false);
    m_palBlending->setCurrentIndex(1);     // Simple
    m_palScanlines->setChecked(false);
    m_ntscArtifacting->setCurrentIndex(0); // Standard
    m_ntscSharpness->setChecked(true);
    
    // Media Configuration defaults
    // Floppy Disks - all disabled by default
    for (int i = 0; i < 4; i++) {
        m_diskEnabled[i]->setChecked(false);
        m_diskPath[i]->clear();
        m_diskReadOnly[i]->setChecked(false);
    }
    
    // Cassette - disabled by default
    m_cassetteEnabled->setChecked(false);
    m_cassettePath->clear();
    m_cassetteReadOnly->setChecked(false);
    m_cassetteBootTape->setChecked(false);
    
    // Hard Drives - all disabled by default
    for (int i = 0; i < 4; i++) {
        m_hdEnabled[i]->setChecked(false);
        m_hdPath[i]->clear();
    }
    m_hdReadOnly->setChecked(false);
    m_hdDeviceName->setText("H");
    
    // Special Devices - all disabled by default (R: device is handled in Hardware Extensions)
    m_rDeviceName->setText("R");
    m_netSIOEnabled->setChecked(false);
    m_rtimeEnabled->setChecked(false);
    
    // Update PAL/NTSC dependent controls
    updateVideoSystemDependentControls();
}