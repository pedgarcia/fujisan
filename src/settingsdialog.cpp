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
    
    // Create main horizontal layout for two columns
    QHBoxLayout* mainLayout = new QHBoxLayout(m_machineTab);
    
    // Left column layout
    QVBoxLayout* leftColumn = new QVBoxLayout();
    
    // Right column layout  
    QVBoxLayout* rightColumn = new QVBoxLayout();
    
    // Machine Type Group
    QGroupBox* machineGroup = new QGroupBox("Machine Type");
    QVBoxLayout* machineVLayout = new QVBoxLayout(machineGroup);
    
    // Machine model selection
    QLabel* modelLabel = new QLabel("Model:");
    machineVLayout->addWidget(modelLabel);
    
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
    
    machineVLayout->addWidget(m_machineTypeCombo);
    
    // OS ROM selector below machine dropdown
    m_osRomLabel = new QLabel("OS ROM:");
    machineVLayout->addWidget(m_osRomLabel);
    
    QHBoxLayout* osRomLayout = new QHBoxLayout();
    m_osRomPath = new QLineEdit();
    m_osRomPath->setPlaceholderText("Select OS ROM file");
    setupFilePathTooltip(m_osRomPath);
    osRomLayout->addWidget(m_osRomPath, 1);
    
    m_osRomBrowse = new QPushButton("Browse...");
    connect(m_osRomBrowse, &QPushButton::clicked, this, &SettingsDialog::browseOSROM);
    osRomLayout->addWidget(m_osRomBrowse);
    
    machineVLayout->addLayout(osRomLayout);
    leftColumn->addWidget(machineGroup);
    
    // Video System Group
    QGroupBox* videoGroup = new QGroupBox("Video System");
    QFormLayout* videoLayout = new QFormLayout(videoGroup);
    
    m_videoSystemCombo = new QComboBox();
    m_videoSystemCombo->addItem("PAL (49.86 fps)", "-pal");
    m_videoSystemCombo->addItem("NTSC (59.92 fps)", "-ntsc");
    
    videoLayout->addRow("Standard:", m_videoSystemCombo);
    leftColumn->addWidget(videoGroup);
    
    // System Options Group
    QGroupBox* systemGroup = new QGroupBox("System Options");
    QVBoxLayout* systemLayout = new QVBoxLayout(systemGroup);
    
    // BASIC configuration
    m_basicEnabledCheck = new QCheckBox("Enable BASIC");
    m_basicEnabledCheck->setToolTip("Enable or disable the Atari BASIC interpreter");
    systemLayout->addWidget(m_basicEnabledCheck);
    
    // BASIC ROM selector below checkbox
    m_basicRomLabel = new QLabel("BASIC ROM:");
    systemLayout->addWidget(m_basicRomLabel);
    
    QHBoxLayout* basicRomLayout = new QHBoxLayout();
    m_basicRomPath = new QLineEdit();
    m_basicRomPath->setPlaceholderText("Select BASIC ROM file");
    setupFilePathTooltip(m_basicRomPath);
    basicRomLayout->addWidget(m_basicRomPath, 1);
    
    m_basicRomBrowse = new QPushButton("Browse...");
    connect(m_basicRomBrowse, &QPushButton::clicked, this, &SettingsDialog::browseBasicROM);
    basicRomLayout->addWidget(m_basicRomBrowse);
    
    systemLayout->addLayout(basicRomLayout);
    
    m_altirraOSCheck = new QCheckBox("Use Altirra OS (built-in ROMs)");
    m_altirraOSCheck->setToolTip("Use built-in Altirra OS ROMs instead of external ROM files");
    connect(m_altirraOSCheck, &QCheckBox::toggled, this, &SettingsDialog::onAltirraOSChanged);
    systemLayout->addWidget(m_altirraOSCheck);
    
    leftColumn->addWidget(systemGroup);
    
    // Memory Configuration Group
    QGroupBox* memoryGroup = new QGroupBox("Memory Configuration");
    QVBoxLayout* memoryLayout = new QVBoxLayout(memoryGroup);
    
    m_enable800RamCheck = new QCheckBox("Enable RAM at 0xC000-0xCFFF (Atari 800)");
    m_enable800RamCheck->setToolTip("Enable RAM between 0xC000-0xCFFF in Atari 800");
    memoryLayout->addWidget(m_enable800RamCheck);
    
    // Mosaic RAM expansion
    QHBoxLayout* mosaicLayout = new QHBoxLayout();
    m_enableMosaicCheck = new QCheckBox("Enable Mosaic RAM expansion:");
    mosaicLayout->addWidget(m_enableMosaicCheck);
    
    m_mosaicSizeSpinBox = new QSpinBox();
    m_mosaicSizeSpinBox->setRange(64, 1024);
    m_mosaicSizeSpinBox->setSingleStep(64);
    m_mosaicSizeSpinBox->setSuffix(" KB");
    m_mosaicSizeSpinBox->setValue(320);
    m_mosaicSizeSpinBox->setToolTip("Total RAM size with Mosaic expansion");
    mosaicLayout->addWidget(m_mosaicSizeSpinBox);
    mosaicLayout->addStretch();
    
    connect(m_enableMosaicCheck, &QCheckBox::toggled, m_mosaicSizeSpinBox, &QSpinBox::setEnabled);
    m_mosaicSizeSpinBox->setEnabled(false);
    
    memoryLayout->addLayout(mosaicLayout);
    
    // Axlon RAM expansion
    QHBoxLayout* axlonLayout = new QHBoxLayout();
    m_enableAxlonCheck = new QCheckBox("Enable Axlon RAM expansion:");
    axlonLayout->addWidget(m_enableAxlonCheck);
    
    m_axlonSizeSpinBox = new QSpinBox();
    m_axlonSizeSpinBox->setRange(64, 1024);
    m_axlonSizeSpinBox->setSingleStep(64);
    m_axlonSizeSpinBox->setSuffix(" KB");
    m_axlonSizeSpinBox->setValue(320);
    m_axlonSizeSpinBox->setToolTip("Total RAM size with Axlon expansion");
    axlonLayout->addWidget(m_axlonSizeSpinBox);
    axlonLayout->addStretch();
    
    connect(m_enableAxlonCheck, &QCheckBox::toggled, m_axlonSizeSpinBox, &QSpinBox::setEnabled);
    m_axlonSizeSpinBox->setEnabled(false);
    
    memoryLayout->addLayout(axlonLayout);
    
    m_axlonShadowCheck = new QCheckBox("Use Axlon shadow at 0x0FC0-0x0FFF");
    m_axlonShadowCheck->setToolTip("Enable Axlon shadow memory at 0x0FC0-0x0FFF");
    memoryLayout->addWidget(m_axlonShadowCheck);
    
    m_enableMapRamCheck = new QCheckBox("Enable MapRAM (XL/XE machines)");
    m_enableMapRamCheck->setToolTip("Enable MapRAM feature for XL/XE machines");
    memoryLayout->addWidget(m_enableMapRamCheck);
    
    rightColumn->addWidget(memoryGroup);
    
    // Performance Group
    QGroupBox* performanceGroup = new QGroupBox("Performance");
    QVBoxLayout* performanceLayout = new QVBoxLayout(performanceGroup);
    
    m_turboModeCheck = new QCheckBox("Run as fast as possible (Turbo mode)");
    m_turboModeCheck->setToolTip("Run emulator at maximum speed, ignoring timing constraints");
    performanceLayout->addWidget(m_turboModeCheck);
    
    rightColumn->addWidget(performanceGroup);
    
    // Cartridge Configuration Group
    QGroupBox* cartridgeGroup = new QGroupBox("Cartridge Configuration");
    QVBoxLayout* cartridgeLayout = new QVBoxLayout(cartridgeGroup);
    
    // Primary cartridge
    QVBoxLayout* cart1Layout = new QVBoxLayout();
    QLabel* cart1Label = new QLabel("Primary Cartridge:");
    cart1Label->setStyleSheet("font-weight: bold;");
    cart1Layout->addWidget(cart1Label);
    
    QHBoxLayout* cart1PathLayout = new QHBoxLayout();
    m_cartridgeEnabledCheck = new QCheckBox("Enable");
    cart1PathLayout->addWidget(m_cartridgeEnabledCheck);
    
    m_cartridgePath = new QLineEdit();
    m_cartridgePath->setPlaceholderText("Select cartridge file (.rom, .bin, .car)");
    setupFilePathTooltip(m_cartridgePath);
    cart1PathLayout->addWidget(m_cartridgePath, 1);
    
    m_cartridgeBrowse = new QPushButton("Browse...");
    connect(m_cartridgeBrowse, &QPushButton::clicked, this, &SettingsDialog::browseCartridge);
    cart1PathLayout->addWidget(m_cartridgeBrowse);
    
    cart1Layout->addLayout(cart1PathLayout);
    
    QHBoxLayout* cart1TypeLayout = new QHBoxLayout();
    QLabel* cart1TypeLabel = new QLabel("Type:");
    cart1TypeLayout->addWidget(cart1TypeLabel);
    
    m_cartridgeTypeCombo = new QComboBox();
    m_cartridgeTypeCombo->setToolTip("Cartridge type - use Auto-detect unless you have issues");
    populateCartridgeTypes(m_cartridgeTypeCombo);
    cart1TypeLayout->addWidget(m_cartridgeTypeCombo, 1);
    
    cart1Layout->addLayout(cart1TypeLayout);
    cartridgeLayout->addLayout(cart1Layout);
    
    // Piggyback cartridge
    QVBoxLayout* cart2Layout = new QVBoxLayout();
    QLabel* cart2Label = new QLabel("Piggyback Cartridge:");
    cart2Label->setStyleSheet("font-weight: bold;");
    cart2Layout->addWidget(cart2Label);
    
    QHBoxLayout* cart2PathLayout = new QHBoxLayout();
    m_cartridge2EnabledCheck = new QCheckBox("Enable");
    cart2PathLayout->addWidget(m_cartridge2EnabledCheck);
    
    m_cartridge2Path = new QLineEdit();
    m_cartridge2Path->setPlaceholderText("Select piggyback cartridge file");
    setupFilePathTooltip(m_cartridge2Path);
    cart2PathLayout->addWidget(m_cartridge2Path, 1);
    
    m_cartridge2Browse = new QPushButton("Browse...");
    connect(m_cartridge2Browse, &QPushButton::clicked, this, &SettingsDialog::browseCartridge2);
    cart2PathLayout->addWidget(m_cartridge2Browse);
    
    cart2Layout->addLayout(cart2PathLayout);
    
    QHBoxLayout* cart2TypeLayout = new QHBoxLayout();
    QLabel* cart2TypeLabel = new QLabel("Type:");
    cart2TypeLayout->addWidget(cart2TypeLabel);
    
    m_cartridge2TypeCombo = new QComboBox();
    m_cartridge2TypeCombo->setToolTip("Piggyback cartridge type");
    populateCartridgeTypes(m_cartridge2TypeCombo);
    cart2TypeLayout->addWidget(m_cartridge2TypeCombo, 1);
    
    cart2Layout->addLayout(cart2TypeLayout);
    cartridgeLayout->addLayout(cart2Layout);
    
    // Cartridge options
    m_cartridgeAutoRebootCheck = new QCheckBox("Auto-reboot when cartridge changes");
    m_cartridgeAutoRebootCheck->setToolTip("Automatically restart emulator when cartridges are inserted or removed");
    m_cartridgeAutoRebootCheck->setChecked(true); // Default to enabled
    cartridgeLayout->addWidget(m_cartridgeAutoRebootCheck);
    
    // Enable/disable controls based on checkboxes
    connect(m_cartridgeEnabledCheck, &QCheckBox::toggled, [this](bool enabled) {
        m_cartridgePath->setEnabled(enabled);
        m_cartridgeBrowse->setEnabled(enabled);
        m_cartridgeTypeCombo->setEnabled(enabled);
    });
    
    connect(m_cartridge2EnabledCheck, &QCheckBox::toggled, [this](bool enabled) {
        m_cartridge2Path->setEnabled(enabled);
        m_cartridge2Browse->setEnabled(enabled);
        m_cartridge2TypeCombo->setEnabled(enabled);
    });
    
    // Initially disable controls
    m_cartridgePath->setEnabled(false);
    m_cartridgeBrowse->setEnabled(false);
    m_cartridgeTypeCombo->setEnabled(false);
    m_cartridge2Path->setEnabled(false);
    m_cartridge2Browse->setEnabled(false);
    m_cartridge2TypeCombo->setEnabled(false);
    
    rightColumn->addWidget(cartridgeGroup);
    
    // Add stretch to balance columns
    leftColumn->addStretch();
    rightColumn->addStretch();
    
    // Add both columns to main layout
    mainLayout->addLayout(leftColumn, 1);
    mainLayout->addLayout(rightColumn, 1);
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
    
    // Volume control
    QHBoxLayout* volumeLayout = new QHBoxLayout();
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setToolTip("Audio volume level (0-100%)");
    volumeLayout->addWidget(m_volumeSlider, 1);
    
    m_volumeLabel = new QLabel("80%");
    m_volumeLabel->setMinimumWidth(40);
    m_volumeLabel->setAlignment(Qt::AlignCenter);
    volumeLayout->addWidget(m_volumeLabel);
    
    connect(m_volumeSlider, &QSlider::valueChanged, [this](int value) {
        m_volumeLabel->setText(QString("%1%").arg(value));
    });
    
    audioLayout->addRow("Volume:", volumeLayout);
    
    // Buffer length
    m_bufferLengthSpinBox = new QSpinBox();
    m_bufferLengthSpinBox->setRange(10, 500);
    m_bufferLengthSpinBox->setValue(100);
    m_bufferLengthSpinBox->setSuffix(" ms");
    m_bufferLengthSpinBox->setToolTip("Audio buffer length in milliseconds - lower values reduce latency but may cause audio dropouts");
    audioLayout->addRow("Buffer Length:", m_bufferLengthSpinBox);
    
    // Audio latency
    m_audioLatencySpinBox = new QSpinBox();
    m_audioLatencySpinBox->setRange(0, 200);
    m_audioLatencySpinBox->setValue(20);
    m_audioLatencySpinBox->setSuffix(" ms");
    m_audioLatencySpinBox->setToolTip("Additional audio latency for synchronization - adjust if experiencing audio/video sync issues");
    audioLayout->addRow("Audio Delay:", m_audioLatencySpinBox);
    
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

void SettingsDialog::browseCartridge()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Cartridge File",
        QString(),
        "Cartridge Files (*.rom *.bin *.car *.a52);;ROM Files (*.rom *.bin);;CART Files (*.car);;Atari 5200 (*.a52);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        m_cartridgePath->setText(fileName);
        m_cartridgeEnabledCheck->setChecked(true);
    }
}

void SettingsDialog::browseCartridge2()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Piggyback Cartridge File",
        QString(),
        "Cartridge Files (*.rom *.bin *.car *.a52);;ROM Files (*.rom *.bin);;CART Files (*.car);;Atari 5200 (*.a52);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        m_cartridge2Path->setText(fileName);
        m_cartridge2EnabledCheck->setChecked(true);
    }
}

void SettingsDialog::populateCartridgeTypes(QComboBox* combo)
{
    // Add auto-detect first (most common choice)
    combo->addItem("Auto-detect", -1);
    
    // Add most common cartridge types based on Atari800 manual
    combo->addItem("0 - No cartridge", 0);
    combo->addItem("1 - Standard 8K", 1);
    combo->addItem("2 - Standard 16K", 2);
    combo->addItem("3 - OSS 034M 16K", 3);
    combo->addItem("4 - 5200 32K", 4);
    combo->addItem("5 - DB 32K", 5);
    combo->addItem("6 - Two chip 16K", 6);
    combo->addItem("7 - Bounty Bob 40K", 7);
    combo->addItem("8 - 64K Williams", 8);
    combo->addItem("9 - Express 64K", 9);
    combo->addItem("10 - Diamond 64K", 10);
    combo->addItem("11 - SpartaDOS X 64K", 11);
    combo->addItem("12 - XEGS 32K", 12);
    combo->addItem("13 - XEGS 64K", 13);
    combo->addItem("14 - XEGS 128K", 14);
    combo->addItem("15 - OSS 043M 16K", 15);
    combo->addItem("16 - One chip 16K", 16);
    combo->addItem("17 - Atrax 128K", 17);
    combo->addItem("18 - Bounty Bob 40K", 18);
    combo->addItem("19 - 5200 8K", 19);
    combo->addItem("20 - 5200 4K", 20);
    combo->addItem("21 - Right slot 8K", 21);
    combo->addItem("22 - 32K Williams", 22);
    combo->addItem("23 - XEGS 256K", 23);
    combo->addItem("24 - XEGS 512K", 24);
    combo->addItem("25 - XEGS 1024K", 25);
    combo->addItem("26 - MegaCart 16K", 26);
    combo->addItem("27 - MegaCart 32K", 27);
    combo->addItem("28 - MegaCart 64K", 28);
    combo->addItem("29 - MegaCart 128K", 29);
    combo->addItem("30 - MegaCart 256K", 30);
    combo->addItem("31 - MegaCart 512K", 31);
    combo->addItem("32 - MegaCart 1024K", 32);
    combo->addItem("33 - Switchable XEGS 32K", 33);
    combo->addItem("34 - Switchable XEGS 64K", 34);
    combo->addItem("35 - Switchable XEGS 128K", 35);
    combo->addItem("36 - Switchable XEGS 256K", 36);
    combo->addItem("37 - Switchable XEGS 512K", 37);
    combo->addItem("38 - Switchable XEGS 1024K", 38);
    combo->addItem("39 - Phoenix 8K", 39);
    combo->addItem("40 - Blizzard 16K", 40);
    combo->addItem("41 - Atarimax 128K Flash", 41);
    combo->addItem("42 - Atarimax 1024K Flash", 42);
    combo->addItem("43 - SpartaDOS X 128K", 43);
    combo->addItem("44 - OSS 8K", 44);
    combo->addItem("45 - OSS Two chip 16K", 45);
    combo->addItem("46 - Blizzard 4K", 46);
    combo->addItem("47 - AST 32K", 47);
    combo->addItem("48 - Atrax SDX 64K", 48);
    combo->addItem("49 - Atrax SDX 128K", 49);
    combo->addItem("50 - Turbosoft 64K", 50);
    combo->addItem("51 - Turbosoft 128K", 51);
    combo->addItem("52 - Ultracart 32K", 52);
    combo->addItem("53 - Low bank 8K", 53);
    combo->addItem("54 - SIC! 128K", 54);
    combo->addItem("55 - SIC! 256K", 55);
    combo->addItem("56 - SIC! 512K", 56);
    combo->addItem("57 - Standard 2K", 57);
    combo->addItem("58 - Standard 4K", 58);
    combo->addItem("59 - Right slot 4K", 59);
    combo->addItem("60 - Blizzard 32K", 60);
    
    // Set default to auto-detect
    combo->setCurrentIndex(0);
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
    
    // Load Memory Configuration
    m_enable800RamCheck->setChecked(settings.value("machine/enable800Ram", false).toBool());
    m_enableMosaicCheck->setChecked(settings.value("machine/enableMosaic", false).toBool());
    m_mosaicSizeSpinBox->setValue(settings.value("machine/mosaicSize", 320).toInt());
    m_enableAxlonCheck->setChecked(settings.value("machine/enableAxlon", false).toBool());
    m_axlonSizeSpinBox->setValue(settings.value("machine/axlonSize", 320).toInt());
    m_axlonShadowCheck->setChecked(settings.value("machine/axlonShadow", false).toBool());
    m_enableMapRamCheck->setChecked(settings.value("machine/enableMapRam", false).toBool());
    
    // Load Performance settings
    m_turboModeCheck->setChecked(settings.value("machine/turboMode", false).toBool());
    
    // Load Cartridge Configuration
    m_cartridgeEnabledCheck->setChecked(settings.value("machine/cartridgeEnabled", false).toBool());
    m_cartridgePath->setText(settings.value("machine/cartridgePath", "").toString());
    int cartridgeType = settings.value("machine/cartridgeType", -1).toInt();
    for (int i = 0; i < m_cartridgeTypeCombo->count(); ++i) {
        if (m_cartridgeTypeCombo->itemData(i).toInt() == cartridgeType) {
            m_cartridgeTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_cartridge2EnabledCheck->setChecked(settings.value("machine/cartridge2Enabled", false).toBool());
    m_cartridge2Path->setText(settings.value("machine/cartridge2Path", "").toString());
    int cartridge2Type = settings.value("machine/cartridge2Type", -1).toInt();
    for (int i = 0; i < m_cartridge2TypeCombo->count(); ++i) {
        if (m_cartridge2TypeCombo->itemData(i).toInt() == cartridge2Type) {
            m_cartridge2TypeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_cartridgeAutoRebootCheck->setChecked(settings.value("machine/cartridgeAutoReboot", true).toBool());
    
    // Update spinbox enabled state based on checkbox values
    m_mosaicSizeSpinBox->setEnabled(m_enableMosaicCheck->isChecked());
    m_axlonSizeSpinBox->setEnabled(m_enableAxlonCheck->isChecked());
    
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
    
    // Load volume control
    int volume = settings.value("audio/volume", 80).toInt();
    m_volumeSlider->setValue(volume);
    m_volumeLabel->setText(QString("%1%").arg(volume));
    
    // Load buffer settings
    m_bufferLengthSpinBox->setValue(settings.value("audio/bufferLength", 100).toInt());
    m_audioLatencySpinBox->setValue(settings.value("audio/latency", 20).toInt());
    
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
    
    // Save Memory Configuration
    settings.setValue("machine/enable800Ram", m_enable800RamCheck->isChecked());
    settings.setValue("machine/enableMosaic", m_enableMosaicCheck->isChecked());
    settings.setValue("machine/mosaicSize", m_mosaicSizeSpinBox->value());
    settings.setValue("machine/enableAxlon", m_enableAxlonCheck->isChecked());
    settings.setValue("machine/axlonSize", m_axlonSizeSpinBox->value());
    settings.setValue("machine/axlonShadow", m_axlonShadowCheck->isChecked());
    settings.setValue("machine/enableMapRam", m_enableMapRamCheck->isChecked());
    
    // Save Performance settings
    settings.setValue("machine/turboMode", m_turboModeCheck->isChecked());
    
    // Save Cartridge Configuration
    settings.setValue("machine/cartridgeEnabled", m_cartridgeEnabledCheck->isChecked());
    settings.setValue("machine/cartridgePath", m_cartridgePath->text());
    settings.setValue("machine/cartridgeType", m_cartridgeTypeCombo->currentData().toInt());
    settings.setValue("machine/cartridge2Enabled", m_cartridge2EnabledCheck->isChecked());
    settings.setValue("machine/cartridge2Path", m_cartridge2Path->text());
    settings.setValue("machine/cartridge2Type", m_cartridge2TypeCombo->currentData().toInt());
    settings.setValue("machine/cartridgeAutoReboot", m_cartridgeAutoRebootCheck->isChecked());
    
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
    settings.setValue("audio/volume", m_volumeSlider->value());
    settings.setValue("audio/bufferLength", m_bufferLengthSpinBox->value());
    settings.setValue("audio/latency", m_audioLatencySpinBox->value());
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
    
    // Memory Configuration defaults
    m_enable800RamCheck->setChecked(false);
    m_enableMosaicCheck->setChecked(false);
    m_mosaicSizeSpinBox->setValue(320);
    m_enableAxlonCheck->setChecked(false);
    m_axlonSizeSpinBox->setValue(320);
    m_axlonShadowCheck->setChecked(false);
    m_enableMapRamCheck->setChecked(false);
    
    // Performance defaults
    m_turboModeCheck->setChecked(false);
    
    // Cartridge Configuration defaults
    m_cartridgeEnabledCheck->setChecked(false);
    m_cartridgePath->clear();
    m_cartridgeTypeCombo->setCurrentIndex(0); // Auto-detect
    m_cartridge2EnabledCheck->setChecked(false);
    m_cartridge2Path->clear();
    m_cartridge2TypeCombo->setCurrentIndex(0); // Auto-detect
    m_cartridgeAutoRebootCheck->setChecked(true);
    
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
    m_volumeSlider->setValue(80);          // 80% volume
    m_volumeLabel->setText("80%");
    m_bufferLengthSpinBox->setValue(100);  // 100ms buffer
    m_audioLatencySpinBox->setValue(20);   // 20ms latency
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