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
    , m_profileManager(nullptr)
    , m_profileWidget(nullptr)
    , m_hardwareTab(nullptr)
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
    
    // Initialize profile management
    m_profileManager = new ConfigurationProfileManager(this);
    
    // Create profile selection section
    createProfileSection();
    mainLayout->addWidget(m_profileWidget);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget);
    
    // Create tabs
    createHardwareTab();
    createAudioConfigTab();
    createVideoDisplayTab();
    createInputConfigTab();
    createMediaConfigTab();
    
    // Create button box with custom buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_defaultsButton = new QPushButton("Restore Defaults");
    m_buttonBox->addButton(m_defaultsButton, QDialogButtonBox::ResetRole);
    
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(m_defaultsButton, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    
    mainLayout->addWidget(m_buttonBox);
    
    // Debug: Check slider values before loading settings
    qDebug() << "NTSC slider values BEFORE loadSettings() - Sat:" << m_ntscSaturationSlider->value() 
             << "Cont:" << m_ntscContrastSlider->value() 
             << "Bright:" << m_ntscBrightnessSlider->value()
             << "Gamma:" << m_ntscGammaSlider->value() 
             << "Tint:" << m_ntscTintSlider->value();

    // Load current settings
    loadSettings();
    
    // Debug: Check slider values after loading settings
    qDebug() << "NTSC slider values AFTER loadSettings() - Sat:" << m_ntscSaturationSlider->value() 
             << "Cont:" << m_ntscContrastSlider->value() 
             << "Bright:" << m_ntscBrightnessSlider->value()
             << "Gamma:" << m_ntscGammaSlider->value() 
             << "Tint:" << m_ntscTintSlider->value();
    
    // Connect video system change to update PAL/NTSC controls
    connect(m_videoSystemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updateVideoSystemDependentControls);
    
    // Debug: Final check of slider values after complete initialization
    qDebug() << "NTSC slider values FINAL (constructor end) - Sat:" << m_ntscSaturationSlider->value() 
             << "Cont:" << m_ntscContrastSlider->value() 
             << "Bright:" << m_ntscBrightnessSlider->value()
             << "Gamma:" << m_ntscGammaSlider->value() 
             << "Tint:" << m_ntscTintSlider->value();
}

void SettingsDialog::createHardwareTab()
{
    m_hardwareTab = new QWidget();
    m_tabWidget->addTab(m_hardwareTab, "Hardware");
    
    // Create main horizontal layout for three columns to keep height manageable
    QHBoxLayout* mainLayout = new QHBoxLayout(m_hardwareTab);
    
    // Left column layout
    QVBoxLayout* leftColumn = new QVBoxLayout();
    
    // Center column layout  
    QVBoxLayout* centerColumn = new QVBoxLayout();
    
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
    
    m_osRomBrowse = new QPushButton("...");
    m_osRomBrowse->setMaximumWidth(45);
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
    
    m_basicRomBrowse = new QPushButton("...");
    m_basicRomBrowse->setMaximumWidth(45);
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
    
    centerColumn->addWidget(memoryGroup);
    
    // Performance Group
    QGroupBox* performanceGroup = new QGroupBox("Performance");
    QVBoxLayout* performanceLayout = new QVBoxLayout(performanceGroup);
    
    m_turboModeCheck = new QCheckBox("Run as fast as possible (Turbo mode)");
    m_turboModeCheck->setToolTip("Run emulator at maximum speed, ignoring timing constraints");
    performanceLayout->addWidget(m_turboModeCheck);
    
    // Speed Control
    QWidget* speedWidget = new QWidget();
    QHBoxLayout* speedLayout = new QHBoxLayout(speedWidget);
    speedLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* speedTitleLabel = new QLabel("Emulation Speed:");
    speedLayout->addWidget(speedTitleLabel);
    
    m_speedSlider = new QSlider();
    m_speedSlider->setOrientation(Qt::Horizontal);
    m_speedSlider->setMinimum(0);    // 0.5x speed (index 0)
    m_speedSlider->setMaximum(10);   // 10x speed (index 10)  
    m_speedSlider->setValue(1);      // 1x default speed (index 1)
    m_speedSlider->setToolTip("Set emulation speed multiplier (0.5x - 10x)");
    m_speedSlider->setTickPosition(QSlider::TicksBelow);
    m_speedSlider->setTickInterval(1);
    speedLayout->addWidget(m_speedSlider, 1);
    
    m_speedLabel = new QLabel("1x");
    m_speedLabel->setMinimumWidth(60);
    m_speedLabel->setAlignment(Qt::AlignCenter);
    speedLayout->addWidget(m_speedLabel);
    
    connect(m_speedSlider, &QSlider::valueChanged, [this](int index) {
        // Convert slider index to speed multiplier
        double speedMultiplier;
        QString labelText;
        
        if (index == 0) {
            speedMultiplier = 0.5;
            labelText = "0.5x";
        } else {
            speedMultiplier = index;  // 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
            labelText = QString::number(index) + "x";
        }
        
        m_speedLabel->setText(labelText);
        
        // Update emulator speed in real-time
        if (m_emulator) {
            int percentage = (int)(speedMultiplier * 100);
            m_emulator->setEmulationSpeed(percentage);
        }
    });
    
    performanceLayout->addWidget(speedWidget);
    
    rightColumn->addWidget(performanceGroup);
    
    // Add stretch to balance columns
    leftColumn->addStretch();
    // Move memory group to center column to balance layout
    centerColumn->addWidget(memoryGroup);
    
    // POKEY Enhancements Group
    QGroupBox* pokeyGroup = new QGroupBox("POKEY");
    QVBoxLayout* pokeyLayout = new QVBoxLayout(pokeyGroup);
    
    m_stereoPokey = new QCheckBox("Stereo POKEY");
    m_stereoPokey->setToolTip("Enable dual POKEY sound chips for stereo audio");
    pokeyLayout->addWidget(m_stereoPokey);
    
    rightColumn->addWidget(pokeyGroup);
    
    // SIO Performance Group  
    QGroupBox* sioGroup = new QGroupBox("SIO");
    QVBoxLayout* sioLayout = new QVBoxLayout(sioGroup);
    
    m_sioAcceleration = new QCheckBox("SIO acceleration");
    m_sioAcceleration->setToolTip("Speed up disk and cassette operations");
    sioLayout->addWidget(m_sioAcceleration);
    
    rightColumn->addWidget(sioGroup);
    
    // 80-Column Display Cards Group
    QGroupBox* displayGroup = new QGroupBox("80-Column Cards");
    QVBoxLayout* displayLayout = new QVBoxLayout(displayGroup);
    
    m_xep80Enabled = new QCheckBox("XEP80");
    m_xep80Enabled->setToolTip("Enable XEP80 80-column display interface");
    displayLayout->addWidget(m_xep80Enabled);
    
    m_af80Enabled = new QCheckBox("Austin Franklin 80");
    m_af80Enabled->setToolTip("Enable Austin Franklin 80-column display board");
    displayLayout->addWidget(m_af80Enabled);
    
    m_bit3Enabled = new QCheckBox("Bit3 Full View 80");
    m_bit3Enabled->setToolTip("Enable Bit3 Full View 80-column display board");
    displayLayout->addWidget(m_bit3Enabled);
    
    rightColumn->addWidget(displayGroup);
    
    // PBI Extensions Group
    QGroupBox* pbiGroup = new QGroupBox("PBI Extensions");
    QVBoxLayout* pbiLayout = new QVBoxLayout(pbiGroup);
    
    m_atari1400Enabled = new QCheckBox("1400XL Modem");
    m_atari1400Enabled->setToolTip("Enable Atari 1400XL built-in modem");
    pbiLayout->addWidget(m_atari1400Enabled);
    
    m_atari1450Enabled = new QCheckBox("1450XLD Disk");
    m_atari1450Enabled->setToolTip("Enable Atari 1450XLD built-in disk drive");
    pbiLayout->addWidget(m_atari1450Enabled);
    
    m_proto80Enabled = new QCheckBox("Proto 80-Column");
    m_proto80Enabled->setToolTip("Enable prototype 80-column board for 1090");
    pbiLayout->addWidget(m_proto80Enabled);
    
    // Voice Synthesis
    m_voiceboxEnabled = new QCheckBox("Voicebox");
    m_voiceboxEnabled->setToolTip("Enable Voicebox speech synthesis");
    pbiLayout->addWidget(m_voiceboxEnabled);
    
    rightColumn->addWidget(pbiGroup);
    
    // Add stretch to balance columns
    leftColumn->addStretch();
    centerColumn->addStretch();
    rightColumn->addStretch();
    
    // Add all three columns to main layout
    mainLayout->addLayout(leftColumn, 1);
    mainLayout->addLayout(centerColumn, 1);
    mainLayout->addLayout(rightColumn, 1);
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
    
    // Connect for real-time audio enable/disable
    connect(m_soundEnabled, &QCheckBox::toggled, [this](bool enabled) {
        if (m_emulator) {
            m_emulator->enableAudio(enabled);
        }
    });
    
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
        
        // Update emulator volume in real-time
        if (m_emulator) {
            float volume = value / 100.0f; // Convert percentage to 0.0-1.0 range
            m_emulator->setVolume(volume);
        }
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
    
    // General Video Group - 3 column layout for space efficiency
    QGroupBox* generalGroup = new QGroupBox("General Video");
    QHBoxLayout* generalMainLayout = new QHBoxLayout(generalGroup);
    
    // Column 1: Artifacting and Core Settings
    QVBoxLayout* generalCol1 = new QVBoxLayout();
    
    QHBoxLayout* artifactRow = new QHBoxLayout();
    artifactRow->addWidget(new QLabel("Artifacting:"));
    m_artifactingMode = new QComboBox();
    m_artifactingMode->addItem("None", "none");
    m_artifactingMode->addItem("NTSC Old", "ntsc-old");
    m_artifactingMode->addItem("NTSC New", "ntsc-new");
    // Note: ntsc-full and pal-blend disabled in current build
    m_artifactingMode->setToolTip("Color artifacting simulation mode (NTSC modes work in NTSC video, PAL Simple available for PAL)");
    artifactRow->addWidget(m_artifactingMode);
    artifactRow->addStretch();
    generalCol1->addLayout(artifactRow);
    
    // Connect artifact mode for real-time updates
    connect(m_artifactingMode, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        QString artifactMode = m_artifactingMode->currentData().toString();
        m_emulator->updateArtifactSettings(artifactMode);
    });
    
    m_showFPS = new QCheckBox("Show FPS Counter");
    m_showFPS->setToolTip("Display frames per second in the corner");
    generalCol1->addWidget(m_showFPS);
    generalCol1->addStretch();
    
    // Column 2: Scaling and Aspect Settings
    QVBoxLayout* generalCol2 = new QVBoxLayout();
    
    m_scalingFilter = new QCheckBox("Enable Scaling Filter");
    m_scalingFilter->setToolTip("Apply smoothing when scaling the display");
    generalCol2->addWidget(m_scalingFilter);
    
    m_keepAspectRatio = new QCheckBox("Keep 4:3 Aspect Ratio");
    m_keepAspectRatio->setToolTip("Maintain authentic 4:3 display proportions when resizing window");
    generalCol2->addWidget(m_keepAspectRatio);
    generalCol2->addStretch();
    
    // Column 3: Fullscreen Settings
    QVBoxLayout* generalCol3 = new QVBoxLayout();
    
    m_fullscreenMode = new QCheckBox("Start in Fullscreen Mode");
    m_fullscreenMode->setToolTip("Launch application in fullscreen mode for immersive retro experience");
    generalCol3->addWidget(m_fullscreenMode);
    generalCol3->addStretch();
    
    // Add columns to main layout
    generalMainLayout->addLayout(generalCol1, 1);
    generalMainLayout->addLayout(generalCol2, 1);
    generalMainLayout->addLayout(generalCol3, 1);
    
    tabLayout->addWidget(generalGroup);
    
    // Screen Display Options Group - 3 column layout for space efficiency
    QGroupBox* displayGroup = new QGroupBox("Screen Display Options");
    QHBoxLayout* displayMainLayout = new QHBoxLayout(displayGroup);
    
    // Column 1: Area Controls
    QVBoxLayout* displayCol1 = new QVBoxLayout();
    
    QHBoxLayout* horizAreaRow = new QHBoxLayout();
    horizAreaRow->addWidget(new QLabel("Horizontal Area:"));
    m_horizontalArea = new QComboBox();
    m_horizontalArea->addItem("Narrow", "narrow");
    m_horizontalArea->addItem("TV Safe Area", "tv");
    m_horizontalArea->addItem("Full Width", "full");
    m_horizontalArea->setToolTip("Set horizontal visible area (narrow=320px, tv=336px, full=384px)");
    horizAreaRow->addWidget(m_horizontalArea);
    displayCol1->addLayout(horizAreaRow);
    
    QHBoxLayout* vertAreaRow = new QHBoxLayout();
    vertAreaRow->addWidget(new QLabel("Vertical Area:"));
    m_verticalArea = new QComboBox();
    m_verticalArea->addItem("Short", "short");
    m_verticalArea->addItem("TV Safe Area", "tv");
    m_verticalArea->addItem("Full Height", "full");
    m_verticalArea->setToolTip("Set vertical visible area (short=200px, tv=240px, full=300px)");
    vertAreaRow->addWidget(m_verticalArea);
    displayCol1->addLayout(vertAreaRow);
    displayCol1->addStretch();
    
    // Column 2: Shift Controls
    QVBoxLayout* displayCol2 = new QVBoxLayout();
    
    QHBoxLayout* horizShiftRow = new QHBoxLayout();
    horizShiftRow->addWidget(new QLabel("H Shift:"));
    m_horizontalShift = new QSpinBox();
    m_horizontalShift->setRange(-384, 384);
    m_horizontalShift->setValue(0);
    m_horizontalShift->setSuffix(" px");
    m_horizontalShift->setToolTip("Shift display horizontally (-384 to 384 pixels)");
    horizShiftRow->addWidget(m_horizontalShift);
    displayCol2->addLayout(horizShiftRow);
    
    QHBoxLayout* vertShiftRow = new QHBoxLayout();
    vertShiftRow->addWidget(new QLabel("V Shift:"));
    m_verticalShift = new QSpinBox();
    m_verticalShift->setRange(-300, 300);
    m_verticalShift->setValue(0);
    m_verticalShift->setSuffix(" px");
    m_verticalShift->setToolTip("Shift display vertically (-300 to 300 pixels)");
    vertShiftRow->addWidget(m_verticalShift);
    displayCol2->addLayout(vertShiftRow);
    displayCol2->addStretch();
    
    // Column 3: Mode Controls
    QVBoxLayout* displayCol3 = new QVBoxLayout();
    
    QHBoxLayout* fitScreenRow = new QHBoxLayout();
    fitScreenRow->addWidget(new QLabel("Fit Screen:"));
    m_fitScreen = new QComboBox();
    m_fitScreen->addItem("Fit Width", "width");
    m_fitScreen->addItem("Fit Height", "height");
    m_fitScreen->addItem("Fit Both", "both");
    m_fitScreen->setToolTip("Method for fitting image to screen size");
    fitScreenRow->addWidget(m_fitScreen);
    displayCol3->addLayout(fitScreenRow);
    
    m_show80Column = new QCheckBox("Enable 80-Column Display");
    m_show80Column->setToolTip("Show 80-column text mode (requires compatible software)");
    displayCol3->addWidget(m_show80Column);
    
    m_vSyncEnabled = new QCheckBox("Enable Vertical Sync");
    m_vSyncEnabled->setToolTip("Synchronize display to monitor refresh rate (reduces tearing)");
    displayCol3->addWidget(m_vSyncEnabled);
    displayCol3->addStretch();
    
    // Add columns to main layout
    displayMainLayout->addLayout(displayCol1, 1);
    displayMainLayout->addLayout(displayCol2, 1);
    displayMainLayout->addLayout(displayCol3, 1);
    
    tabLayout->addWidget(displayGroup);
    
    // PAL-specific settings
    m_palGroup = new QGroupBox("PAL Video Options");
    QVBoxLayout* palLayout = new QVBoxLayout(m_palGroup);
    
    // Color Blending row
    QHBoxLayout* blendingRow = new QHBoxLayout();
    blendingRow->addWidget(new QLabel("Color Blending:"));
    m_palBlending = new QComboBox();
    m_palBlending->addItem("None", "none");
    m_palBlending->addItem("Simple", "simple");
    m_palBlending->addItem("Linear", "linear");
    m_palBlending->setToolTip("PAL color blending mode");
    blendingRow->addWidget(m_palBlending);
    blendingRow->addStretch();
    palLayout->addLayout(blendingRow);
    
    // PAL Color Adjustment Controls - isolated in separate widgets
    // PAL Saturation - completely isolated
    QWidget* palSatWidget = new QWidget();
    QHBoxLayout* palSatLayout = new QHBoxLayout(palSatWidget);
    palSatLayout->setContentsMargins(0, 0, 0, 0);
    m_palSaturationSlider = new QSlider();
    m_palSaturationSlider->setOrientation(Qt::Horizontal);
    m_palSaturationSlider->setParent(palSatWidget);
    m_palSaturationSlider->setMinimum(-100);
    m_palSaturationSlider->setMaximum(100);
    m_palSaturationSlider->setValue(0);
    // Debug: Track user vs programmatic moves
    connect(m_palSaturationSlider, &QSlider::sliderMoved, [this](int value) {
        qDebug() << "PAL Saturation slider MOVED by user to:" << value;
    });
    palSatLayout->addWidget(m_palSaturationSlider, 1);
    m_palSaturationLabel = new QLabel("0.00", palSatWidget);
    m_palSaturationLabel->setMinimumWidth(60);
    m_palSaturationLabel->setAlignment(Qt::AlignCenter);
    palSatLayout->addWidget(m_palSaturationLabel);
    connect(m_palSaturationSlider, &QSlider::valueChanged, [this](int value) {
        qDebug() << "PAL Saturation slider changed to:" << value;
        m_palSaturationLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updatePalColorSettings(
                m_palSaturationSlider->value(),
                m_palContrastSlider->value(),
                m_palBrightnessSlider->value(),
                m_palGammaSlider->value(),
                m_palTintSlider->value()
            );
        }
    });
    // Add saturation row manually
    QHBoxLayout* palSatRow = new QHBoxLayout();
    palSatRow->addWidget(new QLabel("Saturation:"));
    palSatRow->addWidget(palSatWidget, 1);
    palLayout->addLayout(palSatRow);
    
    // TODO: Fix visual bouncing issue - sliders work correctly but bounce visually
    
    // PAL Contrast
    QWidget* palContrastWidget = new QWidget();
    QHBoxLayout* palContrastLayout = new QHBoxLayout(palContrastWidget);
    palContrastLayout->setContentsMargins(0, 0, 0, 0);
    m_palContrastSlider = new QSlider();
    m_palContrastSlider->setOrientation(Qt::Horizontal);
    m_palContrastSlider->setParent(palContrastWidget);
    m_palContrastSlider->setMinimum(-100);
    m_palContrastSlider->setMaximum(100);
    m_palContrastSlider->setValue(0);
    palContrastLayout->addWidget(m_palContrastSlider, 1);
    m_palContrastLabel = new QLabel("0.00", palContrastWidget);
    m_palContrastLabel->setMinimumWidth(60);
    m_palContrastLabel->setAlignment(Qt::AlignCenter);
    palContrastLayout->addWidget(m_palContrastLabel);
    connect(m_palContrastSlider, &QSlider::valueChanged, [this](int value) {
        m_palContrastLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updatePalColorSettings(
                m_palSaturationSlider->value(),
                m_palContrastSlider->value(),
                m_palBrightnessSlider->value(),
                m_palGammaSlider->value(),
                m_palTintSlider->value()
            );
        }
    });
    QHBoxLayout* palContrastRow = new QHBoxLayout();
    palContrastRow->addWidget(new QLabel("Contrast:"));
    palContrastRow->addWidget(palContrastWidget, 1);
    palLayout->addLayout(palContrastRow);
    
    // PAL Brightness
    QWidget* palBrightnessWidget = new QWidget();
    QHBoxLayout* palBrightnessLayout = new QHBoxLayout(palBrightnessWidget);
    palBrightnessLayout->setContentsMargins(0, 0, 0, 0);
    m_palBrightnessSlider = new QSlider();
    m_palBrightnessSlider->setOrientation(Qt::Horizontal);
    m_palBrightnessSlider->setParent(palBrightnessWidget);
    m_palBrightnessSlider->setMinimum(-100);
    m_palBrightnessSlider->setMaximum(100);
    m_palBrightnessSlider->setValue(0);
    palBrightnessLayout->addWidget(m_palBrightnessSlider, 1);
    m_palBrightnessLabel = new QLabel("0.00", palBrightnessWidget);
    m_palBrightnessLabel->setMinimumWidth(60);
    m_palBrightnessLabel->setAlignment(Qt::AlignCenter);
    palBrightnessLayout->addWidget(m_palBrightnessLabel);
    connect(m_palBrightnessSlider, &QSlider::valueChanged, [this](int value) {
        m_palBrightnessLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updatePalColorSettings(
                m_palSaturationSlider->value(),
                m_palContrastSlider->value(),
                m_palBrightnessSlider->value(),
                m_palGammaSlider->value(),
                m_palTintSlider->value()
            );
        }
    });
    QHBoxLayout* palBrightnessRow = new QHBoxLayout();
    palBrightnessRow->addWidget(new QLabel("Brightness:"));
    palBrightnessRow->addWidget(palBrightnessWidget, 1);
    palLayout->addLayout(palBrightnessRow);
    
    // PAL Gamma
    QWidget* palGammaWidget = new QWidget();
    QHBoxLayout* palGammaLayout = new QHBoxLayout(palGammaWidget);
    palGammaLayout->setContentsMargins(0, 0, 0, 0);
    m_palGammaSlider = new QSlider();
    m_palGammaSlider->setOrientation(Qt::Horizontal);
    m_palGammaSlider->setParent(palGammaWidget);
    m_palGammaSlider->setMinimum(10);
    m_palGammaSlider->setMaximum(400);
    m_palGammaSlider->setValue(100);
    palGammaLayout->addWidget(m_palGammaSlider, 1);
    m_palGammaLabel = new QLabel("1.00", palGammaWidget);
    m_palGammaLabel->setMinimumWidth(60);
    m_palGammaLabel->setAlignment(Qt::AlignCenter);
    palGammaLayout->addWidget(m_palGammaLabel);
    connect(m_palGammaSlider, &QSlider::valueChanged, [this](int value) {
        m_palGammaLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updatePalColorSettings(
                m_palSaturationSlider->value(),
                m_palContrastSlider->value(),
                m_palBrightnessSlider->value(),
                m_palGammaSlider->value(),
                m_palTintSlider->value()
            );
        }
    });
    QHBoxLayout* palGammaRow = new QHBoxLayout();
    palGammaRow->addWidget(new QLabel("Gamma:"));
    palGammaRow->addWidget(palGammaWidget, 1);
    palLayout->addLayout(palGammaRow);
    
    // PAL Tint
    QWidget* palTintWidget = new QWidget();
    QHBoxLayout* palTintLayout = new QHBoxLayout(palTintWidget);
    palTintLayout->setContentsMargins(0, 0, 0, 0);
    m_palTintSlider = new QSlider();
    m_palTintSlider->setOrientation(Qt::Horizontal);
    m_palTintSlider->setParent(palTintWidget);
    m_palTintSlider->setMinimum(-180);
    m_palTintSlider->setMaximum(180);
    m_palTintSlider->setValue(0);
    palTintLayout->addWidget(m_palTintSlider, 1);
    m_palTintLabel = new QLabel("0°", palTintWidget);
    m_palTintLabel->setMinimumWidth(60);
    m_palTintLabel->setAlignment(Qt::AlignCenter);
    palTintLayout->addWidget(m_palTintLabel);
    connect(m_palTintSlider, &QSlider::valueChanged, [this](int value) {
        m_palTintLabel->setText(QString::number(value) + "°");
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updatePalColorSettings(
                m_palSaturationSlider->value(),
                m_palContrastSlider->value(),
                m_palBrightnessSlider->value(),
                m_palGammaSlider->value(),
                m_palTintSlider->value()
            );
        }
    });
    QHBoxLayout* palTintRow = new QHBoxLayout();
    palTintRow->addWidget(new QLabel("Tint:"));
    palTintRow->addWidget(palTintWidget, 1);
    palLayout->addLayout(palTintRow);
    
    // PAL Reset Colors Button
    QHBoxLayout* palResetRow = new QHBoxLayout();
    QPushButton* resetPalColorsButton = new QPushButton("Reset PAL Colors");
    resetPalColorsButton->setToolTip("Reset all PAL color settings to defaults");
    resetPalColorsButton->setMaximumWidth(150);
    connect(resetPalColorsButton, &QPushButton::clicked, [this]() {
        // Reset all PAL color sliders to defaults
        m_palSaturationSlider->blockSignals(true);
        m_palContrastSlider->blockSignals(true);
        m_palBrightnessSlider->blockSignals(true);
        m_palGammaSlider->blockSignals(true);
        m_palTintSlider->blockSignals(true);
        
        m_palSaturationSlider->setValue(0);
        m_palContrastSlider->setValue(0);
        m_palBrightnessSlider->setValue(0);
        m_palGammaSlider->setValue(100);
        m_palTintSlider->setValue(0);
        
        // Update labels
        m_palSaturationLabel->setText("0.00");
        m_palContrastLabel->setText("0.00");
        m_palBrightnessLabel->setText("0.00");
        m_palGammaLabel->setText("1.00");
        m_palTintLabel->setText("0°");
        
        m_palSaturationSlider->blockSignals(false);
        m_palContrastSlider->blockSignals(false);
        m_palBrightnessSlider->blockSignals(false);
        m_palGammaSlider->blockSignals(false);
        m_palTintSlider->blockSignals(false);
        
        // Apply the reset values to emulator
        if (m_emulator) {
            m_emulator->updatePalColorSettings(0, 0, 0, 100, 0);
        }
        
        qDebug() << "PAL colors reset to defaults";
    });
    palResetRow->addWidget(resetPalColorsButton);
    palResetRow->addStretch();
    palLayout->addLayout(palResetRow);
    
    // Set tooltips for PAL color controls
    m_palSaturationSlider->setToolTip("Adjust PAL color saturation (-1.0 to 1.0)");
    m_palContrastSlider->setToolTip("Adjust PAL contrast (-1.0 to 1.0)");
    m_palBrightnessSlider->setToolTip("Adjust PAL brightness (-1.0 to 1.0)");
    m_palGammaSlider->setToolTip("Adjust PAL gamma correction (0.1 to 4.0)");
    m_palTintSlider->setToolTip("Adjust PAL color tint (-180° to 180°)");
    
    // WORKAROUND: Apply custom stylesheet to fix QTBUG-98093 (macOS Monterey slider bounce)
    QString sliderStyleSheet = 
        "QSlider::groove:horizontal {"
            "border: 1px solid #999999;"
            "height: 6px;"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4);"
            "margin: 2px 0;"
            "border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f);"
            "border: 1px solid #5c5c5c;"
            "width: 14px;"
            "margin: -2px 0;"
            "border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal:hover {"
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #d4d4d4, stop:1 #afafaf);"
        "}";
    
    // Apply stylesheet to all PAL sliders
    m_palSaturationSlider->setStyleSheet(sliderStyleSheet);
    m_palContrastSlider->setStyleSheet(sliderStyleSheet);
    m_palBrightnessSlider->setStyleSheet(sliderStyleSheet);
    m_palGammaSlider->setStyleSheet(sliderStyleSheet);
    m_palTintSlider->setStyleSheet(sliderStyleSheet);
    
    // NTSC-specific settings
    m_ntscGroup = new QGroupBox("NTSC Video Options");
    QVBoxLayout* ntscLayout = new QVBoxLayout(m_ntscGroup);
    
    // Note: NTSC artifacting is now handled by the main artifacting dropdown in General Video
    // This redundant NTSC artifacting dropdown is commented out
    // QHBoxLayout* artifactingRow = new QHBoxLayout();
    // artifactingRow->addWidget(new QLabel("Artifacting:"));
    // m_ntscArtifacting = new QComboBox();
    // m_ntscArtifacting->addItem("Standard", "standard");
    // m_ntscArtifacting->addItem("High Quality", "high");
    // m_ntscArtifacting->addItem("Composite", "composite");
    // m_ntscArtifacting->setToolTip("NTSC color artifacting quality");
    // artifactingRow->addWidget(m_ntscArtifacting);
    // artifactingRow->addStretch();
    // ntscLayout->addLayout(artifactingRow);
    
    m_ntscSharpness = new QCheckBox("Enable NTSC Sharpness");
    m_ntscSharpness->setToolTip("Enhance NTSC video sharpness");
    ntscLayout->addWidget(m_ntscSharpness);
    
    // NTSC Color Adjustment Controls - completely isolated
    // NTSC Saturation
    QWidget* ntscSatWidget = new QWidget();
    QHBoxLayout* ntscSatLayout = new QHBoxLayout(ntscSatWidget);
    ntscSatLayout->setContentsMargins(0, 0, 0, 0);
    m_ntscSaturationSlider = new QSlider();
    m_ntscSaturationSlider->setOrientation(Qt::Horizontal);
    m_ntscSaturationSlider->setParent(ntscSatWidget);
    m_ntscSaturationSlider->setMinimum(-100);
    m_ntscSaturationSlider->setMaximum(100);
    m_ntscSaturationSlider->setValue(0);
    // Debug: Override setValue to track programmatic changes
    connect(m_ntscSaturationSlider, &QSlider::sliderMoved, [this](int value) {
        qDebug() << "NTSC Saturation slider MOVED by user to:" << value;
    });
    ntscSatLayout->addWidget(m_ntscSaturationSlider, 1);
    m_ntscSaturationLabel = new QLabel("0.00", ntscSatWidget);
    m_ntscSaturationLabel->setMinimumWidth(60);
    m_ntscSaturationLabel->setAlignment(Qt::AlignCenter);
    ntscSatLayout->addWidget(m_ntscSaturationLabel);
    connect(m_ntscSaturationSlider, &QSlider::valueChanged, [this](int value) {
        qDebug() << "NTSC Saturation slider changed to:" << value;
        m_ntscSaturationLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time - ONLY pass the changed value
        if (m_emulator) {
            m_emulator->updateNtscColorSettings(
                value,  // Only pass the changed saturation value
                m_ntscContrastSlider->value(),
                m_ntscBrightnessSlider->value(),
                m_ntscGammaSlider->value(),
                m_ntscTintSlider->value()
            );
        }
    });
    // Add NTSC saturation row manually
    QHBoxLayout* ntscSatRow = new QHBoxLayout();
    ntscSatRow->addWidget(new QLabel("Saturation:"));
    ntscSatRow->addWidget(ntscSatWidget, 1);
    ntscLayout->addLayout(ntscSatRow);
    
    // NTSC Contrast
    QWidget* ntscContrastWidget = new QWidget();
    QHBoxLayout* ntscContrastLayout = new QHBoxLayout(ntscContrastWidget);
    ntscContrastLayout->setContentsMargins(0, 0, 0, 0);
    m_ntscContrastSlider = new QSlider();
    m_ntscContrastSlider->setOrientation(Qt::Horizontal);
    m_ntscContrastSlider->setParent(ntscContrastWidget);
    m_ntscContrastSlider->setMinimum(-100);
    m_ntscContrastSlider->setMaximum(100);
    m_ntscContrastSlider->setValue(0);
    // Debug: Track user vs programmatic moves
    connect(m_ntscContrastSlider, &QSlider::sliderMoved, [this](int value) {
        qDebug() << "NTSC Contrast slider MOVED by user to:" << value;
    });
    ntscContrastLayout->addWidget(m_ntscContrastSlider, 1);
    m_ntscContrastLabel = new QLabel("0.00", ntscContrastWidget);
    m_ntscContrastLabel->setMinimumWidth(60);
    m_ntscContrastLabel->setAlignment(Qt::AlignCenter);
    ntscContrastLayout->addWidget(m_ntscContrastLabel);
    connect(m_ntscContrastSlider, &QSlider::valueChanged, [this](int value) {
        qDebug() << "NTSC Contrast slider changed to:" << value;
        m_ntscContrastLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updateNtscColorSettings(
                m_ntscSaturationSlider->value(),
                value,  // Use the changed value directly
                m_ntscBrightnessSlider->value(),
                m_ntscGammaSlider->value(),
                m_ntscTintSlider->value()
            );
        }
    });
    QHBoxLayout* ntscContrastRow = new QHBoxLayout();
    ntscContrastRow->addWidget(new QLabel("Contrast:"));
    ntscContrastRow->addWidget(ntscContrastWidget, 1);
    ntscLayout->addLayout(ntscContrastRow);
    
    // NTSC Brightness
    QWidget* ntscBrightnessWidget = new QWidget();
    QHBoxLayout* ntscBrightnessLayout = new QHBoxLayout(ntscBrightnessWidget);
    ntscBrightnessLayout->setContentsMargins(0, 0, 0, 0);
    m_ntscBrightnessSlider = new QSlider();
    m_ntscBrightnessSlider->setOrientation(Qt::Horizontal);
    m_ntscBrightnessSlider->setParent(ntscBrightnessWidget);
    m_ntscBrightnessSlider->setMinimum(-100);
    m_ntscBrightnessSlider->setMaximum(100);
    m_ntscBrightnessSlider->setValue(0);
    ntscBrightnessLayout->addWidget(m_ntscBrightnessSlider, 1);
    m_ntscBrightnessLabel = new QLabel("0.00", ntscBrightnessWidget);
    m_ntscBrightnessLabel->setMinimumWidth(60);
    m_ntscBrightnessLabel->setAlignment(Qt::AlignCenter);
    ntscBrightnessLayout->addWidget(m_ntscBrightnessLabel);
    connect(m_ntscBrightnessSlider, &QSlider::valueChanged, [this](int value) {
        qDebug() << "NTSC Brightness slider changed to:" << value;
        m_ntscBrightnessLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updateNtscColorSettings(
                m_ntscSaturationSlider->value(),
                m_ntscContrastSlider->value(),
                value,  // Use the changed value directly
                m_ntscGammaSlider->value(),
                m_ntscTintSlider->value()
            );
        }
    });
    QHBoxLayout* ntscBrightnessRow = new QHBoxLayout();
    ntscBrightnessRow->addWidget(new QLabel("Brightness:"));
    ntscBrightnessRow->addWidget(ntscBrightnessWidget, 1);
    ntscLayout->addLayout(ntscBrightnessRow);
    
    // NTSC Gamma
    QWidget* ntscGammaWidget = new QWidget();
    QHBoxLayout* ntscGammaLayout = new QHBoxLayout(ntscGammaWidget);
    ntscGammaLayout->setContentsMargins(0, 0, 0, 0);
    m_ntscGammaSlider = new QSlider();
    m_ntscGammaSlider->setOrientation(Qt::Horizontal);
    m_ntscGammaSlider->setParent(ntscGammaWidget);
    m_ntscGammaSlider->setMinimum(10);
    m_ntscGammaSlider->setMaximum(400);
    m_ntscGammaSlider->setValue(100);
    ntscGammaLayout->addWidget(m_ntscGammaSlider, 1);
    m_ntscGammaLabel = new QLabel("1.00", ntscGammaWidget);
    m_ntscGammaLabel->setMinimumWidth(60);
    m_ntscGammaLabel->setAlignment(Qt::AlignCenter);
    ntscGammaLayout->addWidget(m_ntscGammaLabel);
    connect(m_ntscGammaSlider, &QSlider::valueChanged, [this](int value) {
        qDebug() << "NTSC Gamma slider changed to:" << value;
        m_ntscGammaLabel->setText(QString::number(value / 100.0, 'f', 2));
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updateNtscColorSettings(
                m_ntscSaturationSlider->value(),
                m_ntscContrastSlider->value(),
                m_ntscBrightnessSlider->value(),
                value,  // Use the changed value directly
                m_ntscTintSlider->value()
            );
        }
    });
    QHBoxLayout* ntscGammaRow = new QHBoxLayout();
    ntscGammaRow->addWidget(new QLabel("Gamma:"));
    ntscGammaRow->addWidget(ntscGammaWidget, 1);
    ntscLayout->addLayout(ntscGammaRow);
    
    // NTSC Tint
    QWidget* ntscTintWidget = new QWidget();
    QHBoxLayout* ntscTintLayout = new QHBoxLayout(ntscTintWidget);
    ntscTintLayout->setContentsMargins(0, 0, 0, 0);
    m_ntscTintSlider = new QSlider();
    m_ntscTintSlider->setOrientation(Qt::Horizontal);
    m_ntscTintSlider->setParent(ntscTintWidget);
    m_ntscTintSlider->setMinimum(-180);
    m_ntscTintSlider->setMaximum(180);
    m_ntscTintSlider->setValue(0);
    ntscTintLayout->addWidget(m_ntscTintSlider, 1);
    m_ntscTintLabel = new QLabel("0°", ntscTintWidget);
    m_ntscTintLabel->setMinimumWidth(60);
    m_ntscTintLabel->setAlignment(Qt::AlignCenter);
    ntscTintLayout->addWidget(m_ntscTintLabel);
    connect(m_ntscTintSlider, &QSlider::valueChanged, [this](int value) {
        qDebug() << "NTSC Tint slider changed to:" << value;
        m_ntscTintLabel->setText(QString::number(value) + "°");
        // Update emulator color settings in real-time
        if (m_emulator) {
            m_emulator->updateNtscColorSettings(
                m_ntscSaturationSlider->value(),
                m_ntscContrastSlider->value(),
                m_ntscBrightnessSlider->value(),
                m_ntscGammaSlider->value(),
                value  // Use the changed value directly
            );
        }
    });
    QHBoxLayout* ntscTintRow = new QHBoxLayout();
    ntscTintRow->addWidget(new QLabel("Tint:"));
    ntscTintRow->addWidget(ntscTintWidget, 1);
    ntscLayout->addLayout(ntscTintRow);
    
    // NTSC Reset Colors Button
    QHBoxLayout* ntscResetRow = new QHBoxLayout();
    QPushButton* resetNtscColorsButton = new QPushButton("Reset NTSC Colors");
    resetNtscColorsButton->setToolTip("Reset all NTSC color settings to defaults");
    resetNtscColorsButton->setMaximumWidth(150);
    connect(resetNtscColorsButton, &QPushButton::clicked, [this]() {
        // Reset all NTSC color sliders to defaults
        m_ntscSaturationSlider->blockSignals(true);
        m_ntscContrastSlider->blockSignals(true);
        m_ntscBrightnessSlider->blockSignals(true);
        m_ntscGammaSlider->blockSignals(true);
        m_ntscTintSlider->blockSignals(true);
        
        m_ntscSaturationSlider->setValue(0);
        m_ntscContrastSlider->setValue(0);
        m_ntscBrightnessSlider->setValue(0);
        m_ntscGammaSlider->setValue(100);
        m_ntscTintSlider->setValue(0);
        
        // Update labels
        m_ntscSaturationLabel->setText("0.00");
        m_ntscContrastLabel->setText("0.00");
        m_ntscBrightnessLabel->setText("0.00");
        m_ntscGammaLabel->setText("1.00");
        m_ntscTintLabel->setText("0°");
        
        m_ntscSaturationSlider->blockSignals(false);
        m_ntscContrastSlider->blockSignals(false);
        m_ntscBrightnessSlider->blockSignals(false);
        m_ntscGammaSlider->blockSignals(false);
        m_ntscTintSlider->blockSignals(false);
        
        // Apply the reset values to emulator
        if (m_emulator) {
            m_emulator->updateNtscColorSettings(0, 0, 0, 100, 0);
        }
        
        qDebug() << "NTSC colors reset to defaults";
    });
    ntscResetRow->addWidget(resetNtscColorsButton);
    ntscResetRow->addStretch();
    ntscLayout->addLayout(ntscResetRow);
    
    // Set tooltips for NTSC color controls
    m_ntscSaturationSlider->setToolTip("Adjust NTSC color saturation (-1.0 to 1.0)");
    m_ntscContrastSlider->setToolTip("Adjust NTSC contrast (-1.0 to 1.0)");
    m_ntscBrightnessSlider->setToolTip("Adjust NTSC brightness (-1.0 to 1.0)");
    m_ntscGammaSlider->setToolTip("Adjust NTSC gamma correction (0.1 to 4.0)");
    m_ntscTintSlider->setToolTip("Adjust NTSC color tint (-180° to 180°)");
    
    // WORKAROUND: Apply same custom stylesheet to fix QTBUG-98093 (macOS Monterey slider bounce)
    // Apply stylesheet to all NTSC sliders
    m_ntscSaturationSlider->setStyleSheet(sliderStyleSheet);
    m_ntscContrastSlider->setStyleSheet(sliderStyleSheet);
    m_ntscBrightnessSlider->setStyleSheet(sliderStyleSheet);
    m_ntscGammaSlider->setStyleSheet(sliderStyleSheet);
    m_ntscTintSlider->setStyleSheet(sliderStyleSheet);
    
    // Add PAL and NTSC group boxes side by side
    QHBoxLayout* videoOptionsLayout = new QHBoxLayout();
    videoOptionsLayout->addWidget(m_palGroup);
    videoOptionsLayout->addWidget(m_ntscGroup);
    
    tabLayout->addLayout(videoOptionsLayout);
    tabLayout->addStretch();
}

void SettingsDialog::createInputConfigTab()
{
    m_inputTab = new QWidget();
    m_tabWidget->addTab(m_inputTab, "Input Configuration");
    
    // Create main horizontal layout
    QHBoxLayout* mainLayout = new QHBoxLayout(m_inputTab);
    
    // Left column - Joystick Configuration
    QVBoxLayout* leftColumn = new QVBoxLayout();
    
    // Joystick Configuration Group
    QGroupBox* joystickGroup = new QGroupBox("Joystick Configuration");
    QVBoxLayout* joystickLayout = new QVBoxLayout(joystickGroup);
    
    // Enable/Disable joysticks
    m_joystickEnabled = new QCheckBox("Enable Joystick Support");
    m_joystickEnabled->setToolTip("Enable or disable joystick input (equivalent to -nojoystick)");
    m_joystickEnabled->setChecked(true); // Default enabled
    joystickLayout->addWidget(m_joystickEnabled);
    
    // Hat support for joysticks 0-3
    QLabel* hatLabel = new QLabel("Hat Support:");
    hatLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    joystickLayout->addWidget(hatLabel);
    
    m_joystick0Hat = new QCheckBox("Use hat of joystick 0 (-joy0hat)");
    m_joystick0Hat->setToolTip("Use hat switch of joystick 0 for movement");
    joystickLayout->addWidget(m_joystick0Hat);
    
    m_joystick1Hat = new QCheckBox("Use hat of joystick 1 (-joy1hat)");
    m_joystick1Hat->setToolTip("Use hat switch of joystick 1 for movement");
    joystickLayout->addWidget(m_joystick1Hat);
    
    m_joystick2Hat = new QCheckBox("Use hat of joystick 2 (-joy2hat)");
    m_joystick2Hat->setToolTip("Use hat switch of joystick 2 for movement");
    joystickLayout->addWidget(m_joystick2Hat);
    
    m_joystick3Hat = new QCheckBox("Use hat of joystick 3 (-joy3hat)");
    m_joystick3Hat->setToolTip("Use hat switch of joystick 3 for movement");
    joystickLayout->addWidget(m_joystick3Hat);
    
    // Distinct joysticks option
    m_joyDistinct = new QCheckBox("One input device per emulated stick (-joy-distinct)");
    m_joyDistinct->setToolTip("Use separate input devices for each emulated joystick");
    joystickLayout->addWidget(m_joyDistinct);
    
    leftColumn->addWidget(joystickGroup);
    
    // Keyboard Joystick Emulation Group
    QGroupBox* kbdJoyGroup = new QGroupBox("Keyboard Joystick Emulation");
    QVBoxLayout* kbdJoyLayout = new QVBoxLayout(kbdJoyGroup);
    
    m_kbdJoy0Enabled = new QCheckBox("Enable joystick 0 keyboard emulation (-kbdjoy0)");
    m_kbdJoy0Enabled->setToolTip("Allow keyboard keys to emulate joystick 0 (WASD, etc.)");
    kbdJoyLayout->addWidget(m_kbdJoy0Enabled);
    
    m_kbdJoy1Enabled = new QCheckBox("Enable joystick 1 keyboard emulation (-kbdjoy1)");
    m_kbdJoy1Enabled->setToolTip("Allow keyboard keys to emulate joystick 1");
    kbdJoyLayout->addWidget(m_kbdJoy1Enabled);
    
    // Add some spacing
    kbdJoyLayout->addSpacing(10);
    
    m_swapJoysticks = new QCheckBox("Swap joystick assignments (WASD becomes Joy0)");
    m_swapJoysticks->setToolTip("When enabled: WASD controls Joy0, Numpad controls Joy1");
    kbdJoyLayout->addWidget(m_swapJoysticks);
    
    leftColumn->addWidget(kbdJoyGroup);
    leftColumn->addStretch();
    
    // Right column - Mouse and Keyboard Configuration
    QVBoxLayout* rightColumn = new QVBoxLayout();
    
    // Mouse Configuration Group
    QGroupBox* mouseGroup = new QGroupBox("Mouse Configuration");
    QVBoxLayout* mouseLayout = new QVBoxLayout(mouseGroup);
    
    m_grabMouse = new QCheckBox("Grab mouse (-grabmouse)");
    m_grabMouse->setToolTip("Prevent mouse from leaving emulator window");
    mouseLayout->addWidget(m_grabMouse);
    
    // Mouse device setting
    QHBoxLayout* mouseDeviceLayout = new QHBoxLayout();
    QLabel* mouseDeviceLabel = new QLabel("Mouse Device:");
    mouseDeviceLayout->addWidget(mouseDeviceLabel);
    
    m_mouseDevice = new QLineEdit();
    m_mouseDevice->setPlaceholderText("Default mouse device");
    m_mouseDevice->setToolTip("Specify custom mouse device (-mouse-device)");
    mouseDeviceLayout->addWidget(m_mouseDevice, 1);
    
    mouseLayout->addLayout(mouseDeviceLayout);
    rightColumn->addWidget(mouseGroup);
    
    // Keyboard Configuration Group
    QGroupBox* keyboardGroup = new QGroupBox("Keyboard Configuration");
    QVBoxLayout* keyboardLayout = new QVBoxLayout(keyboardGroup);
    
    m_keyboardToggle = new QCheckBox("Enable keyboard toggle functionality (-keyboardtoggle)");
    m_keyboardToggle->setToolTip("Enable special keyboard toggle features");
    keyboardLayout->addWidget(m_keyboardToggle);
    
    m_keyboardLeds = new QCheckBox("Enable keyboard LEDs (-keyboard-leds)");
    m_keyboardLeds->setToolTip("Enable keyboard LED indicators (for Atari 1200XL)");
    keyboardLayout->addWidget(m_keyboardLeds);
    
    rightColumn->addWidget(keyboardGroup);
    rightColumn->addStretch();
    
    // Connect joystick enable/disable functionality
    connect(m_joystickEnabled, &QCheckBox::toggled, [this](bool enabled) {
        m_joystick0Hat->setEnabled(enabled);
        m_joystick1Hat->setEnabled(enabled);
        m_joystick2Hat->setEnabled(enabled);
        m_joystick3Hat->setEnabled(enabled);
        m_joyDistinct->setEnabled(enabled);
    });
    
    // Add both columns to main layout
    mainLayout->addLayout(leftColumn, 1);
    mainLayout->addLayout(rightColumn, 1);
}

void SettingsDialog::createMediaConfigTab()
{
    m_mediaTab = new QWidget();
    m_tabWidget->addTab(m_mediaTab, "Media Configuration");
    
    // Create main horizontal layout
    QHBoxLayout* mainLayout = new QHBoxLayout(m_mediaTab);
    
    // Left column - Floppy Disks, Cassette and Special Devices
    QVBoxLayout* leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(8); // Reduce spacing between groups
    
    // Cartridge Configuration Group
    QGroupBox* cartridgeGroup = new QGroupBox("Cartridge Configuration");
    QVBoxLayout* cartridgeLayout = new QVBoxLayout(cartridgeGroup);
    cartridgeLayout->setSpacing(4); // Reduce spacing
    
    // Primary cartridge
    QVBoxLayout* cart1Layout = new QVBoxLayout();
    QLabel* cart1Label = new QLabel("Primary Cartridge:");
    cart1Label->setStyleSheet("font-weight: bold;");
    cart1Layout->addWidget(cart1Label);
    
    QHBoxLayout* cart1PathLayout = new QHBoxLayout();
    m_cartridgeEnabledCheck = new QCheckBox("Enable");
    m_cartridgeEnabledCheck->setMinimumWidth(50);
    cart1PathLayout->addWidget(m_cartridgeEnabledCheck);
    
    // Add spacing between checkbox and input field
    cart1PathLayout->addSpacing(8);
    
    m_cartridgePath = new QLineEdit();
    m_cartridgePath->setPlaceholderText("Select cartridge file (.rom, .bin, .car)");
    setupFilePathTooltip(m_cartridgePath);
    cart1PathLayout->addWidget(m_cartridgePath, 1);
    
    m_cartridgeBrowse = new QPushButton("...");
    m_cartridgeBrowse->setMaximumWidth(45);
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
    
    // Add spacing between primary and piggyback cartridges
    cartridgeLayout->addSpacing(12);
    
    // Piggyback cartridge
    QVBoxLayout* cart2Layout = new QVBoxLayout();
    QLabel* cart2Label = new QLabel("Piggyback Cartridge:");
    cart2Label->setStyleSheet("font-weight: bold;");
    cart2Layout->addWidget(cart2Label);
    
    QHBoxLayout* cart2PathLayout = new QHBoxLayout();
    m_cartridge2EnabledCheck = new QCheckBox("Enable");
    cart2PathLayout->addWidget(m_cartridge2EnabledCheck);
    
    // Add spacing between checkbox and input field
    cart2PathLayout->addSpacing(8);
    
    m_cartridge2Path = new QLineEdit();
    m_cartridge2Path->setPlaceholderText("Select piggyback cartridge file");
    setupFilePathTooltip(m_cartridge2Path);
    cart2PathLayout->addWidget(m_cartridge2Path, 1);
    
    m_cartridge2Browse = new QPushButton("...");
    m_cartridge2Browse->setMaximumWidth(45);
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
    
    // Add spacing before cartridge options
    cartridgeLayout->addSpacing(8);
    
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
    
    // Floppy Disks Group
    QGroupBox* floppyGroup = new QGroupBox("Floppy Disk Drives");
    QVBoxLayout* floppyLayout = new QVBoxLayout(floppyGroup);
    floppyLayout->setSpacing(4); // Reduce spacing between disk drives
    
    for (int i = 0; i < 8; i++) {
        QString diskLabel = QString("D%1:").arg(i + 1);
        
        QHBoxLayout* diskLayout = new QHBoxLayout();
        diskLayout->setContentsMargins(0, 2, 0, 2); // Tighter margins
        
        m_diskEnabled[i] = new QCheckBox(diskLabel);
        m_diskEnabled[i]->setMinimumWidth(50);
        diskLayout->addWidget(m_diskEnabled[i]);
        
        m_diskPath[i] = new QLineEdit();
        m_diskPath[i]->setPlaceholderText("Select disk image (.atr, .xfd, .dcm, .pro, .atx)");
        setupFilePathTooltip(m_diskPath[i]);
        diskLayout->addWidget(m_diskPath[i], 1);
        
        m_diskBrowse[i] = new QPushButton("...");
        m_diskBrowse[i]->setMaximumWidth(45);
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
    cassetteLayout->setSpacing(4); // Reduce spacing
    
    QHBoxLayout* cassettePathLayout = new QHBoxLayout();
    m_cassetteEnabled = new QCheckBox("Enable Cassette");
    cassettePathLayout->addWidget(m_cassetteEnabled);
    
    m_cassettePath = new QLineEdit();
    m_cassettePath->setPlaceholderText("Select cassette image (.cas)");
    setupFilePathTooltip(m_cassettePath);
    cassettePathLayout->addWidget(m_cassettePath, 1);
    
    m_cassetteBrowse = new QPushButton("...");
    m_cassetteBrowse->setMaximumWidth(45);
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
    
    // Right column - Cartridge and Hard Drive
    QVBoxLayout* rightColumn = new QVBoxLayout();
    rightColumn->setSpacing(8); // Reduce spacing between groups
    
    // Add cartridge group to right column
    rightColumn->addWidget(cartridgeGroup);
    
    // Add spacing between cartridge and hard drive groups
    rightColumn->addSpacing(15);
    
    // Hard Drive Group
    QGroupBox* hdGroup = new QGroupBox("Hard Drive Emulation");
    QVBoxLayout* hdLayout = new QVBoxLayout(hdGroup);
    hdLayout->setSpacing(4); // Reduce spacing between hard drives
    
    for (int i = 0; i < 4; i++) {
        QString hdLabel = QString("H%1:").arg(i + 1);
        
        QHBoxLayout* hdDriveLayout = new QHBoxLayout();
        hdDriveLayout->setContentsMargins(0, 2, 0, 2); // Tighter margins
        
        m_hdEnabled[i] = new QCheckBox(hdLabel);
        m_hdEnabled[i]->setMinimumWidth(50);
        hdDriveLayout->addWidget(m_hdEnabled[i]);
        
        m_hdPath[i] = new QLineEdit();
        m_hdPath[i]->setPlaceholderText("Select directory path for hard drive emulation");
        setupFilePathTooltip(m_hdPath[i]);
        hdDriveLayout->addWidget(m_hdPath[i], 1);
        
        m_hdBrowse[i] = new QPushButton("...");
        m_hdBrowse[i]->setMaximumWidth(45);
        connect(m_hdBrowse[i], &QPushButton::clicked, [this, i]() { browseHardDriveDirectory(i); });
        hdDriveLayout->addWidget(m_hdBrowse[i]);
        
        hdLayout->addLayout(hdDriveLayout);
    }
    
    // Add spacing before hard drive options
    hdLayout->addSpacing(8);
    
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
    
    leftColumn->addWidget(specialGroup);
    leftColumn->addStretch();
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
    int speedIndex = settings.value("machine/emulationSpeedIndex", 1).toInt(); // Default to 1x (index 1)
    m_speedSlider->setValue(speedIndex);
    // Update label based on loaded index
    if (speedIndex == 0) {
        m_speedLabel->setText("0.5x");
    } else {
        m_speedLabel->setText(QString::number(speedIndex) + "x");
    }
    
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
    
    // Load Screen Display Options - use "full" by default to avoid cropping
    QString horizontalArea = settings.value("video/horizontalArea", "full").toString();
    for (int i = 0; i < m_horizontalArea->count(); ++i) {
        if (m_horizontalArea->itemData(i).toString() == horizontalArea) {
            m_horizontalArea->setCurrentIndex(i);
            break;
        }
    }
    
    QString verticalArea = settings.value("video/verticalArea", "full").toString();
    for (int i = 0; i < m_verticalArea->count(); ++i) {
        if (m_verticalArea->itemData(i).toString() == verticalArea) {
            m_verticalArea->setCurrentIndex(i);
            break;
        }
    }
    
    m_horizontalShift->setValue(settings.value("video/horizontalShift", 0).toInt());
    m_verticalShift->setValue(settings.value("video/verticalShift", 0).toInt());
    
    QString fitScreen = settings.value("video/fitScreen", "both").toString();
    for (int i = 0; i < m_fitScreen->count(); ++i) {
        if (m_fitScreen->itemData(i).toString() == fitScreen) {
            m_fitScreen->setCurrentIndex(i);
            break;
        }
    }
    
    m_show80Column->setChecked(settings.value("video/show80Column", false).toBool());
    m_vSyncEnabled->setChecked(settings.value("video/vSyncEnabled", false).toBool());
    
    QString palBlending = settings.value("video/palBlending", "simple").toString();
    for (int i = 0; i < m_palBlending->count(); ++i) {
        if (m_palBlending->itemData(i).toString() == palBlending) {
            m_palBlending->setCurrentIndex(i);
            break;
        }
    }
    
    // FUTURE: Load universal scanlines settings (commented out - not working)
    // m_scanlinesSlider->setValue(settings.value("video/scanlinesPercentage", 0).toInt());
    // m_scanlinesLabel->setText(QString("%1%").arg(m_scanlinesSlider->value()));
    // m_scanlinesInterpolation->setChecked(settings.value("video/scanlinesInterpolation", false).toBool());
    
    // NTSC artifacting is now handled by the main artifacting dropdown
    // QString ntscArtifacting = settings.value("video/ntscArtifacting", "standard").toString();
    // for (int i = 0; i < m_ntscArtifacting->count(); ++i) {
    //     if (m_ntscArtifacting->itemData(i).toString() == ntscArtifacting) {
    //         m_ntscArtifacting->setCurrentIndex(i);
    //         break;
    //     }
    // }
    
    m_ntscSharpness->setChecked(settings.value("video/ntscSharpness", true).toBool());
    
    // Load PAL Color Adjustment settings - blockSignals to prevent bouncing during load
    m_palSaturationSlider->blockSignals(true);
    m_palContrastSlider->blockSignals(true);
    m_palBrightnessSlider->blockSignals(true);
    m_palGammaSlider->blockSignals(true);
    m_palTintSlider->blockSignals(true);
    
    m_palSaturationSlider->setValue(settings.value("video/palSaturation", 0).toInt());
    m_palContrastSlider->setValue(settings.value("video/palContrast", 0).toInt());
    m_palBrightnessSlider->setValue(settings.value("video/palBrightness", 0).toInt());
    m_palGammaSlider->setValue(settings.value("video/palGamma", 100).toInt());
    m_palTintSlider->setValue(settings.value("video/palTint", 0).toInt());
    
    // Update labels manually since signals are blocked
    m_palSaturationLabel->setText(QString::number(m_palSaturationSlider->value() / 100.0, 'f', 2));
    m_palContrastLabel->setText(QString::number(m_palContrastSlider->value() / 100.0, 'f', 2));
    m_palBrightnessLabel->setText(QString::number(m_palBrightnessSlider->value() / 100.0, 'f', 2));
    m_palGammaLabel->setText(QString::number(m_palGammaSlider->value() / 100.0, 'f', 2));
    m_palTintLabel->setText(QString::number(m_palTintSlider->value()) + "°");
    
    m_palSaturationSlider->blockSignals(false);
    m_palContrastSlider->blockSignals(false);
    m_palBrightnessSlider->blockSignals(false);
    m_palGammaSlider->blockSignals(false);
    m_palTintSlider->blockSignals(false);
    
    // Load NTSC Color Adjustment settings - blockSignals to prevent bouncing during load
    m_ntscSaturationSlider->blockSignals(true);
    m_ntscContrastSlider->blockSignals(true);
    m_ntscBrightnessSlider->blockSignals(true);
    m_ntscGammaSlider->blockSignals(true);
    m_ntscTintSlider->blockSignals(true);
    
    m_ntscSaturationSlider->setValue(settings.value("video/ntscSaturation", 0).toInt());
    m_ntscContrastSlider->setValue(settings.value("video/ntscContrast", 0).toInt());
    m_ntscBrightnessSlider->setValue(settings.value("video/ntscBrightness", 0).toInt());
    m_ntscGammaSlider->setValue(settings.value("video/ntscGamma", 100).toInt());
    m_ntscTintSlider->setValue(settings.value("video/ntscTint", 0).toInt());
    
    // Update labels manually since signals are blocked
    m_ntscSaturationLabel->setText(QString::number(m_ntscSaturationSlider->value() / 100.0, 'f', 2));
    m_ntscContrastLabel->setText(QString::number(m_ntscContrastSlider->value() / 100.0, 'f', 2));
    m_ntscBrightnessLabel->setText(QString::number(m_ntscBrightnessSlider->value() / 100.0, 'f', 2));
    m_ntscGammaLabel->setText(QString::number(m_ntscGammaSlider->value() / 100.0, 'f', 2));
    m_ntscTintLabel->setText(QString::number(m_ntscTintSlider->value()) + "°");
    
    m_ntscSaturationSlider->blockSignals(false);
    m_ntscContrastSlider->blockSignals(false);
    m_ntscBrightnessSlider->blockSignals(false);
    m_ntscGammaSlider->blockSignals(false);
    m_ntscTintSlider->blockSignals(false);
    
    // Load Input Configuration
    m_joystickEnabled->setChecked(settings.value("input/joystickEnabled", true).toBool());
    m_joystick0Hat->setChecked(settings.value("input/joystick0Hat", false).toBool());
    m_joystick1Hat->setChecked(settings.value("input/joystick1Hat", false).toBool());
    m_joystick2Hat->setChecked(settings.value("input/joystick2Hat", false).toBool());
    m_joystick3Hat->setChecked(settings.value("input/joystick3Hat", false).toBool());
    m_joyDistinct->setChecked(settings.value("input/joyDistinct", false).toBool());
    m_kbdJoy0Enabled->setChecked(settings.value("input/kbdJoy0Enabled", true).toBool());  // Default true to match SDL default
    m_kbdJoy1Enabled->setChecked(settings.value("input/kbdJoy1Enabled", false).toBool()); // Default false to match SDL default
    m_swapJoysticks->setChecked(settings.value("input/swapJoysticks", false).toBool());   // Default false: Joy0=Numpad, Joy1=WASD
    m_grabMouse->setChecked(settings.value("input/grabMouse", false).toBool());
    m_mouseDevice->setText(settings.value("input/mouseDevice", "").toString());
    m_keyboardToggle->setChecked(settings.value("input/keyboardToggle", false).toBool());
    m_keyboardLeds->setChecked(settings.value("input/keyboardLeds", false).toBool());
    
    // Load Media Configuration
    // Floppy Disks
    for (int i = 0; i < 8; i++) {
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
    settings.setValue("machine/emulationSpeedIndex", m_speedSlider->value());
    
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
    
    // Save Screen Display Options
    settings.setValue("video/horizontalArea", m_horizontalArea->currentData().toString());
    settings.setValue("video/verticalArea", m_verticalArea->currentData().toString());
    settings.setValue("video/horizontalShift", m_horizontalShift->value());
    settings.setValue("video/verticalShift", m_verticalShift->value());
    settings.setValue("video/fitScreen", m_fitScreen->currentData().toString());
    settings.setValue("video/show80Column", m_show80Column->isChecked());
    settings.setValue("video/vSyncEnabled", m_vSyncEnabled->isChecked());
    settings.setValue("video/palBlending", m_palBlending->currentData().toString());
    // FUTURE: Save universal scanlines settings (commented out - not working)
    // settings.setValue("video/scanlinesPercentage", m_scanlinesSlider->value());
    // settings.setValue("video/scanlinesInterpolation", m_scanlinesInterpolation->isChecked());
    // NTSC artifacting is now handled by the main artifacting dropdown
    // settings.setValue("video/ntscArtifacting", m_ntscArtifacting->currentData().toString());
    settings.setValue("video/ntscSharpness", m_ntscSharpness->isChecked());
    
    // Save PAL Color Adjustment settings
    settings.setValue("video/palSaturation", m_palSaturationSlider->value());
    settings.setValue("video/palContrast", m_palContrastSlider->value());
    settings.setValue("video/palBrightness", m_palBrightnessSlider->value());
    settings.setValue("video/palGamma", m_palGammaSlider->value());
    settings.setValue("video/palTint", m_palTintSlider->value());
    
    // Save NTSC Color Adjustment settings
    settings.setValue("video/ntscSaturation", m_ntscSaturationSlider->value());
    settings.setValue("video/ntscContrast", m_ntscContrastSlider->value());
    settings.setValue("video/ntscBrightness", m_ntscBrightnessSlider->value());
    settings.setValue("video/ntscGamma", m_ntscGammaSlider->value());
    settings.setValue("video/ntscTint", m_ntscTintSlider->value());
    
    // Save Input Configuration
    settings.setValue("input/joystickEnabled", m_joystickEnabled->isChecked());
    settings.setValue("input/joystick0Hat", m_joystick0Hat->isChecked());
    settings.setValue("input/joystick1Hat", m_joystick1Hat->isChecked());
    settings.setValue("input/joystick2Hat", m_joystick2Hat->isChecked());
    settings.setValue("input/joystick3Hat", m_joystick3Hat->isChecked());
    settings.setValue("input/joyDistinct", m_joyDistinct->isChecked());
    settings.setValue("input/kbdJoy0Enabled", m_kbdJoy0Enabled->isChecked());
    settings.setValue("input/kbdJoy1Enabled", m_kbdJoy1Enabled->isChecked());
    settings.setValue("input/swapJoysticks", m_swapJoysticks->isChecked());
    settings.setValue("input/grabMouse", m_grabMouse->isChecked());
    settings.setValue("input/mouseDevice", m_mouseDevice->text());
    settings.setValue("input/keyboardToggle", m_keyboardToggle->isChecked());
    settings.setValue("input/keyboardLeds", m_keyboardLeds->isChecked());
    
    // Save Media Configuration
    // Floppy Disks
    for (int i = 0; i < 8; i++) {
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
    
    // Apply cartridge settings
    if (m_cartridgeEnabledCheck->isChecked() && !m_cartridgePath->text().isEmpty()) {
        QString cartridgePath = m_cartridgePath->text();
        qDebug() << "Loading cartridge:" << cartridgePath;
        
        if (m_emulator->loadFile(cartridgePath)) {
            qDebug() << "Successfully loaded cartridge:" << cartridgePath;
        } else {
            qDebug() << "Failed to load cartridge:" << cartridgePath;
        }
    }
    
    // Apply piggyback cartridge settings 
    if (m_cartridge2EnabledCheck->isChecked() && !m_cartridge2Path->text().isEmpty()) {
        QString cartridge2Path = m_cartridge2Path->text();
        qDebug() << "Loading piggyback cartridge:" << cartridge2Path;
        
        if (m_emulator->loadFile(cartridge2Path)) {
            qDebug() << "Successfully loaded piggyback cartridge:" << cartridge2Path;
        } else {
            qDebug() << "Failed to load piggyback cartridge:" << cartridge2Path;
        }
    }
    
    // Mount/unmount disk images for D1-D8
    for (int i = 0; i < 8; i++) {
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
    // Check which settings require emulator restart vs live updates
    bool needsRestart = false;
    
    // Check for changes that require emulator restart
    if (m_machineTypeCombo->currentData().toString() != m_emulator->getMachineType() ||
        m_videoSystemCombo->currentData().toString() != m_emulator->getVideoSystem() ||
        m_basicEnabledCheck->isChecked() != m_emulator->isBasicEnabled() ||
        m_altirraOSCheck->isChecked() != m_emulator->isAltirraOSEnabled()) {
        needsRestart = true;
    }
    
    // Apply joystick settings immediately (no restart needed)
    if (m_emulator) {
        bool joy0Enabled = m_kbdJoy0Enabled->isChecked();
        bool joy1Enabled = m_kbdJoy1Enabled->isChecked();
        bool swapped = m_swapJoysticks->isChecked();
        
        // Apply joystick settings directly for immediate effect
        m_emulator->setKbdJoy0Enabled(joy0Enabled);
        m_emulator->setKbdJoy1Enabled(joy1Enabled);
        m_emulator->setJoysticksSwapped(swapped);
        
        qDebug() << "Applied joystick settings live - Joy0:" << joy0Enabled << "Joy1:" << joy1Enabled << "Swap:" << swapped;
    }
    
    saveSettings();
    
    if (needsRestart) {
        // Full restart needed for machine/video/OS settings
        m_emulator->shutdown();
        
        // Get artifact settings from UI
        QString artifactMode = m_artifactingMode->currentData().toString();
        
        if (m_emulator->initializeWithInputConfig(
                m_emulator->isBasicEnabled(), 
                m_emulator->getMachineType(), 
                m_emulator->getVideoSystem(),
                artifactMode,
                "tv", "tv", 0, 0, "both", false, false,
                m_kbdJoy0Enabled->isChecked(),
                m_kbdJoy1Enabled->isChecked(),
                m_swapJoysticks->isChecked())) {
            qDebug() << "Emulator restarted with new settings";
            
            // Reapply media settings after restart
            applyMediaSettings();
        } else {
            qDebug() << "Failed to restart emulator with new settings";
            return;
        }
    }
    
    // Apply speed setting (can be applied live)
    if (m_emulator) {
        int speedIndex = m_speedSlider->value();
        int percentage = (speedIndex == 0) ? 50 : speedIndex * 100; // 0.5x = 50%, others = index * 100
        m_emulator->setEmulationSpeed(percentage);
    }
    
    emit settingsChanged();
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
    
    // Only enable/disable the group boxes - avoid individual slider enable/disable
    // which might be causing value resets
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
    m_speedSlider->setValue(1);  // Default to 1x speed (index 1)
    m_speedLabel->setText("1x");
    
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
    
    // Screen Display Options defaults
    m_horizontalArea->setCurrentIndex(1);  // TV Safe Area
    m_verticalArea->setCurrentIndex(1);    // TV Safe Area
    m_horizontalShift->setValue(0);        // No shift
    m_verticalShift->setValue(0);          // No shift
    m_fitScreen->setCurrentIndex(2);       // Fit Both
    m_show80Column->setChecked(false);     // Standard display
    m_vSyncEnabled->setChecked(false);     // VSync off (for performance)
    
    m_palBlending->setCurrentIndex(1);     // Simple
    // FUTURE: Universal scanlines defaults (commented out - not working)
    // m_scanlinesSlider->setValue(0);
    // m_scanlinesLabel->setText("0%");
    // m_scanlinesInterpolation->setChecked(false);
    // NTSC artifacting is now handled by the main artifacting dropdown
    // m_ntscArtifacting->setCurrentIndex(0); // Standard
    m_ntscSharpness->setChecked(true);
    
    // PAL Color Adjustment defaults
    m_palSaturationSlider->setValue(0);  // 0.0
    m_palContrastSlider->setValue(0);    // 0.0
    m_palBrightnessSlider->setValue(0);  // 0.0
    m_palGammaSlider->setValue(100);     // 1.0
    m_palTintSlider->setValue(0);        // 0°
    
    // NTSC Color Adjustment defaults
    m_ntscSaturationSlider->setValue(0); // 0.0
    m_ntscContrastSlider->setValue(0);   // 0.0
    m_ntscBrightnessSlider->setValue(0); // 0.0
    m_ntscGammaSlider->setValue(100);    // 1.0
    m_ntscTintSlider->setValue(0);       // 0°
    
    // Input Configuration defaults
    m_joystickEnabled->setChecked(true);     // Joystick enabled by default
    m_joystick0Hat->setChecked(false);
    m_joystick1Hat->setChecked(false);
    m_joystick2Hat->setChecked(false);
    m_joystick3Hat->setChecked(false);
    m_joyDistinct->setChecked(false);
    m_kbdJoy0Enabled->setChecked(true);  // Default true to match SDL default
    m_kbdJoy1Enabled->setChecked(false); // Default false to match SDL default
    m_swapJoysticks->setChecked(false);  // Default false: Joy0=Numpad, Joy1=WASD
    m_grabMouse->setChecked(false);
    m_mouseDevice->clear();
    m_keyboardToggle->setChecked(false);
    m_keyboardLeds->setChecked(false);
    
    // Media Configuration defaults
    // Floppy Disks - all disabled by default
    for (int i = 0; i < 8; i++) {
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

// Profile management implementation
void SettingsDialog::createProfileSection()
{
    m_profileWidget = new ProfileSelectionWidget(m_profileManager, this);
    
    // Connect profile signals
    connect(m_profileWidget, &ProfileSelectionWidget::profileChangeRequested,
            this, &SettingsDialog::onProfileChangeRequested);
    connect(m_profileWidget, &ProfileSelectionWidget::saveCurrentProfile,
            this, &SettingsDialog::onSaveCurrentProfile);
    connect(m_profileWidget, &ProfileSelectionWidget::loadProfile,
            this, &SettingsDialog::onLoadProfile);
}

void SettingsDialog::onProfileChangeRequested(const QString& profileName)
{
    // Update profile manager's current profile
    m_profileManager->setCurrentProfileName(profileName);
    qDebug() << "Profile selection changed to:" << profileName;
}

void SettingsDialog::onSaveCurrentProfile(const QString& profileName)
{
    ConfigurationProfile profile = getCurrentUIState();
    profile.name = profileName;
    profile.description = QString("Saved on %1").arg(QDateTime::currentDateTime().toString("MMM dd, yyyy"));
    
    bool success = m_profileManager->saveProfile(profileName, profile);
    if (success) {
        qDebug() << "Profile saved successfully:" << profileName;
    } else {
        qWarning() << "Failed to save profile:" << profileName;
    }
}

void SettingsDialog::onLoadProfile(const QString& profileName)
{
    ConfigurationProfile profile = m_profileManager->loadProfile(profileName);
    if (profile.isValid()) {
        loadProfileToUI(profile);
        m_profileManager->setCurrentProfileName(profileName);
        qDebug() << "Profile loaded successfully:" << profileName;
    } else {
        qWarning() << "Failed to load profile:" << profileName;
    }
}

ConfigurationProfile SettingsDialog::getCurrentUIState() const
{
    ConfigurationProfile profile;
    
    // Machine Configuration
    profile.machineType = m_machineTypeCombo->currentData().toString();
    profile.videoSystem = m_videoSystemCombo->currentData().toString();
    profile.basicEnabled = m_basicEnabledCheck->isChecked();
    profile.altirraOSEnabled = m_altirraOSCheck->isChecked();
    profile.osRomPath = m_osRomPath->text();
    profile.basicRomPath = m_basicRomPath->text();
    
    // Memory Configuration
    profile.enable800Ram = m_enable800RamCheck->isChecked();
    profile.mosaicSize = m_mosaicSizeSpinBox->value();
    profile.axlonSize = m_axlonSizeSpinBox->value();
    profile.axlonShadow = m_axlonShadowCheck->isChecked();
    profile.enableMapRam = m_enableMapRamCheck->isChecked();
    
    // Performance
    profile.turboMode = m_turboModeCheck->isChecked();
    profile.emulationSpeedIndex = m_speedSlider->value();
    
    // Audio Configuration
    profile.audioEnabled = m_soundEnabled->isChecked();
    profile.audioFrequency = m_audioFrequency->currentData().toInt();
    profile.audioBits = m_audioBits->currentData().toInt();
    profile.audioVolume = m_volumeSlider->value();
    profile.audioBufferLength = m_bufferLengthSpinBox->value();
    profile.audioLatency = m_audioLatencySpinBox->value();
    profile.consoleSound = m_consoleSound->isChecked();
    profile.serialSound = m_serialSound->isChecked();
    profile.stereoPokey = m_stereoPokey->isChecked();
    
    // Video Configuration
    profile.artifactingMode = m_artifactingMode->currentData().toString();
    profile.showFPS = m_showFPS->isChecked();
    profile.scalingFilter = m_scalingFilter->isChecked();
    profile.keepAspectRatio = m_keepAspectRatio->isChecked();
    profile.fullscreenMode = m_fullscreenMode->isChecked();
    
    // Screen Display Options
    profile.horizontalArea = m_horizontalArea->currentData().toString();
    profile.verticalArea = m_verticalArea->currentData().toString();
    profile.horizontalShift = m_horizontalShift->value();
    profile.verticalShift = m_verticalShift->value();
    profile.fitScreen = m_fitScreen->currentData().toString();
    profile.show80Column = m_show80Column->isChecked();
    profile.vSyncEnabled = m_vSyncEnabled->isChecked();
    
    // Color Settings
    profile.palSaturation = m_palSaturationSlider->value();
    profile.palContrast = m_palContrastSlider->value();
    profile.palBrightness = m_palBrightnessSlider->value();
    profile.palGamma = m_palGammaSlider->value();
    profile.palTint = m_palTintSlider->value();
    
    profile.ntscSaturation = m_ntscSaturationSlider->value();
    profile.ntscContrast = m_ntscContrastSlider->value();
    profile.ntscBrightness = m_ntscBrightnessSlider->value();
    profile.ntscGamma = m_ntscGammaSlider->value();
    profile.ntscTint = m_ntscTintSlider->value();
    
    // Input Configuration
    profile.joystickEnabled = m_joystickEnabled->isChecked();
    profile.joystick0Hat = m_joystick0Hat->isChecked();
    profile.joystick1Hat = m_joystick1Hat->isChecked();
    profile.joystick2Hat = m_joystick2Hat->isChecked();
    profile.joystick3Hat = m_joystick3Hat->isChecked();
    profile.joyDistinct = m_joyDistinct->isChecked();
    profile.kbdJoy0Enabled = m_kbdJoy0Enabled->isChecked();
    profile.kbdJoy1Enabled = m_kbdJoy1Enabled->isChecked();
    profile.swapJoysticks = m_swapJoysticks->isChecked();
    profile.grabMouse = m_grabMouse->isChecked();
    profile.mouseDevice = m_mouseDevice->text();
    profile.keyboardToggle = m_keyboardToggle->isChecked();
    profile.keyboardLeds = m_keyboardLeds->isChecked();
    
    // Cartridge Configuration
    profile.primaryCartridge.enabled = m_cartridgeEnabledCheck->isChecked();
    profile.primaryCartridge.path = m_cartridgePath->text();
    profile.primaryCartridge.type = m_cartridgeTypeCombo->currentData().toInt();
    profile.piggybackCartridge.enabled = m_cartridge2EnabledCheck->isChecked();
    profile.piggybackCartridge.path = m_cartridge2Path->text();
    profile.piggybackCartridge.type = m_cartridge2TypeCombo->currentData().toInt();
    profile.cartridgeAutoReboot = m_cartridgeAutoRebootCheck->isChecked();
    
    // Disk Configuration
    for (int i = 0; i < 8; i++) {
        profile.disks[i].enabled = m_diskEnabled[i]->isChecked();
        profile.disks[i].path = m_diskPath[i]->text();
        profile.disks[i].readOnly = m_diskReadOnly[i]->isChecked();
    }
    
    // Cassette Configuration
    profile.cassette.enabled = m_cassetteEnabled->isChecked();
    profile.cassette.path = m_cassettePath->text();
    profile.cassette.readOnly = m_cassetteReadOnly->isChecked();
    profile.cassette.bootTape = m_cassetteBootTape->isChecked();
    
    // Hard Drive Configuration
    for (int i = 0; i < 4; i++) {
        profile.hardDrives[i].enabled = m_hdEnabled[i]->isChecked();
        profile.hardDrives[i].path = m_hdPath[i]->text();
    }
    profile.hdReadOnly = m_hdReadOnly->isChecked();
    profile.hdDeviceName = m_hdDeviceName->text();
    
    // Special Devices
    profile.rDeviceName = m_rDeviceName->text();
    profile.netSIOEnabled = m_netSIOEnabled->isChecked();
    profile.rtimeEnabled = m_rtimeEnabled->isChecked();
    
    // Hardware Extensions
    profile.xep80Enabled = m_xep80Enabled->isChecked();
    profile.af80Enabled = m_af80Enabled->isChecked();
    profile.bit3Enabled = m_bit3Enabled->isChecked();
    profile.atari1400Enabled = m_atari1400Enabled->isChecked();
    profile.atari1450Enabled = m_atari1450Enabled->isChecked();
    profile.proto80Enabled = m_proto80Enabled->isChecked();
    profile.voiceboxEnabled = m_voiceboxEnabled->isChecked();
    profile.sioAcceleration = m_sioAcceleration->isChecked();
    
    return profile;
}

void SettingsDialog::loadProfileToUI(const ConfigurationProfile& profile)
{
    // Block signals to prevent cascading updates
    const bool wasBlocked = blockSignals(true);
    
    // Machine Configuration
    for (int i = 0; i < m_machineTypeCombo->count(); ++i) {
        if (m_machineTypeCombo->itemData(i).toString() == profile.machineType) {
            m_machineTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    for (int i = 0; i < m_videoSystemCombo->count(); ++i) {
        if (m_videoSystemCombo->itemData(i).toString() == profile.videoSystem) {
            m_videoSystemCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_basicEnabledCheck->setChecked(profile.basicEnabled);
    m_altirraOSCheck->setChecked(profile.altirraOSEnabled);
    m_osRomPath->setText(profile.osRomPath);
    m_basicRomPath->setText(profile.basicRomPath);
    
    // Memory Configuration
    m_enable800RamCheck->setChecked(profile.enable800Ram);
    m_mosaicSizeSpinBox->setValue(profile.mosaicSize);
    m_axlonSizeSpinBox->setValue(profile.axlonSize);
    m_axlonShadowCheck->setChecked(profile.axlonShadow);
    m_enableMapRamCheck->setChecked(profile.enableMapRam);
    
    // Performance
    m_turboModeCheck->setChecked(profile.turboMode);
    m_speedSlider->setValue(profile.emulationSpeedIndex);
    
    // Audio Configuration
    m_soundEnabled->setChecked(profile.audioEnabled);
    
    for (int i = 0; i < m_audioFrequency->count(); ++i) {
        if (m_audioFrequency->itemData(i).toInt() == profile.audioFrequency) {
            m_audioFrequency->setCurrentIndex(i);
            break;
        }
    }
    
    for (int i = 0; i < m_audioBits->count(); ++i) {
        if (m_audioBits->itemData(i).toInt() == profile.audioBits) {
            m_audioBits->setCurrentIndex(i);
            break;
        }
    }
    
    m_volumeSlider->setValue(profile.audioVolume);
    m_volumeLabel->setText(QString("%1%").arg(profile.audioVolume));
    m_bufferLengthSpinBox->setValue(profile.audioBufferLength);
    m_audioLatencySpinBox->setValue(profile.audioLatency);
    m_consoleSound->setChecked(profile.consoleSound);
    m_serialSound->setChecked(profile.serialSound);
    m_stereoPokey->setChecked(profile.stereoPokey);
    
    // Video Configuration
    for (int i = 0; i < m_artifactingMode->count(); ++i) {
        if (m_artifactingMode->itemData(i).toString() == profile.artifactingMode) {
            m_artifactingMode->setCurrentIndex(i);
            break;
        }
    }
    
    m_showFPS->setChecked(profile.showFPS);
    m_scalingFilter->setChecked(profile.scalingFilter);
    m_keepAspectRatio->setChecked(profile.keepAspectRatio);
    m_fullscreenMode->setChecked(profile.fullscreenMode);
    
    // Screen Display Options
    for (int i = 0; i < m_horizontalArea->count(); ++i) {
        if (m_horizontalArea->itemData(i).toString() == profile.horizontalArea) {
            m_horizontalArea->setCurrentIndex(i);
            break;
        }
    }
    
    for (int i = 0; i < m_verticalArea->count(); ++i) {
        if (m_verticalArea->itemData(i).toString() == profile.verticalArea) {
            m_verticalArea->setCurrentIndex(i);
            break;
        }
    }
    
    m_horizontalShift->setValue(profile.horizontalShift);
    m_verticalShift->setValue(profile.verticalShift);
    
    for (int i = 0; i < m_fitScreen->count(); ++i) {
        if (m_fitScreen->itemData(i).toString() == profile.fitScreen) {
            m_fitScreen->setCurrentIndex(i);
            break;
        }
    }
    
    m_show80Column->setChecked(profile.show80Column);
    m_vSyncEnabled->setChecked(profile.vSyncEnabled);
    
    // Color Settings - block individual slider signals to prevent visual bouncing
    // PAL Color Settings
    m_palSaturationSlider->blockSignals(true);
    m_palContrastSlider->blockSignals(true);
    m_palBrightnessSlider->blockSignals(true);
    m_palGammaSlider->blockSignals(true);
    m_palTintSlider->blockSignals(true);
    
    m_palSaturationSlider->setValue(profile.palSaturation);
    m_palContrastSlider->setValue(profile.palContrast);
    m_palBrightnessSlider->setValue(profile.palBrightness);
    m_palGammaSlider->setValue(profile.palGamma);
    m_palTintSlider->setValue(profile.palTint);
    
    // Update labels manually since signals are blocked
    m_palSaturationLabel->setText(QString::number(profile.palSaturation / 100.0, 'f', 2));
    m_palContrastLabel->setText(QString::number(profile.palContrast / 100.0, 'f', 2));
    m_palBrightnessLabel->setText(QString::number(profile.palBrightness / 100.0, 'f', 2));
    m_palGammaLabel->setText(QString::number(profile.palGamma / 100.0, 'f', 2));
    m_palTintLabel->setText(QString::number(profile.palTint) + "°");
    
    m_palSaturationSlider->blockSignals(false);
    m_palContrastSlider->blockSignals(false);
    m_palBrightnessSlider->blockSignals(false);
    m_palGammaSlider->blockSignals(false);
    m_palTintSlider->blockSignals(false);
    
    // NTSC Color Settings
    m_ntscSaturationSlider->blockSignals(true);
    m_ntscContrastSlider->blockSignals(true);
    m_ntscBrightnessSlider->blockSignals(true);
    m_ntscGammaSlider->blockSignals(true);
    m_ntscTintSlider->blockSignals(true);
    
    m_ntscSaturationSlider->setValue(profile.ntscSaturation);
    m_ntscContrastSlider->setValue(profile.ntscContrast);
    m_ntscBrightnessSlider->setValue(profile.ntscBrightness);
    m_ntscGammaSlider->setValue(profile.ntscGamma);
    m_ntscTintSlider->setValue(profile.ntscTint);
    
    // Update labels manually since signals are blocked
    m_ntscSaturationLabel->setText(QString::number(profile.ntscSaturation / 100.0, 'f', 2));
    m_ntscContrastLabel->setText(QString::number(profile.ntscContrast / 100.0, 'f', 2));
    m_ntscBrightnessLabel->setText(QString::number(profile.ntscBrightness / 100.0, 'f', 2));
    m_ntscGammaLabel->setText(QString::number(profile.ntscGamma / 100.0, 'f', 2));
    m_ntscTintLabel->setText(QString::number(profile.ntscTint) + "°");
    
    m_ntscSaturationSlider->blockSignals(false);
    m_ntscContrastSlider->blockSignals(false);
    m_ntscBrightnessSlider->blockSignals(false);
    m_ntscGammaSlider->blockSignals(false);
    m_ntscTintSlider->blockSignals(false);
    
    // Input Configuration
    m_joystickEnabled->setChecked(profile.joystickEnabled);
    m_joystick0Hat->setChecked(profile.joystick0Hat);
    m_joystick1Hat->setChecked(profile.joystick1Hat);
    m_joystick2Hat->setChecked(profile.joystick2Hat);
    m_joystick3Hat->setChecked(profile.joystick3Hat);
    m_joyDistinct->setChecked(profile.joyDistinct);
    m_kbdJoy0Enabled->setChecked(profile.kbdJoy0Enabled);
    m_kbdJoy1Enabled->setChecked(profile.kbdJoy1Enabled);
    m_swapJoysticks->setChecked(profile.swapJoysticks);
    m_grabMouse->setChecked(profile.grabMouse);
    m_mouseDevice->setText(profile.mouseDevice);
    m_keyboardToggle->setChecked(profile.keyboardToggle);
    m_keyboardLeds->setChecked(profile.keyboardLeds);
    
    // Cartridge Configuration
    m_cartridgeEnabledCheck->setChecked(profile.primaryCartridge.enabled);
    m_cartridgePath->setText(profile.primaryCartridge.path);
    for (int i = 0; i < m_cartridgeTypeCombo->count(); ++i) {
        if (m_cartridgeTypeCombo->itemData(i).toInt() == profile.primaryCartridge.type) {
            m_cartridgeTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_cartridge2EnabledCheck->setChecked(profile.piggybackCartridge.enabled);
    m_cartridge2Path->setText(profile.piggybackCartridge.path);
    for (int i = 0; i < m_cartridge2TypeCombo->count(); ++i) {
        if (m_cartridge2TypeCombo->itemData(i).toInt() == profile.piggybackCartridge.type) {
            m_cartridge2TypeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_cartridgeAutoRebootCheck->setChecked(profile.cartridgeAutoReboot);
    
    // Disk Configuration
    for (int i = 0; i < 8; i++) {
        m_diskEnabled[i]->setChecked(profile.disks[i].enabled);
        m_diskPath[i]->setText(profile.disks[i].path);
        m_diskReadOnly[i]->setChecked(profile.disks[i].readOnly);
    }
    
    // Cassette Configuration
    m_cassetteEnabled->setChecked(profile.cassette.enabled);
    m_cassettePath->setText(profile.cassette.path);
    m_cassetteReadOnly->setChecked(profile.cassette.readOnly);
    m_cassetteBootTape->setChecked(profile.cassette.bootTape);
    
    // Hard Drive Configuration
    for (int i = 0; i < 4; i++) {
        m_hdEnabled[i]->setChecked(profile.hardDrives[i].enabled);
        m_hdPath[i]->setText(profile.hardDrives[i].path);
    }
    m_hdReadOnly->setChecked(profile.hdReadOnly);
    m_hdDeviceName->setText(profile.hdDeviceName);
    
    // Special Devices
    m_rDeviceName->setText(profile.rDeviceName);
    m_netSIOEnabled->setChecked(profile.netSIOEnabled);
    m_rtimeEnabled->setChecked(profile.rtimeEnabled);
    
    // Hardware Extensions
    m_xep80Enabled->setChecked(profile.xep80Enabled);
    m_af80Enabled->setChecked(profile.af80Enabled);
    m_bit3Enabled->setChecked(profile.bit3Enabled);
    m_atari1400Enabled->setChecked(profile.atari1400Enabled);
    m_atari1450Enabled->setChecked(profile.atari1450Enabled);
    m_proto80Enabled->setChecked(profile.proto80Enabled);
    m_voiceboxEnabled->setChecked(profile.voiceboxEnabled);
    m_sioAcceleration->setChecked(profile.sioAcceleration);
    
    // Restore signal blocking state
    blockSignals(wasBlocked);
    
    // Update dependent controls
    updateVideoSystemDependentControls();
    
    qDebug() << "Profile loaded to UI successfully";
}