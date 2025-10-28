/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifdef _WIN32
#include "windows_compat.h"
#endif

#include "mainwindow.h"
#include "version.h"  // For FUJISAN_FULL_VERSION_STRING
#include <QApplication>
#include <QMessageBox>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QDebug>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QFileDialog>
#include <QFileInfo>

// Debug control - uncomment to enable verbose disk I/O logging
// #define DEBUG_DISK_IO

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_emulator(new AtariEmulator(this))
    , m_emulatorWidget(nullptr)
    , m_tcpServer(new TCPServer(m_emulator, this, this))
    , m_keepAspectRatio(true)
    , m_startInFullscreen(false)
    , m_isInCustomFullscreen(false)
    , m_fullscreenWidget(nullptr)
    , m_pasteTimer(new QTimer(this))
    , m_pasteIndex(0)
    , m_originalEmulationSpeed(100)
    , m_profileManager(new ConfigurationProfileManager(this))
    , m_diskDrive1(nullptr)
    , m_mediaPeripheralsDock(nullptr)
    , m_mediaPeripheralsDockWidget(nullptr)
    , m_mediaToggleButton(nullptr)
    , m_logoLabel(nullptr)
{
    setWindowTitle(QString("Fujisan %1").arg(FUJISAN_VERSION));
    setMinimumSize(800, 600);
    resize(1440, 960);

#ifdef Q_OS_MACOS
    // Disable automatic macOS fullscreen to prevent duplicate menu items
    setWindowFlags(windowFlags() & ~Qt::WindowFullscreenButtonHint);
#endif

    // Style dock areas to be black so they blend with emulator widget background
    setStyleSheet("QMainWindow::separator { background: black; width: 0px; height: 0px; }");

    createMenus();
    createToolBar();
    createEmulatorWidget();
    createDebugger();
    createMediaPeripheralsDock();

    // Enable hybrid disk activity system
    m_emulator->setDiskActivityCallback([](int driveNumber, bool isWriting) {
        // Activity callback handled via Qt signals
    });

    // Setup paste timer
    m_pasteTimer->setSingleShot(false);
    m_pasteTimer->setInterval(75); // 75ms for character/clear cycle
    connect(m_pasteTimer, &QTimer::timeout, this, &MainWindow::sendNextCharacter);

    // Load initial settings and initialize emulator with them
    loadInitialSettings();
    loadVideoSettings();

    // Show initial status message
    statusBar()->showMessage("Fujisan ready", 3000);

    // Auto-start TCP Server if enabled in settings
    QSettings settings;
    bool tcpEnabled = settings.value("emulator/tcpServerEnabled", true).toBool();
    int tcpPort = settings.value("emulator/tcpServerPort", 6502).toInt();
    
    if (tcpEnabled && m_tcpServer && !m_tcpServer->isRunning()) {
        bool success = m_tcpServer->startServer(tcpPort);
        if (success) {
            m_tcpServerAction->setChecked(true);
            m_tcpServerAction->setText("&TCP Server (Running)");
            qDebug() << "TCP Server auto-started on localhost:" << tcpPort;
        } else {
            qDebug() << "Failed to auto-start TCP Server on port" << tcpPort;
        }
    }

    qDebug() << "Fujisan initialized successfully";
}

MainWindow::~MainWindow()
{
    if (m_emulator) {
        m_emulator->shutdown();
    }
}

void MainWindow::createMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");

    m_loadRomAction = new QAction("&Load File...", this);
    m_loadRomAction->setShortcut(QKeySequence::Open);
    connect(m_loadRomAction, &QAction::triggered, this, &MainWindow::loadRom);
    fileMenu->addAction(m_loadRomAction);

    fileMenu->addSeparator();

    m_coldBootAction = new QAction("&Cold Reset", this);
    connect(m_coldBootAction, &QAction::triggered, this, &MainWindow::coldBoot);
    fileMenu->addAction(m_coldBootAction);

    m_warmBootAction = new QAction("&Warm Reset", this);
    connect(m_warmBootAction, &QAction::triggered, this, &MainWindow::warmBoot);
    fileMenu->addAction(m_warmBootAction);

    fileMenu->addSeparator();

    m_pauseAction = new QAction("&Pause", this);
    m_pauseAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_P));
    m_pauseAction->setCheckable(true);
    connect(m_pauseAction, &QAction::triggered, this, &MainWindow::togglePause);
    fileMenu->addAction(m_pauseAction);

    fileMenu->addSeparator();

    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(m_exitAction);

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu("&Edit");

    m_pasteAction = new QAction("&Paste Text", this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setToolTip("Paste clipboard text into emulator (Ctrl+V)");
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::pasteText);
    editMenu->addAction(m_pasteAction);

    // System menu
    QMenu* systemMenu = menuBar()->addMenu("&System");

    m_basicAction = new QAction("Enable &BASIC", this);
    m_basicAction->setCheckable(true);
    m_basicAction->setChecked(m_emulator->isBasicEnabled());
    connect(m_basicAction, &QAction::toggled, this, &MainWindow::toggleBasic);
    systemMenu->addAction(m_basicAction);

    m_altirraOSAction = new QAction("Use &Altirra OS", this);
    m_altirraOSAction->setCheckable(true);
    m_altirraOSAction->setChecked(m_emulator->isAltirraOSEnabled());
    connect(m_altirraOSAction, &QAction::toggled, this, &MainWindow::toggleAltirraOS);
    systemMenu->addAction(m_altirraOSAction);

    m_altirraBASICAction = new QAction("Use Altirra &BASIC", this);
    m_altirraBASICAction->setCheckable(true);
    m_altirraBASICAction->setChecked(m_emulator->isAltirraBASICEnabled());
    connect(m_altirraBASICAction, &QAction::toggled, this, &MainWindow::toggleAltirraBASIC);
    systemMenu->addAction(m_altirraBASICAction);

    systemMenu->addSeparator();
    
    // State save/load actions
    m_quickSaveStateAction = new QAction("&Quick Save State", this);
    m_quickSaveStateAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F5));
    m_quickSaveStateAction->setToolTip("Quick save current state (Shift+F5)");
    connect(m_quickSaveStateAction, &QAction::triggered, this, &MainWindow::quickSaveState);
    systemMenu->addAction(m_quickSaveStateAction);
    
    m_quickLoadStateAction = new QAction("Quick &Load State", this);
    m_quickLoadStateAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F9));
    m_quickLoadStateAction->setToolTip("Quick load saved state (Shift+F9)");
    connect(m_quickLoadStateAction, &QAction::triggered, this, &MainWindow::quickLoadState);
    systemMenu->addAction(m_quickLoadStateAction);
    
    systemMenu->addSeparator();
    
    m_saveStateAction = new QAction("Save State...", this);
    m_saveStateAction->setToolTip("Save current state to file");
    connect(m_saveStateAction, &QAction::triggered, this, &MainWindow::saveState);
    systemMenu->addAction(m_saveStateAction);
    
    m_loadStateAction = new QAction("Load State...", this);
    m_loadStateAction->setToolTip("Load state from file");
    connect(m_loadStateAction, &QAction::triggered, this, &MainWindow::loadState);
    systemMenu->addAction(m_loadStateAction);
    
    systemMenu->addSeparator();

#ifdef Q_OS_MACOS
    // On macOS, use "Preferences" to match platform conventions
    m_settingsAction = new QAction("&Preferences...", this);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
#else
    // On other platforms, use "Settings"
    m_settingsAction = new QAction("&Settings...", this);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
#endif
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    systemMenu->addAction(m_settingsAction);

    // View menu
    QMenu* viewMenu = menuBar()->addMenu("&View");

    m_fullscreenAction = new QAction("&Fullscreen", this);
#ifdef Q_OS_MACOS
    m_fullscreenAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return));
    m_fullscreenAction->setToolTip("Toggle fullscreen mode (Cmd+Enter)");
#else
    m_fullscreenAction->setShortcut(Qt::Key_F11);
    m_fullscreenAction->setToolTip("Toggle fullscreen mode (F11)");
#endif
    m_fullscreenAction->setCheckable(true);
    connect(m_fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    viewMenu->addAction(m_fullscreenAction);

    viewMenu->addSeparator();

    m_debuggerAction = new QAction("&Debugger", this);
    m_debuggerAction->setShortcut(QKeySequence(Qt::Key_F12));
    m_debuggerAction->setToolTip("Toggle debugger panel (F12)");
    m_debuggerAction->setCheckable(true);
    connect(m_debuggerAction, &QAction::triggered, this, &MainWindow::toggleDebugger);
    viewMenu->addAction(m_debuggerAction);

    // Tools menu
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    
    m_tcpServerAction = new QAction("&TCP Server", this);
    m_tcpServerAction->setToolTip("Start/stop TCP server for remote control (port configurable in settings)");
    m_tcpServerAction->setCheckable(true);
    connect(m_tcpServerAction, &QAction::triggered, this, &MainWindow::toggleTCPServer);
    toolsMenu->addAction(m_tcpServerAction);

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu("&Help");

    m_aboutAction = new QAction("&About", this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::createToolBar()
{
    m_toolBar = addToolBar("Main Toolbar");
    m_toolBar->setMovable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_toolBar->setIconSize(QSize(32, 32));

    // Set toolbar background color
    // Don't set a specific background color - let the system theme handle it
    // m_toolBar->setStyleSheet("QToolBar { background-color: rgb(220, 216, 207); }");

    // Increase toolbar height to accommodate multiple controls
    m_toolBar->setMinimumHeight(70);

    // Create joystick configuration section
    createJoystickToolbarSection();

    // Add separator between joystick and audio sections
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("margin: 2px;");
    m_toolBar->addWidget(separator);

    // Create audio configuration section
    createAudioToolbarSection();

    // Add divider
    separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    m_toolBar->addWidget(separator);

    // Create profile configuration section
    createProfileToolbarSection();

    // Add divider
    separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    m_toolBar->addWidget(separator);

    // Add spacer to push console/system buttons and logo to the right
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spacer);

    // Reset icons removed - functionality moved to console-style buttons
}

void MainWindow::createJoystickToolbarSection()
{
    // Create joystick configuration container
    QWidget* joystickContainer = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(joystickContainer);
    mainLayout->setContentsMargins(8, 0, 8, 2); // Reduced top margin from 2 to 0
    mainLayout->setSpacing(3); // Increased spacing from 2 to 3

    // Title label
    QLabel* titleLabel = new QLabel("🕹️ Joystick");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Controls container
    QWidget* controlsWidget = new QWidget();
    QHBoxLayout* controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(6);

    // Left column: enable checkboxes
    QWidget* leftColumn = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftColumn);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(2); // Increased spacing from 1 to 2

    m_joystickEnabledCheck = new QCheckBox("Enable");
    m_joystickEnabledCheck->setStyleSheet("font-size: 10px;");
    m_joystickEnabledCheck->setToolTip("Enable joystick support");
    leftLayout->addWidget(m_joystickEnabledCheck);

    m_kbdJoy0Check = new QCheckBox("Kbd J1");
    m_kbdJoy0Check->setStyleSheet("font-size: 10px;");
    m_kbdJoy0Check->setToolTip("Enable keyboard joystick 1 (Numpad)");
    leftLayout->addWidget(m_kbdJoy0Check);

    m_kbdJoy1Check = new QCheckBox("Kbd J2");
    m_kbdJoy1Check->setStyleSheet("font-size: 10px;");
    m_kbdJoy1Check->setToolTip("Enable keyboard joystick 2 (WASD)");
    leftLayout->addWidget(m_kbdJoy1Check);

    controlsLayout->addWidget(leftColumn);

    // Vertical separator
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    // Let system theme handle separator color
    // separator->setStyleSheet("color: #ccc;");
    controlsLayout->addWidget(separator);

    // Right column: compact swap widget
    m_joystickSwapWidget = new JoystickSwapWidget(this, true); // Compact mode
    controlsLayout->addWidget(m_joystickSwapWidget);

    mainLayout->addWidget(controlsWidget);

    // Connect signals for immediate updates
    connect(m_joystickEnabledCheck, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings settings;
        settings.setValue("input/joystickEnabled", checked);
        settings.sync(); // Force immediate write

        qDebug() << "=== JOYSTICK CHECKBOX TOGGLED ===";
        qDebug() << "New value:" << checked;
        qDebug() << "Saved to settings and synced";

        if (m_emulator) {
            // Apply the effective keyboard joystick state based on both checkboxes
            bool effectiveKbdJoy0 = checked && m_kbdJoy0Check->isChecked();
            bool effectiveKbdJoy1 = checked && m_kbdJoy1Check->isChecked();
            m_emulator->setKbdJoy0Enabled(effectiveKbdJoy0);
            m_emulator->setKbdJoy1Enabled(effectiveKbdJoy1);
        }

        // Enable/disable the keyboard joystick checkboxes visually (but preserve their state)
        m_kbdJoy0Check->setEnabled(checked);
        m_kbdJoy1Check->setEnabled(checked);
        m_joystickSwapWidget->setEnabled(checked);

        qDebug() << "Joystick enabled changed to:" << checked;

        // Restore focus to emulator widget (fixes Windows/Linux focus loss)
        if (m_emulatorWidget) {
            m_emulatorWidget->setFocus();
        }
    });

    connect(m_kbdJoy0Check, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings settings;
        settings.setValue("input/kbdJoy0Enabled", checked);
        if (m_emulator) {
            // Only enable if main joystick support is also enabled
            bool effectiveEnabled = checked && m_joystickEnabledCheck->isChecked();
            m_emulator->setKbdJoy0Enabled(effectiveEnabled);
        }
        qDebug() << "Keyboard Joy0 changed to:" << checked;

        // Restore focus to emulator widget (fixes Windows/Linux focus loss)
        if (m_emulatorWidget) {
            m_emulatorWidget->setFocus();
        }
    });

    connect(m_kbdJoy1Check, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings settings;
        settings.setValue("input/kbdJoy1Enabled", checked);
        if (m_emulator) {
            // Only enable if main joystick support is also enabled
            bool effectiveEnabled = checked && m_joystickEnabledCheck->isChecked();
            m_emulator->setKbdJoy1Enabled(effectiveEnabled);
        }
        qDebug() << "Keyboard Joy1 changed to:" << checked;

        // Restore focus to emulator widget (fixes Windows/Linux focus loss)
        if (m_emulatorWidget) {
            m_emulatorWidget->setFocus();
        }
    });

    connect(m_joystickSwapWidget, &JoystickSwapWidget::toggled, this, [this](bool swapped) {
        QSettings settings;
        settings.setValue("input/swapJoysticks", swapped);
        if (m_emulator) {
            m_emulator->setJoysticksSwapped(swapped);
        }
        qDebug() << "Joystick swap changed to:" << swapped;

        // Restore focus to emulator widget (fixes Windows/Linux focus loss)
        if (m_emulatorWidget) {
            m_emulatorWidget->setFocus();
        }
    });

    // Add to toolbar
    m_toolBar->addWidget(joystickContainer);

    // Load initial settings
    QSettings settings;
    bool joystickEnabled = settings.value("input/joystickEnabled", true).toBool();
    m_joystickEnabledCheck->setChecked(joystickEnabled);
    
    // Set keyboard joystick controls enabled state based on main joystick enabled state
    m_kbdJoy0Check->setEnabled(joystickEnabled);
    m_kbdJoy1Check->setEnabled(joystickEnabled);
    m_joystickSwapWidget->setEnabled(joystickEnabled);
    
    // Load keyboard joystick settings (checkboxes keep their state)
    bool kbdJoy0Checked = settings.value("input/kbdJoy0Enabled", true).toBool();
    bool kbdJoy1Checked = settings.value("input/kbdJoy1Enabled", false).toBool();
    m_kbdJoy0Check->setChecked(kbdJoy0Checked);
    m_kbdJoy1Check->setChecked(kbdJoy1Checked);
    m_joystickSwapWidget->setSwapped(settings.value("input/swapJoysticks", false).toBool());
    
    // Apply effective keyboard joystick state to emulator (only if main joystick is enabled)
    if (m_emulator) {
        m_emulator->setKbdJoy0Enabled(joystickEnabled && kbdJoy0Checked);
        m_emulator->setKbdJoy1Enabled(joystickEnabled && kbdJoy1Checked);
    }
}

void MainWindow::createAudioToolbarSection()
{
    // Create audio configuration container
    QWidget* audioContainer = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(audioContainer);
    mainLayout->setContentsMargins(8, 2, 8, 2);
    mainLayout->setSpacing(2);

    // Title label
    QLabel* titleLabel = new QLabel("🔊 Audio");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Volume control container
    QWidget* volumeWidget = new QWidget();
    QVBoxLayout* volumeLayout = new QVBoxLayout(volumeWidget);
    volumeLayout->setContentsMargins(0, 0, 0, 0);
    volumeLayout->setSpacing(2);
    volumeLayout->setAlignment(Qt::AlignCenter);

    // Volume knob
    m_volumeKnob = new VolumeKnob();
    m_volumeKnob->setRange(0, 100);
    m_volumeKnob->setValue(75);
    volumeLayout->addWidget(m_volumeKnob, 0, Qt::AlignCenter);

    // Volume percentage label
    QLabel* volumePercentLabel = new QLabel("75%");
    volumePercentLabel->setStyleSheet("font-size: 9px; min-width: 25px;");
    volumePercentLabel->setAlignment(Qt::AlignCenter);
    volumeLayout->addWidget(volumePercentLabel);

    mainLayout->addWidget(volumeWidget);

    // Connect signals for immediate updates
    connect(m_volumeKnob, &VolumeKnob::valueChanged, this, [this, volumePercentLabel](int value) {
        QSettings settings;
        settings.setValue("audio/volume", value);
        if (m_emulator) {
            m_emulator->setVolume(value / 100.0);
        }
        volumePercentLabel->setText(QString("%1%").arg(value));
        qDebug() << "Volume changed to:" << value;

        // Restore focus to emulator widget (fixes Windows/Linux focus loss)
        if (m_emulatorWidget) {
            m_emulatorWidget->setFocus();
        }
    });

    // Add to toolbar
    m_toolBar->addWidget(audioContainer);

    // Load initial settings
    QSettings settings;
    int volume = settings.value("audio/volume", 75).toInt();
    m_volumeKnob->setValue(volume);
    volumePercentLabel->setText(QString("%1%").arg(volume));

    // Audio is always enabled when there's a volume knob
    if (m_emulator) {
        m_emulator->enableAudio(true);
    }
}

void MainWindow::createProfileToolbarSection()
{
    // Create profile configuration container
    QWidget* profileContainer = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(profileContainer);
    mainLayout->setContentsMargins(8, 0, 8, 2);
    mainLayout->setSpacing(0);  // Reduce spacing between label and controls

    // Add centered label at the top
    QLabel* profileLabel = new QLabel("Profile/State");
    profileLabel->setAlignment(Qt::AlignCenter);
    profileLabel->setStyleSheet("font-weight: bold; font-size: 11px;");
    mainLayout->addWidget(profileLabel);

    // Top row: Profile dropdown and load button
    QWidget* topRow = new QWidget();
    QHBoxLayout* topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 0, 0, 2);  // Add small bottom margin
    topLayout->setSpacing(4);
    topLayout->setAlignment(Qt::AlignVCenter);  // Vertically center align

    m_profileCombo = new QComboBox();
    m_profileCombo->setStyleSheet("font-size: 10px;");
    m_profileCombo->setToolTip("Select configuration profile");
    m_profileCombo->setMinimumWidth(120);  // Wider combo box
    m_profileCombo->setFixedHeight(20);  // Fixed height for consistent alignment
    topLayout->addWidget(m_profileCombo, 0, Qt::AlignVCenter);

    // Add load button to the same row as the profile dropdown
    m_loadProfileButton = new QPushButton("LOAD");
    
    // Create adaptive button style using system palette colors for dark mode compatibility
    QPalette pal = QApplication::palette();
    QColor buttonColor = pal.color(QPalette::Button);
    QColor windowColor = pal.color(QPalette::Window);
    
    // Check if we're in dark mode
    bool isDarkMode = windowColor.lightness() < 128;
    
    // Explicitly set text color based on theme
    QColor buttonTextColor;
    if (isDarkMode) {
        buttonTextColor = QColor(255, 255, 255);  // White text for dark mode
    } else {
        buttonTextColor = QColor(0, 0, 0);  // Black text for light mode
    }
    
    // Create slightly lighter/darker versions for hover and pressed states
    QColor buttonHoverColor;
    QColor buttonPressedColor;
    QColor borderColor;
    
    if (isDarkMode) {
        // Dark mode: make hover lighter and pressed even lighter
        buttonHoverColor = buttonColor.lighter(120);
        buttonPressedColor = buttonColor.lighter(140);
        borderColor = buttonColor.lighter(150);
    } else {
        // Light mode: make hover darker and pressed even darker
        buttonHoverColor = buttonColor.darker(110);
        buttonPressedColor = buttonColor.darker(120);
        borderColor = buttonColor.darker(150);
    }
    
    QString profileButtonStyle = QString(
        "QPushButton {"
        "    font-size: 9px;"
        "    font-weight: bold;"
        "    padding: 2px 4px;"
        "    margin: 0px;"
        "    border: 1px solid %1;"
        "    background-color: %2;"
        "    color: %3;"
        "    min-width: 50px;"
        "    max-width: 80px;"  // Increased for longer text
        "    min-height: 16px;"  // Increased from 10px to prevent clipping
        "    max-height: 18px;"  // Increased from 12px
        "}"
        "QPushButton:hover {"
        "    background-color: %4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %5;"
        "    border: 1px solid %6;"
        "}").arg(borderColor.name())
            .arg(buttonColor.name())
            .arg(buttonTextColor.name())
            .arg(buttonHoverColor.name())
            .arg(buttonPressedColor.name())
            .arg(borderColor.darker(120).name());
    
    m_loadProfileButton->setStyleSheet(profileButtonStyle);
    m_loadProfileButton->setToolTip("Load selected profile");
    topLayout->addWidget(m_loadProfileButton, 0, Qt::AlignVCenter);

    mainLayout->addWidget(topRow);

    // Bottom row: State quick buttons
    QWidget* bottomRow = new QWidget();
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(2);
    
    // Add quick save/load buttons
    m_quickSaveButton = new QPushButton("STATE SAVE");
    m_quickSaveButton->setStyleSheet(profileButtonStyle);  // Use same adaptive style
    m_quickSaveButton->setToolTip("Quick Save State (Shift+F5)");
    bottomLayout->addWidget(m_quickSaveButton);
    
    m_quickLoadButton = new QPushButton("STATE LOAD");
    m_quickLoadButton->setStyleSheet(profileButtonStyle);  // Use same adaptive style
    m_quickLoadButton->setToolTip("Quick Load State (Shift+F9)");
    bottomLayout->addWidget(m_quickLoadButton);

    mainLayout->addWidget(bottomRow);

    // Initialize profile list
    refreshProfileList();

    // Connect signals
    connect(m_loadProfileButton, &QPushButton::clicked, this, &MainWindow::onLoadProfile);
    connect(m_quickSaveButton, &QPushButton::clicked, this, &MainWindow::quickSaveState);
    connect(m_quickLoadButton, &QPushButton::clicked, this, &MainWindow::quickLoadState);
    connect(m_profileManager, &ConfigurationProfileManager::profileListChanged,
            this, &MainWindow::refreshProfileList);

    m_toolBar->addWidget(profileContainer);
}

void MainWindow::createLogoSection()
{
    // Create logo label
    m_logoLabel = new QLabel();
    
    // Try to load Fujisan logo from multiple paths
    QStringList imagePaths = {
        "./images/fujisanlogo.png",                 // Correct filename in images folder
        "../images/fujisanlogo.png",
        QApplication::applicationDirPath() + "/images/fujisanlogo.png",
        QApplication::applicationDirPath() + "/../images/fujisanlogo.png",
#ifdef Q_OS_MAC
        QApplication::applicationDirPath() + "/../Resources/images/fujisanlogo.png",
#endif
#ifdef Q_OS_LINUX
        "/usr/share/fujisan/images/fujisanlogo.png",
        QApplication::applicationDirPath() + "/../share/images/fujisanlogo.png",
#endif
        "/Users/pgarcia/Downloads/fujisanlogo.png",  // Fallback to Downloads
        ":/images/fujisanlogo.png"
    };
    
    QPixmap logoPixmap;
    bool logoLoaded = false;
    
    for (const QString& path : imagePaths) {
        if (logoPixmap.load(path)) {
            logoLoaded = true;
            break;
        }
    }
    
    if (!logoLoaded) {
        // Create a simple text-based logo as fallback
        m_logoLabel->setText("FUJISAN");
        m_logoLabel->setStyleSheet("font-weight: bold; font-size: 12px; margin: 0 8px;");
        m_logoLabel->setToolTip("Fujisan - Modern Atari Emulator");
    } else {
        // Store original pixmap and update based on theme
        m_logoLabel->setProperty("originalPixmap", logoPixmap);
        updateToolbarLogo();
        m_logoLabel->setToolTip("Fujisan - Modern Atari Emulator");
        m_logoLabel->setContentsMargins(6, 2, 10, 2);
    }
    
    m_toolBar->addWidget(m_logoLabel);
}

void MainWindow::createEmulatorWidget()
{
    // Create a container widget for the emulator
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0); // Remove margins
    layout->setSpacing(0); // Remove spacing

    m_emulatorWidget = new EmulatorWidget(centralWidget);
    m_emulatorWidget->setEmulator(m_emulator);

    // Make the emulator widget expand to fill all available space
    layout->addWidget(m_emulatorWidget);

    setCentralWidget(centralWidget);

    // Give the emulator widget focus by default
    m_emulatorWidget->setFocus();
}

void MainWindow::createDebugger()
{
    // Create debugger widget
    m_debuggerWidget = new DebuggerWidget(m_emulator, this);
    
    // Connect debugger to TCP server for remote debugging
    m_tcpServer->setDebuggerWidget(m_debuggerWidget);

    // Create dock widget for debugger
    m_debuggerDock = new QDockWidget("Debugger", this);
    m_debuggerDock->setWidget(m_debuggerWidget);
    m_debuggerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_debuggerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);

    // Add dock widget to right side by default
    addDockWidget(Qt::RightDockWidgetArea, m_debuggerDock);

    // Hide by default
    m_debuggerDock->hide();

    // Connect dock visibility to menu action
    connect(m_debuggerDock, &QDockWidget::visibilityChanged, [this](bool visible) {
        m_debuggerAction->setChecked(visible);
    });
}

void MainWindow::loadRom()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load Atari File",
        QString(),
        "Atari Files (*.rom *.bin *.car *.atr *.xex *.exe *.com);;Cartridge ROMs (*.rom *.bin *.car);;Disk Images (*.atr);;Executables (*.xex *.exe *.com);;All Files (*)"
    );

    if (!fileName.isEmpty()) {
        if (m_emulator->loadFile(fileName)) {
            qDebug() << "Successfully loaded:" << fileName;
            statusBar()->showMessage("Loaded: " + fileName, 3000);
        } else {
            QMessageBox::warning(this, "Error", "Failed to load ROM file: " + fileName);
        }
    }
}


void MainWindow::coldBoot()
{
    m_emulator->coldBoot();
    statusBar()->showMessage("Cold boot performed", 2000);
}

void MainWindow::warmBoot()
{
    m_emulator->warmBoot();
    statusBar()->showMessage("Warm boot performed", 2000);
}

void MainWindow::toggleBasic(bool enabled)
{
    m_emulator->setBasicEnabled(enabled);
    QString message = enabled ? "BASIC enabled - restarting..." : "BASIC disabled - restarting...";
    statusBar()->showMessage(message, 3000);

    // Also update the menu checkbox to stay in sync
    m_basicAction->setChecked(enabled);

    restartEmulator();

    // Restore focus to emulator widget (fixes Windows/Linux focus loss)
    if (m_emulatorWidget) {
        m_emulatorWidget->setFocus();
    }
}

void MainWindow::toggleAltirraOS(bool enabled)
{
    m_emulator->setAltirraOSEnabled(enabled);
    QString message = enabled ? "Altirra OS enabled - restarting..." : "Original Atari OS enabled - restarting...";
    statusBar()->showMessage(message, 3000);
    restartEmulator();
}

void MainWindow::toggleAltirraBASIC(bool enabled)
{
    m_emulator->setAltirraBASICEnabled(enabled);
    QString message = enabled ? "Altirra BASIC enabled - restarting..." : "Original Atari BASIC enabled - restarting...";
    statusBar()->showMessage(message, 3000);
    restartEmulator();
}

void MainWindow::onMachineTypeChanged(int index)
{
    QString machineType;
    QString message;

    switch (index) {
        case 0: // Atari 400/800
            machineType = "-atari";
            message = "Machine set to Atari 400/800 - restarting...";
            break;
        case 1: // Atari 1200XL
            machineType = "-1200";
            message = "Machine set to Atari 1200XL - restarting...";
            break;
        case 2: // Atari 800XL (default)
            machineType = "-xl";
            message = "Machine set to Atari 800XL - restarting...";
            break;
        case 3: // Atari 130XE
            machineType = "-xe";
            message = "Machine set to Atari 130XE - restarting...";
            break;
        case 4: // Atari 320XE (Compy-Shop)
            machineType = "-320xe";
            message = "Machine set to Atari 320XE (Compy-Shop) - restarting...";
            break;
        case 5: // Atari 320XE (Rambo XL)
            machineType = "-rambo";
            message = "Machine set to Atari 320XE (Rambo XL) - restarting...";
            break;
        case 6: // Atari 576XE
            machineType = "-576xe";
            message = "Machine set to Atari 576XE - restarting...";
            break;
        case 7: // Atari 1088XE
            machineType = "-1088xe";
            message = "Machine set to Atari 1088XE - restarting...";
            break;
        case 8: // Atari XEGS
            machineType = "-xegs";
            message = "Machine set to Atari XEGS - restarting...";
            break;
        case 9: // Atari 5200
            machineType = "-5200";
            message = "Machine set to Atari 5200 - restarting...";
            break;
        default:
            return;
    }

    m_emulator->setMachineType(machineType);
    statusBar()->showMessage(message, 3000);
    restartEmulator();
}

void MainWindow::restartEmulator()
{
    m_emulator->shutdown();

    // Load display and artifact settings from preferences
    QSettings settings("8bitrelics", "Fujisan");
    QString artifactMode = settings.value("video/artifacting", "none").toString();

    // Load display settings - use "full" by default to avoid cropping
    QString horizontalArea = settings.value("video/horizontalArea", "full").toString();
    QString verticalArea = settings.value("video/verticalArea", "full").toString();
    int horizontalShift = settings.value("video/horizontalShift", 0).toInt();
    int verticalShift = settings.value("video/verticalShift", 0).toInt();
    QString fitScreen = settings.value("video/fitScreen", "both").toString();
    bool show80Column = settings.value("video/show80Column", false).toBool();
    bool vSyncEnabled = settings.value("video/vSyncEnabled", false).toBool();

    qDebug() << "Display settings - HArea:" << horizontalArea << "VArea:" << verticalArea
             << "HShift:" << horizontalShift << "VShift:" << verticalShift
             << "Fit:" << fitScreen << "80Col:" << show80Column << "VSync:" << vSyncEnabled;

    // Load input settings for keyboard joystick emulation
    bool joystickEnabled = settings.value("input/joystickEnabled", true).toBool();  // Main joystick support
    bool kbdJoy0Saved = settings.value("input/kbdJoy0Enabled", true).toBool();     // Saved kbd joy0 state
    bool kbdJoy1Saved = settings.value("input/kbdJoy1Enabled", false).toBool();    // Saved kbd joy1 state
    bool swapJoysticks = settings.value("input/swapJoysticks", false).toBool();     // Default false: Joy0=Numpad, Joy1=WASD
    
    qDebug() << "=== LOADED JOYSTICK SETTINGS FROM QSETTINGS ===";
    qDebug() << "Main joystick enabled from settings:" << joystickEnabled;
    qDebug() << "Kbd Joy0 saved state:" << kbdJoy0Saved;
    qDebug() << "Kbd Joy1 saved state:" << kbdJoy1Saved;
    
    // Only enable keyboard joysticks if main joystick support is enabled
    bool kbdJoy0Enabled = joystickEnabled && kbdJoy0Saved;
    bool kbdJoy1Enabled = joystickEnabled && kbdJoy1Saved;

    // Load special device settings
    bool netSIOEnabled = settings.value("media/netSIOEnabled", false).toBool();
    bool rtimeEnabled = settings.value("media/rtimeEnabled", false).toBool();

    qDebug() << "Applying input settings - KbdJoy0:" << kbdJoy0Enabled << "KbdJoy1:" << kbdJoy1Enabled << "Swap:" << swapJoysticks;
    qDebug() << "Special devices - NetSIO:" << netSIOEnabled << "RTime:" << rtimeEnabled;

    // Load ROM paths BEFORE initialization
    QString machineType = m_emulator->getMachineType();
    QString osRomKey = QString("machine/osRom_%1").arg(machineType.mid(1)); // Remove the '-' prefix
    QString osRomPath = settings.value(osRomKey, "").toString();
    QString basicRomPath = settings.value("machine/basicRom", "").toString();
    
    // Set ROM paths in the emulator before initialization
    m_emulator->setOSRomPath(osRomPath);
    m_emulator->setBasicRomPath(basicRomPath);
    
    qDebug() << "ROM paths for restart - OS:" << osRomPath << "BASIC:" << basicRomPath;
    qDebug() << "Altirra OS enabled:" << m_emulator->isAltirraOSEnabled();

    if (m_emulator->initializeWithNetSIOConfig(m_emulator->isBasicEnabled(),
                                             m_emulator->getMachineType(),
                                             m_emulator->getVideoSystem(),
                                             artifactMode,
                                             horizontalArea, verticalArea,
                                             horizontalShift, verticalShift,
                                             fitScreen, show80Column, vSyncEnabled,
                                             kbdJoy0Enabled, kbdJoy1Enabled, swapJoysticks,
                                             netSIOEnabled, rtimeEnabled)) {
        QString message = QString("Emulator restarted: %1 %2 with BASIC %3")
                         .arg(m_emulator->getMachineType())
                         .arg(m_emulator->getVideoSystem())
                         .arg(m_emulator->isBasicEnabled() ? "enabled" : "disabled");
        statusBar()->showMessage(message, 3000);
        qDebug() << message;
        
        // Ensure keyboard joystick state is properly applied after restart
        m_emulator->setKbdJoy0Enabled(kbdJoy0Enabled);
        m_emulator->setKbdJoy1Enabled(kbdJoy1Enabled);
        qDebug() << "Applied keyboard joystick state after restart - Joy0:" << kbdJoy0Enabled << "Joy1:" << kbdJoy1Enabled;
        
        // Update toolbar to reflect actual BASIC state (may have been auto-disabled for FujiNet)
        updateToolbarFromSettings();
    } else {
        QMessageBox::critical(this, "Error", "Failed to restart emulator");
    }
}


void MainWindow::onVideoSystemToggled(bool isPAL)
{
    if (isPAL) {
        // PAL mode (toggle ON)
        m_emulator->setVideoSystem("-pal");
        statusBar()->showMessage("Video system set to PAL (49.86 fps) - restarting...", 3000);
    } else {
        // NTSC mode (toggle OFF)
        m_emulator->setVideoSystem("-ntsc");
        statusBar()->showMessage("Video system set to NTSC (59.92 fps) - restarting...", 3000);
    }
    restartEmulator();

    // Restore focus to emulator widget (fixes Windows/Linux focus loss)
    if (m_emulatorWidget) {
        m_emulatorWidget->setFocus();
    }
}

void MainWindow::onSpeedToggled(bool isFullSpeed)
{
    if (isFullSpeed) {
        // Full speed mode (toggle ON) - unlimited host speed
        m_emulator->setEmulationSpeed(0); // 0 = unlimited/host speed
        statusBar()->showMessage("Emulation speed set to Full (unlimited host speed)", 2000);
    } else {
        // Real speed mode (toggle OFF) - authentic Atari timing
        m_emulator->setEmulationSpeed(100);
        statusBar()->showMessage("Emulation speed set to Real (authentic Atari speed)", 2000);
    }

    // Restore focus to emulator widget (fixes Windows/Linux focus loss)
    if (m_emulatorWidget) {
        m_emulatorWidget->setFocus();
    }
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(m_emulator, this);
    connect(&dialog, &SettingsDialog::settingsChanged, this, &MainWindow::onSettingsChanged);
    
    // Connect signal to sync printer state when profile is being saved
    connect(&dialog, &SettingsDialog::syncPrinterStateRequested, this, [this, &dialog]() {
        if (m_mediaPeripheralsDock && m_mediaPeripheralsDock->getPrinterWidget()) {
            auto* printerWidget = m_mediaPeripheralsDock->getPrinterWidget();
            printerWidget->saveSettings();
            dialog.loadSettings();
            qDebug() << "Synced PrinterWidget state for profile saving";
        }
    });
    
    // Sync current PrinterWidget state to Settings Dialog before showing
    if (m_mediaPeripheralsDock && m_mediaPeripheralsDock->getPrinterWidget()) {
        auto* printerWidget = m_mediaPeripheralsDock->getPrinterWidget();
        
        // Make sure PrinterWidget saves its current state to QSettings
        printerWidget->saveSettings();
        
        // Force the dialog to reload settings to get the latest values
        dialog.loadSettings();
        
        qDebug() << "Synced PrinterWidget state to Settings Dialog";
    }
    
    dialog.exec();
}

void MainWindow::onSettingsChanged()
{
    qDebug() << "Settings changed - updating toolbar and video settings";
    updateToolbarFromSettings();
    loadVideoSettings();

    // Reload media settings to sync disk widgets with any changes made in settings dialog
    loadAndApplyMediaSettings();
    
    // Sync printer settings from QSettings to PrinterWidget
    if (m_mediaPeripheralsDock && m_mediaPeripheralsDock->getPrinterWidget()) {
        QSettings settings;
        bool printerEnabled = settings.value("printer/enabled", false).toBool();
        QString outputFormat = settings.value("printer/outputFormat", "Text").toString();
        QString printerType = settings.value("printer/type", "Generic").toString();
        
        qDebug() << "Syncing printer settings to widget - Enabled:" << printerEnabled 
                 << "Format:" << outputFormat << "Type:" << printerType;
        
        auto* printerWidget = m_mediaPeripheralsDock->getPrinterWidget();
        printerWidget->setPrinterEnabled(printerEnabled);
        printerWidget->setOutputFormat(outputFormat);
        printerWidget->setPrinterType(printerType);
    }

    statusBar()->showMessage("Settings applied and emulator restarted", 3000);
}

void MainWindow::updateToolbarFromSettings()
{
    // Update machine combo
    QString machineType = m_emulator->getMachineType();
    int machineIndex = 2; // Default to 800XL (updated index)
    if (machineType == "-atari") machineIndex = 0;
    else if (machineType == "-1200") machineIndex = 1;
    else if (machineType == "-xl") machineIndex = 2;
    else if (machineType == "-xe") machineIndex = 3;
    else if (machineType == "-320xe") machineIndex = 4;
    else if (machineType == "-rambo") machineIndex = 5;
    else if (machineType == "-576xe") machineIndex = 6;
    else if (machineType == "-1088xe") machineIndex = 7;
    else if (machineType == "-xegs") machineIndex = 8;
    else if (machineType == "-5200") machineIndex = 9;

    m_machineCombo->blockSignals(true);
    m_machineCombo->setCurrentIndex(machineIndex);
    m_machineCombo->blockSignals(false);

    // Update BASIC toggle
    m_basicToggle->blockSignals(true);
    m_basicToggle->setChecked(m_emulator->isBasicEnabled());
    m_basicToggle->blockSignals(false);

    // Update video toggle (PAL = ON, NTSC = OFF)
    bool isPAL = (m_emulator->getVideoSystem() == "-pal");
    m_videoToggle->blockSignals(true);
    m_videoToggle->setChecked(isPAL);
    m_videoToggle->blockSignals(false);
    

    // Update speed toggle from settings
    QSettings speedSettings("8bitrelics", "Fujisan");
    bool turboMode = speedSettings.value("machine/turboMode", false).toBool();
    m_speedToggle->blockSignals(true);
    m_speedToggle->setChecked(turboMode);
    m_speedToggle->blockSignals(false);

    // Update menu actions
    m_basicAction->blockSignals(true);
    m_basicAction->setChecked(m_emulator->isBasicEnabled());
    m_basicAction->blockSignals(false);

    m_altirraOSAction->blockSignals(true);
    m_altirraOSAction->setChecked(m_emulator->isAltirraOSEnabled());
    m_altirraOSAction->blockSignals(false);

    m_altirraBASICAction->blockSignals(true);
    m_altirraBASICAction->setChecked(m_emulator->isAltirraBASICEnabled());
    m_altirraBASICAction->blockSignals(false);

    // Video system state is now managed by the toolbar toggle widget only

    // Update audio settings from emulator state
    QSettings settings;
    int volume = settings.value("audio/volume", 75).toInt();

    m_volumeKnob->blockSignals(true);
    m_volumeKnob->setValue(volume);
    m_volumeKnob->blockSignals(false);

    // Audio is always enabled when there's a volume knob
    if (m_emulator) {
        m_emulator->enableAudio(true);
    }
    
    // Update joystick checkbox from settings
    bool joystickEnabled = settings.value("input/joystickEnabled", true).toBool();
    m_joystickEnabledCheck->blockSignals(true);
    m_joystickEnabledCheck->setChecked(joystickEnabled);
    m_joystickEnabledCheck->blockSignals(false);
    
    // Enable/disable keyboard joystick checkboxes based on main joystick state
    m_kbdJoy0Check->setEnabled(joystickEnabled);
    m_kbdJoy1Check->setEnabled(joystickEnabled);
    m_joystickSwapWidget->setEnabled(joystickEnabled);

    // Update joystick settings from saved state
    if (m_emulator && m_joystickEnabledCheck && m_kbdJoy0Check && m_kbdJoy1Check && m_joystickSwapWidget) {
        bool joystickEnabled = settings.value("input/joystickEnabled", true).toBool();
        bool kbdJoy0Saved = settings.value("input/kbdJoy0Enabled", true).toBool();
        bool kbdJoy1Saved = settings.value("input/kbdJoy1Enabled", false).toBool();
        bool swapJoysticks = settings.value("input/swapJoysticks", false).toBool();
        
        m_joystickEnabledCheck->blockSignals(true);
        m_joystickEnabledCheck->setChecked(joystickEnabled);
        m_joystickEnabledCheck->blockSignals(false);

        m_kbdJoy0Check->blockSignals(true);
        m_kbdJoy0Check->setChecked(kbdJoy0Saved);
        m_kbdJoy0Check->blockSignals(false);

        m_kbdJoy1Check->blockSignals(true);
        m_kbdJoy1Check->setChecked(kbdJoy1Saved);
        m_kbdJoy1Check->blockSignals(false);

        m_joystickSwapWidget->setSwapped(swapJoysticks);
        
        // Enable/disable keyboard joystick checkboxes based on main joystick state
        m_kbdJoy0Check->setEnabled(joystickEnabled);
        m_kbdJoy1Check->setEnabled(joystickEnabled);
        m_joystickSwapWidget->setEnabled(joystickEnabled);
        
        // Apply the effective joystick state to the emulator
        // Only enable keyboard joysticks if main joystick support is enabled
        m_emulator->setKbdJoy0Enabled(joystickEnabled && kbdJoy0Saved);
        m_emulator->setKbdJoy1Enabled(joystickEnabled && kbdJoy1Saved);
        m_emulator->setJoysticksSwapped(swapJoysticks);
        
        qDebug() << "Applied effective joystick state - MainJoystick:" << joystickEnabled
                 << "Joy0:" << (joystickEnabled && kbdJoy0Saved)
                 << "Joy1:" << (joystickEnabled && kbdJoy1Saved)
                 << "Swap:" << swapJoysticks;
    }

    qDebug() << "Toolbar updated - Machine:" << machineType << "BASIC:" << m_emulator->isBasicEnabled()
             << "Video:" << m_emulator->getVideoSystem() << "Volume:" << volume
             << "Joy0:" << (m_emulator ? m_emulator->isKbdJoy0Enabled() : false)
             << "Joy1:" << (m_emulator ? m_emulator->isKbdJoy1Enabled() : false) 
             << "Swapped:" << (m_emulator ? m_emulator->isJoysticksSwapped() : false);
}

void MainWindow::updateToolbarLogo()
{
    if (!m_logoLabel) return;
    
    // Check if we have a logo pixmap stored
    QVariant pixmapVariant = m_logoLabel->property("originalPixmap");
    if (pixmapVariant.isNull()) return;
    
    QPixmap originalPixmap = pixmapVariant.value<QPixmap>();
    if (originalPixmap.isNull()) return;
    
    // Scale logo to appropriate toolbar size
    QPixmap scaledLogo = originalPixmap.scaled(QSize(80, 40), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Check if we're in dark mode
    QPalette palette = QApplication::palette();
    QColor windowColor = palette.color(QPalette::Window);
    bool isDarkMode = windowColor.lightness() < 128;
    
    if (isDarkMode) {
        // Invert the logo for dark mode
        QImage img = scaledLogo.toImage();
        img.invertPixels();
        scaledLogo = QPixmap::fromImage(img);
    }
    
    m_logoLabel->setPixmap(scaledLogo);
}

void MainWindow::updateToolbarButtonStyles()
{
    // Get current palette colors
    QPalette pal = QApplication::palette();
    QColor buttonColor = pal.color(QPalette::Button);
    QColor windowColor = pal.color(QPalette::Window);
    
    // Check if we're in dark mode
    bool isDarkMode = windowColor.lightness() < 128;
    
    // Explicitly set text color based on theme
    QColor buttonTextColor;
    if (isDarkMode) {
        buttonTextColor = QColor(255, 255, 255);  // White text for dark mode
    } else {
        buttonTextColor = QColor(0, 0, 0);  // Black text for light mode
    }
    
    // Create slightly lighter/darker versions for hover and pressed states
    QColor buttonHoverColor;
    QColor buttonPressedColor;
    QColor borderColor;
    
    if (isDarkMode) {
        // Dark mode: make hover lighter and pressed even lighter
        buttonHoverColor = buttonColor.lighter(120);
        buttonPressedColor = buttonColor.lighter(140);
        borderColor = buttonColor.lighter(150);
    } else {
        // Light mode: make hover darker and pressed even darker
        buttonHoverColor = buttonColor.darker(110);
        buttonPressedColor = buttonColor.darker(120);
        borderColor = buttonColor.darker(150);
    }
    
    // Create button style for console and system buttons
    QString buttonStyle = QString(
        "QPushButton {"
        "    font-size: 9px;"
        "    font-weight: bold;"
        "    padding: 2px 4px;"
        "    margin: 0px;"
        "    border: 1px solid %1;"
        "    background-color: %2;"
        "    color: %3;"
        "    min-width: 50px;"
        "    max-width: 60px;"
        "    min-height: 14px;"
        "    max-height: 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: %4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %5;"
        "    border: 1px solid %6;"
        "}").arg(borderColor.name())
            .arg(buttonColor.name())
            .arg(buttonTextColor.name())
            .arg(buttonHoverColor.name())
            .arg(buttonPressedColor.name())
            .arg(borderColor.darker(120).name());
    
    // Apply style to console buttons if they exist
    if (m_startButton) m_startButton->setStyleSheet(buttonStyle);
    if (m_selectButton) m_selectButton->setStyleSheet(buttonStyle);
    if (m_optionButton) m_optionButton->setStyleSheet(buttonStyle);
    if (m_breakButton) m_breakButton->setStyleSheet(buttonStyle);
    if (m_pauseButton) m_pauseButton->setStyleSheet(buttonStyle);
    
    // Apply to other buttons that may exist in the dock
    QList<QPushButton*> allButtons = findChildren<QPushButton*>();
    for (QPushButton* button : allButtons) {
        QString text = button->text();
        if (text == "COLD" || text == "WARM" || text == "INVERSE") {
            button->setStyleSheet(buttonStyle);
        }
    }
    
    // Create profile button style (slightly wider for longer text)
    QString profileButtonStyle = QString(
        "QPushButton {"
        "    font-size: 9px;"
        "    font-weight: bold;"
        "    padding: 2px 4px;"
        "    margin: 0px;"
        "    border: 1px solid %1;"
        "    background-color: %2;"
        "    color: %3;"
        "    min-width: 50px;"
        "    max-width: 80px;"
        "    min-height: 16px;"
        "    max-height: 18px;"
        "}"
        "QPushButton:hover {"
        "    background-color: %4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %5;"
        "    border: 1px solid %6;"
        "}").arg(borderColor.name())
            .arg(buttonColor.name())
            .arg(buttonTextColor.name())
            .arg(buttonHoverColor.name())
            .arg(buttonPressedColor.name())
            .arg(borderColor.darker(120).name());
    
    // Apply style to profile buttons if they exist
    if (m_loadProfileButton) m_loadProfileButton->setStyleSheet(profileButtonStyle);
    if (m_quickSaveButton) m_quickSaveButton->setStyleSheet(profileButtonStyle);
    if (m_quickLoadButton) m_quickLoadButton->setStyleSheet(profileButtonStyle);
}

void MainWindow::toggleFullscreen()
{
    if (m_isInCustomFullscreen) {
        // Exit fullscreen mode
        exitCustomFullscreen();
    } else {
        // Enter fullscreen mode
        enterCustomFullscreen();
    }
}

void MainWindow::enterCustomFullscreen()
{
    if (m_isInCustomFullscreen || !m_emulatorWidget) return;

    // Create fullscreen widget
    m_fullscreenWidget = new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint);
    m_fullscreenWidget->setWindowTitle("Fujisan - Fullscreen");
    m_fullscreenWidget->setAttribute(Qt::WA_DeleteOnClose, false);
    m_fullscreenWidget->setStyleSheet("background-color: black;");

    // Create layout for the fullscreen widget
    QVBoxLayout* layout = new QVBoxLayout(m_fullscreenWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create a new EmulatorWidget for fullscreen (don't move the original)
    EmulatorWidget* fullscreenEmulator = new EmulatorWidget(m_fullscreenWidget);
    fullscreenEmulator->setEmulator(m_emulator);
    layout->addWidget(fullscreenEmulator);

    // Show fullscreen
    m_fullscreenWidget->showFullScreen();
    fullscreenEmulator->setFocus();

    // Install event filter to handle key presses in fullscreen
    m_fullscreenWidget->installEventFilter(this);

    // Update state
    m_isInCustomFullscreen = true;
    m_fullscreenAction->setChecked(true);

#ifdef Q_OS_MACOS
    statusBar()->showMessage("Fullscreen mode enabled - Press Cmd+Enter to exit", 3000);
#else
    statusBar()->showMessage("Fullscreen mode enabled - Press F11 to exit", 3000);
#endif
}

void MainWindow::exitCustomFullscreen()
{
    if (!m_isInCustomFullscreen || !m_fullscreenWidget) return;

    // Remove event filter
    m_fullscreenWidget->removeEventFilter(this);

    // Close and delete the fullscreen widget (this also deletes the fullscreen emulator widget)
    m_fullscreenWidget->close();
    delete m_fullscreenWidget;
    m_fullscreenWidget = nullptr;

    // Update state
    m_isInCustomFullscreen = false;
    m_fullscreenAction->setChecked(false);

    // Give focus back to the main emulator widget
    m_emulatorWidget->setFocus();

    statusBar()->showMessage("Fullscreen mode disabled", 2000);
}

void MainWindow::showAbout()
{
#ifdef Q_OS_MACOS
    QString shortcut = "Cmd+Enter";
#else
    QString shortcut = "F11";
#endif

    // Create custom about dialog
    QDialog aboutDialog(this);
    aboutDialog.setWindowTitle("About Fujisan");
    aboutDialog.setFixedSize(500, 400);
    aboutDialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&aboutDialog);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);

    // Logo
    QLabel* logoLabel = new QLabel();
    
    // Try to load Fujisan logo from multiple paths (same as toolbar)
    QStringList imagePaths = {
        "./images/fujisanlogo.png",
        "../images/fujisanlogo.png",
        QApplication::applicationDirPath() + "/images/fujisanlogo.png",
        QApplication::applicationDirPath() + "/../images/fujisanlogo.png",
#ifdef Q_OS_MAC
        QApplication::applicationDirPath() + "/../Resources/images/fujisanlogo.png",
#endif
#ifdef Q_OS_LINUX
        "/usr/share/fujisan/images/fujisanlogo.png",
        QApplication::applicationDirPath() + "/../share/images/fujisanlogo.png",
#endif
        ":/images/fujisanlogo.png"
    };
    
    QPixmap logo;
    bool logoLoaded = false;
    
    for (const QString& path : imagePaths) {
        if (logo.load(path)) {
            logoLoaded = true;
            break;
        }
    }
    
    if (logoLoaded) {
        // Scale logo to fit nicely in the dialog
        QPixmap scaledLogo = logo.scaled(300, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // Check if we're in dark mode and invert the logo if needed
        QPalette palette = QApplication::palette();
        QColor windowColor = palette.color(QPalette::Window);
        bool isDarkMode = windowColor.lightness() < 128;
        
        if (isDarkMode) {
            // Invert the logo for dark mode
            QImage img = scaledLogo.toImage();
            img.invertPixels();
            scaledLogo = QPixmap::fromImage(img);
        }
        
        logoLabel->setPixmap(scaledLogo);
        logoLabel->setAlignment(Qt::AlignCenter);
    } else {
        logoLabel->setText("FUJISAN");
        logoLabel->setAlignment(Qt::AlignCenter);
        QFont logoFont = logoLabel->font();
        logoFont.setPointSize(24);
        logoFont.setBold(true);
        logoLabel->setFont(logoFont);
    }
    layout->addWidget(logoLabel);

    // Title and version
    QLabel* titleLabel = new QLabel(FUJISAN_FULL_VERSION_STRING);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    // Description
    QLabel* descriptionLabel = new QLabel("A modern frontend for the Atari800 emulator");
    descriptionLabel->setAlignment(Qt::AlignCenter);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    // Credits and copyright
    QLabel* creditsLabel = new QLabel(
        "Built on the Atari800 emulator project\n"
        "Copyright © 2025 Paulo Garcia (8bitrelics.com)\n"
        "Licensed under the MIT License");
    creditsLabel->setAlignment(Qt::AlignCenter);
    QFont creditsFont = creditsLabel->font();
    creditsFont.setPointSize(10);
    creditsLabel->setFont(creditsFont);
    creditsLabel->setStyleSheet("color: #666666;");
    layout->addWidget(creditsLabel);

    // OK button
    QPushButton* okButton = new QPushButton("OK");
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, &aboutDialog, &QDialog::accept);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    aboutDialog.exec();
}

void MainWindow::createMediaPeripheralsDock()
{
    // Create D1 drive for toolbar first (so we can get its width)
    m_diskDrive1 = new DiskDriveWidget(1, m_emulator, this);

    // Create cartridge widget for toolbar
    m_cartridgeWidget = new CartridgeWidget(m_emulator, this);

    // Create Media & Peripherals dock widget
    m_mediaPeripheralsDock = new MediaPeripheralsDock(m_emulator, this);

    m_mediaPeripheralsDockWidget = new QDockWidget("", this);
    m_mediaPeripheralsDockWidget->setWidget(m_mediaPeripheralsDock);
    m_mediaPeripheralsDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_mediaPeripheralsDockWidget->setTitleBarWidget(new QWidget()); // Hide title bar completely

    // Media dock background color removed - using default styling

    // Set dock width back to original size
    // Original: 94px + 30px padding = 124px, reduced by 10px
    int dockWidth = 114;
    m_mediaPeripheralsDockWidget->setMinimumWidth(dockWidth);
    m_mediaPeripheralsDockWidget->setMaximumWidth(dockWidth);
    m_mediaPeripheralsDockWidget->setFeatures(QDockWidget::DockWidgetMovable |
                                              QDockWidget::DockWidgetFloatable |
                                              QDockWidget::DockWidgetClosable);

    // Add dock to left side
    addDockWidget(Qt::LeftDockWidgetArea, m_mediaPeripheralsDockWidget);

    // Initially hidden
    m_mediaPeripheralsDockWidget->hide();

    // Create media dock toggle button
    int buttonWidth = m_diskDrive1->width(); // Matches D1 width (now 20% smaller)
    m_mediaToggleButton = new QPushButton("≡", this);
    m_mediaToggleButton->setToolTip("Show/Hide Media & Peripherals dock");
    m_mediaToggleButton->setFixedSize(buttonWidth, 12);
    m_mediaToggleButton->setCheckable(true);
    m_mediaToggleButton->setStyleSheet(
        "QPushButton {"
        "    margin: 0px;"
        "    padding: 0px;"
        "}"
    );

    // Connect signals
    connect(m_diskDrive1, &DiskDriveWidget::diskInserted, this, &MainWindow::onDiskInserted);
    connect(m_diskDrive1, &DiskDriveWidget::diskEjected, this, &MainWindow::onDiskEjected);
    connect(m_diskDrive1, &DiskDriveWidget::driveStateChanged, this, &MainWindow::onDriveStateChanged);

    // Connect cartridge widget signals
    connect(m_cartridgeWidget, &CartridgeWidget::cartridgeInserted, this, &MainWindow::onCartridgeInserted);
    connect(m_cartridgeWidget, &CartridgeWidget::cartridgeEjected, this, &MainWindow::onCartridgeEjected);

    // Connect emulator widget signals
    connect(m_emulatorWidget, &EmulatorWidget::diskDroppedOnEmulator, this, &MainWindow::onDiskDroppedOnEmulator);

    // Connect media dock signals
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::diskInserted, this, &MainWindow::onDiskInserted);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::diskEjected, this, &MainWindow::onDiskEjected);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::driveStateChanged, this, &MainWindow::onDriveStateChanged);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::cassetteInserted, this, &MainWindow::onCassetteInserted);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::cassetteEjected, this, &MainWindow::onCassetteEjected);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::cassetteStateChanged, this, &MainWindow::onCassetteStateChanged);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::printerEnabledChanged, this, &MainWindow::onPrinterEnabledChanged);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::printerOutputFormatChanged, this, &MainWindow::onPrinterOutputFormatChanged);
    connect(m_mediaPeripheralsDock, &MediaPeripheralsDock::printerTypeChanged, this, &MainWindow::onPrinterTypeChanged);
    // Note: Cartridge signals connected to toolbar cartridge widget above
    
    // Connect TCP server signals for GUI integration
    connect(m_tcpServer, &TCPServer::diskInserted, this, &MainWindow::onDiskInserted);
    connect(m_tcpServer, &TCPServer::diskEjected, this, &MainWindow::onDiskEjected);

    // Connect solid LED disk I/O monitoring
    connect(m_emulator, &AtariEmulator::diskIOStart, this, [this](int driveNumber, bool isWriting) {
#ifdef DEBUG_DISK_IO
        qDebug() << "*** MainWindow received diskIOStart signal for D" << driveNumber << ":" << (isWriting ? "WRITE" : "READ") << "***";
#endif
        // Turn LED ON on D1 (toolbar drive)
        if (driveNumber == 1 && m_diskDrive1) {
#ifdef DEBUG_DISK_IO
            qDebug() << "*** Calling turnOn" << (isWriting ? "Write" : "Read") << "LED() for D1 ***";
#endif
            if (isWriting) {
                m_diskDrive1->turnOnWriteLED();
            } else {
                m_diskDrive1->turnOnReadLED();
            }
        }
        // Turn LED ON on D2-D8 (dock drives)
        else if (driveNumber >= 2 && driveNumber <= 8) {
#ifdef DEBUG_DISK_IO
            qDebug() << "*** Looking for dock drive widget D" << driveNumber << "***";
#endif
            DiskDriveWidget* driveWidget = m_mediaPeripheralsDock->getDriveWidget(driveNumber);
            if (driveWidget) {
#ifdef DEBUG_DISK_IO
                qDebug() << "*** Calling turnOn" << (isWriting ? "Write" : "Read") << "LED() for dock drive D" << driveNumber << "***";
#endif
                if (isWriting) {
                    driveWidget->turnOnWriteLED();
                } else {
                    driveWidget->turnOnReadLED();
                }
            } else {
                qDebug() << "*** ERROR: Could not find dock drive widget for D" << driveNumber << "***";
            }
        }
    });

    connect(m_emulator, &AtariEmulator::diskIOEnd, this, [this](int driveNumber) {
#ifdef DEBUG_DISK_IO
        qDebug() << "*** MainWindow received diskIOEnd signal for D" << driveNumber << ":" << "***";
#endif
        // Turn LED OFF on D1 (toolbar drive)
        if (driveNumber == 1 && m_diskDrive1) {
#ifdef DEBUG_DISK_IO
            qDebug() << "*** Calling turnOffActivityLED() for D1 ***";
#endif
            m_diskDrive1->turnOffActivityLED();
        }
        // Turn LED OFF on D2-D8 (dock drives)
        else if (driveNumber >= 2 && driveNumber <= 8) {
#ifdef DEBUG_DISK_IO
            qDebug() << "*** Looking for dock drive widget D" << driveNumber << "for LED OFF ***";
#endif
            DiskDriveWidget* driveWidget = m_mediaPeripheralsDock->getDriveWidget(driveNumber);
            if (driveWidget) {
#ifdef DEBUG_DISK_IO
                qDebug() << "*** Calling turnOffActivityLED() for dock drive D" << driveNumber << "***";
#endif
                driveWidget->turnOffActivityLED();
            } else {
#ifdef DEBUG_DISK_IO
                qDebug() << "*** ERROR: Could not find dock drive widget for D" << driveNumber << "for LED OFF ***";
#endif
            }
        }
    });

    connect(m_mediaToggleButton, &QPushButton::clicked, this, &MainWindow::toggleMediaDock);

    // Create separate D1 container
    QWidget* d1Container = new QWidget(this);
    QVBoxLayout* d1MainLayout = new QVBoxLayout(d1Container);
    d1MainLayout->setContentsMargins(0, 2, 0, 2);
    d1MainLayout->setSpacing(2);
    d1MainLayout->setAlignment(Qt::AlignCenter);

    QWidget* d1Widget = new QWidget();
    d1Widget->setFixedWidth(104);  // Reduced by 20px
    QHBoxLayout* d1Layout = new QHBoxLayout(d1Widget);
    d1Layout->setContentsMargins(0, 0, 0, 0);
    d1Layout->addWidget(m_diskDrive1, 0, Qt::AlignCenter);

    d1MainLayout->addWidget(d1Widget, 0, Qt::AlignCenter);

    // Add media toggle button to D1 section with same width as section
    m_mediaToggleButton->setFixedWidth(104);
    d1MainLayout->addWidget(m_mediaToggleButton);

    // Create separate cartridge container
    QWidget* cartridgeContainer = new QWidget(this);
    QVBoxLayout* cartridgeMainLayout = new QVBoxLayout(cartridgeContainer);
    cartridgeMainLayout->setContentsMargins(0, 1, 0, 1);
    cartridgeMainLayout->setSpacing(1);
    cartridgeMainLayout->setAlignment(Qt::AlignCenter);

    QWidget* cartridgeWidget = new QWidget();
    cartridgeWidget->setFixedWidth(70);
    QHBoxLayout* cartridgeLayout = new QHBoxLayout(cartridgeWidget);
    cartridgeLayout->setContentsMargins(0, 0, 0, 0);
    cartridgeLayout->addWidget(m_cartridgeWidget, 0, Qt::AlignCenter);

    cartridgeMainLayout->addWidget(cartridgeWidget, 0, Qt::AlignCenter);

    // Create console buttons section
    QWidget* consoleButtonsContainer = new QWidget(this);
    QVBoxLayout* buttonsLayout = new QVBoxLayout(consoleButtonsContainer);
    buttonsLayout->setContentsMargins(2, 2, 2, 2);
    buttonsLayout->setSpacing(1);

    // Create console buttons - wider than tall for compact stacking
    m_startButton = new QPushButton("START", this);
    m_selectButton = new QPushButton("SELECT", this);
    m_optionButton = new QPushButton("OPTION", this);
    m_breakButton = new QPushButton("BREAK", this);
    
    // Create reset/system buttons section
    QWidget* systemButtonsContainer = new QWidget(this);
    QVBoxLayout* systemButtonsLayout = new QVBoxLayout(systemButtonsContainer);
    systemButtonsLayout->setContentsMargins(2, 2, 2, 2);
    systemButtonsLayout->setSpacing(1);
    
    // Create system buttons - same style as console buttons
    QPushButton* coldBootButton = new QPushButton("COLD", this);
    QPushButton* warmBootButton = new QPushButton("WARM", this);
    QPushButton* inverseButton = new QPushButton("INVERSE", this);
    m_pauseButton = new QPushButton("PAUSE", this);

    // Style console buttons - use system palette colors for dark mode compatibility
    QPalette pal = QApplication::palette();
    QColor buttonColor = pal.color(QPalette::Button);
    QColor windowColor = pal.color(QPalette::Window);
    
    // Check if we're in dark mode
    bool isDarkMode = windowColor.lightness() < 128;
    
    // Explicitly set text color based on theme
    QColor buttonTextColor;
    if (isDarkMode) {
        buttonTextColor = QColor(255, 255, 255);  // White text for dark mode
    } else {
        buttonTextColor = QColor(0, 0, 0);  // Black text for light mode
    }
    
    // Create slightly lighter/darker versions for hover and pressed states
    QColor buttonHoverColor;
    QColor buttonPressedColor;
    QColor borderColor;
    
    if (isDarkMode) {
        // Dark mode: make hover lighter and pressed even lighter
        buttonHoverColor = buttonColor.lighter(120);
        buttonPressedColor = buttonColor.lighter(140);
        borderColor = buttonColor.lighter(150);
    } else {
        // Light mode: make hover darker and pressed even darker
        buttonHoverColor = buttonColor.darker(110);
        buttonPressedColor = buttonColor.darker(120);
        borderColor = buttonColor.darker(150);
    }
    
    QString buttonStyle = QString(
        "QPushButton {"
        "    font-size: 9px;"
        "    font-weight: bold;"
        "    padding: 2px 4px;"
        "    margin: 0px;"
        "    border: 1px solid %1;"
        "    background-color: %2;"
        "    color: %3;"
        "    min-width: 50px;"
        "    max-width: 60px;"
        "    min-height: 14px;"
        "    max-height: 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: %4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %5;"
        "    border: 1px solid %6;"
        "}").arg(borderColor.name())
            .arg(buttonColor.name())
            .arg(buttonTextColor.name())
            .arg(buttonHoverColor.name())
            .arg(buttonPressedColor.name())
            .arg(borderColor.darker(120).name());

    m_startButton->setStyleSheet(buttonStyle);
    m_selectButton->setStyleSheet(buttonStyle);
    m_optionButton->setStyleSheet(buttonStyle);
    m_breakButton->setStyleSheet(buttonStyle);
    
    // Apply same style to system buttons
    coldBootButton->setStyleSheet(buttonStyle);
    warmBootButton->setStyleSheet(buttonStyle);
    inverseButton->setStyleSheet(buttonStyle);
    m_pauseButton->setStyleSheet(buttonStyle);

    // Add tooltips
    m_startButton->setToolTip("START button (F2)");
    m_selectButton->setToolTip("SELECT button (F3)");
    m_optionButton->setToolTip("OPTION button (F4)");
    m_breakButton->setToolTip("BREAK key (F7)");
    
    // Add tooltips for system buttons
    coldBootButton->setToolTip("Cold boot (complete restart)");
    warmBootButton->setToolTip("Warm boot (soft reset)");
    inverseButton->setToolTip("INVERSE key (video inverse)");
    m_pauseButton->setToolTip("Pause/Resume emulation");

    // Add buttons to layout (reordered: option, select, start, break)
    buttonsLayout->addWidget(m_optionButton);
    buttonsLayout->addWidget(m_selectButton);
    buttonsLayout->addWidget(m_startButton);
    buttonsLayout->addWidget(m_breakButton);
    
    // Add system buttons to layout
    systemButtonsLayout->addWidget(coldBootButton);
    systemButtonsLayout->addWidget(warmBootButton);
    systemButtonsLayout->addWidget(inverseButton);
    systemButtonsLayout->addWidget(m_pauseButton);

    // Connect console button signals - send press event and delay release to allow one frame processing
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_F2, Qt::NoModifier);
        m_emulator->handleKeyPress(&pressEvent);
        qDebug() << "*** START button clicked - F2 pressed ***";

        // Delay release by one frame (about 16ms) to let emulator process it
        QTimer::singleShot(50, [this]() {
            QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_F2, Qt::NoModifier);
            m_emulator->handleKeyRelease(&releaseEvent);
            qDebug() << "*** START button F2 released ***";
        });
    });
    connect(m_selectButton, &QPushButton::clicked, this, [this]() {
        QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_F3, Qt::NoModifier);
        m_emulator->handleKeyPress(&pressEvent);
        qDebug() << "*** SELECT button clicked - F3 pressed ***";

        // Delay release by one frame
        QTimer::singleShot(50, [this]() {
            QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_F3, Qt::NoModifier);
            m_emulator->handleKeyRelease(&releaseEvent);
            qDebug() << "*** SELECT button F3 released ***";
        });
    });
    connect(m_optionButton, &QPushButton::clicked, this, [this]() {
        QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_F4, Qt::NoModifier);
        m_emulator->handleKeyPress(&pressEvent);
        qDebug() << "*** OPTION button clicked - F4 pressed ***";

        // Delay release by one frame
        QTimer::singleShot(50, [this]() {
            QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_F4, Qt::NoModifier);
            m_emulator->handleKeyRelease(&releaseEvent);
            qDebug() << "*** OPTION button F4 released ***";
        });
    });
    connect(m_breakButton, &QPushButton::clicked, this, [this]() {
        QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_F7, Qt::NoModifier);
        m_emulator->handleKeyPress(&pressEvent);
        qDebug() << "*** BREAK button clicked - F7 pressed ***";

        // Delay release by one frame
        QTimer::singleShot(50, [this]() {
            QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_F7, Qt::NoModifier);
            m_emulator->handleKeyRelease(&releaseEvent);
            qDebug() << "*** BREAK button F7 released ***";
        });
    });
    
    // Connect system button signals
    connect(coldBootButton, &QPushButton::clicked, this, &MainWindow::coldBoot);
    connect(warmBootButton, &QPushButton::clicked, this, &MainWindow::warmBoot);
    
    // Connect inverse key - send AKEY_ATARI directly for inverse video toggle
    connect(inverseButton, &QPushButton::clicked, this, [this]() {
        m_emulator->injectAKey(AKEY_ATARI);
        qDebug() << "*** INVERSE button clicked - AKEY_ATARI injected ***";
    });
    
    // Connect pause button
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::togglePause);

    // Create machine controls container (machine dropdown on top, toggles side by side below)
    QWidget* machineControlsContainer = new QWidget(this);
    QVBoxLayout* machineControlsLayout = new QVBoxLayout(machineControlsContainer);
    machineControlsLayout->setContentsMargins(2, 2, 2, 2);
    machineControlsLayout->setSpacing(2);

    // Machine selector on top
    m_machineCombo = new QComboBox();
    m_machineCombo->setIconSize(QSize(32, 20));
    m_machineCombo->setMinimumWidth(200);
    m_machineCombo->setMaximumWidth(200);

    // Create machine icons (same as original createToolBar)
    auto createMachineIcon = [](const QColor& baseColor, const QString& text) -> QIcon {
        QPixmap pixmap(32, 20);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw computer shape
        painter.setPen(QPen(baseColor.darker(150), 1));
        painter.setBrush(QBrush(baseColor));
        painter.drawRoundedRect(2, 2, 28, 16, 2, 2);

        // Draw screen
        painter.setBrush(QBrush(Qt::black));
        painter.drawRect(4, 4, 12, 8);

        // Draw keyboard area
        painter.setBrush(QBrush(baseColor.darker(120)));
        painter.drawRect(4, 13, 24, 4);

        // Add text label
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(6);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRect(18, 4, 12, 8), Qt::AlignCenter, text);

        return QIcon(pixmap);
    };

    // Add machine items
    m_machineCombo->addItem(createMachineIcon(QColor(139, 69, 19), "400"), "Atari 400/800");
    m_machineCombo->addItem(createMachineIcon(QColor(169, 169, 169), "1200"), "Atari 1200XL");
    m_machineCombo->addItem(createMachineIcon(QColor(192, 192, 192), "XL"), "Atari 800XL");
    m_machineCombo->addItem(createMachineIcon(QColor(105, 105, 105), "XE"), "Atari 130XE");
    m_machineCombo->addItem(createMachineIcon(QColor(85, 85, 85), "320C"), "Atari 320XE (Compy-Shop)");
    m_machineCombo->addItem(createMachineIcon(QColor(75, 75, 75), "320R"), "Atari 320XE (Rambo XL)");
    m_machineCombo->addItem(createMachineIcon(QColor(65, 65, 65), "576"), "Atari 576XE");
    m_machineCombo->addItem(createMachineIcon(QColor(55, 55, 55), "1088"), "Atari 1088XE");
    m_machineCombo->addItem(createMachineIcon(QColor(128, 0, 128), "XEGS"), "Atari XEGS");
    m_machineCombo->addItem(createMachineIcon(QColor(70, 130, 180), "5200"), "Atari 5200");

    m_machineCombo->setCurrentIndex(2); // Default to 800XL
    connect(m_machineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onMachineTypeChanged);

    machineControlsLayout->addWidget(m_machineCombo);

    // Toggle switches container - BASIC, Video, and Speed side by side under machine selector
    QWidget* togglesContainer = new QWidget();
    QHBoxLayout* togglesLayout = new QHBoxLayout(togglesContainer);
    togglesLayout->setContentsMargins(0, 0, 0, 0);
    togglesLayout->setSpacing(6);

    // Create BASIC toggle container
    QWidget* basicContainer = new QWidget();
    QHBoxLayout* basicLayout = new QHBoxLayout(basicContainer);
    basicLayout->setContentsMargins(0, 0, 0, 0);
    basicLayout->setSpacing(4);

    // QLabel* basicLabel = new QLabel("BASIC:");
    // basicLabel->setMinimumWidth(35);
    m_basicToggle = new ToggleSwitch();
    m_basicToggle->setLabels(" BASIC", "OS");
    m_basicToggle->setChecked(m_emulator->isBasicEnabled());
    connect(m_basicToggle, &ToggleSwitch::toggled, this, &MainWindow::toggleBasic);

    // basicLayout->addWidget(basicLabel);
    basicLayout->addWidget(m_basicToggle);

    // Create video system toggle container
    QWidget* videoContainer = new QWidget();
    QHBoxLayout* videoLayout = new QHBoxLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(4);

    // QLabel* videoLabel = new QLabel("Video:");
    // videoLabel->setMinimumWidth(35);
    m_videoToggle = new ToggleSwitch();
    m_videoToggle->setLabels("PAL", "NTSC");
    m_videoToggle->setColors(QColor(70, 130, 180), QColor(70, 130, 180)); // Steel blue for both states
    m_videoToggle->setChecked(true); // Default to PAL (ON position)
    connect(m_videoToggle, &ToggleSwitch::toggled, this, &MainWindow::onVideoSystemToggled);

    // videoLayout->addWidget(videoLabel);
    videoLayout->addWidget(m_videoToggle);

    // Create speed toggle container
    QWidget* speedContainer = new QWidget();
    QHBoxLayout* speedLayout = new QHBoxLayout(speedContainer);
    speedLayout->setContentsMargins(0, 0, 0, 0);
    speedLayout->setSpacing(4);

    // QLabel* speedLabel = new QLabel("Speed:");
    // speedLabel->setMinimumWidth(35);
    m_speedToggle = new ToggleSwitch();
    m_speedToggle->setLabels("MAX", "1x");
    m_speedToggle->setColors(QColor(70, 130, 180), QColor(70, 130, 180)); // Steel blue for both states
    m_speedToggle->setChecked(false); // Default to Real speed (OFF position = authentic Atari speed)
    connect(m_speedToggle, &ToggleSwitch::toggled, this, &MainWindow::onSpeedToggled);

    // speedLayout->addWidget(speedLabel);
    speedLayout->addWidget(m_speedToggle);

    // Add all three toggle switches to the container
    togglesLayout->addWidget(basicContainer);
    togglesLayout->addWidget(videoContainer);
    togglesLayout->addWidget(speedContainer);

    machineControlsLayout->addWidget(togglesContainer);

    // Add to toolbar with separators between each section
    // Order: D1, Cartridge, Machine controls (inserted at beginning)
    m_toolBar->insertWidget(m_toolBar->actions().first(), d1Container);
    m_toolBar->insertSeparator(m_toolBar->actions().at(1));
    m_toolBar->insertWidget(m_toolBar->actions().at(2), cartridgeContainer);
    m_toolBar->insertSeparator(m_toolBar->actions().at(3));
    m_toolBar->insertWidget(m_toolBar->actions().at(4), machineControlsContainer);
    m_toolBar->insertSeparator(m_toolBar->actions().at(5));
    
    // Find the spacer widget index (it's after joystick, audio, profile sections)
    // Count widgets: joystick(1) + sep(1) + audio(1) + sep(1) + profile(1) + sep(1) + spacer(1) = 7
    // Plus the ones we just inserted: d1(1) + sep(1) + cart(1) + sep(1) + machine(1) + sep(1) = 6
    // Total before spacer = 6, so spacer is at index 6+6 = 12

    // Insert console and system buttons after the spacer (logo will be added at the very end)
    int insertIndex = 12;
    
    m_toolBar->insertWidget(m_toolBar->actions().at(insertIndex), consoleButtonsContainer);
    m_toolBar->insertSeparator(m_toolBar->actions().at(insertIndex + 1));
    m_toolBar->insertWidget(m_toolBar->actions().at(insertIndex + 2), systemButtonsContainer);

    // Add logo at the very end (after console and system buttons)
    QFrame* logoSeparator = new QFrame();
    logoSeparator->setFrameShape(QFrame::VLine);
    logoSeparator->setFrameShadow(QFrame::Sunken);
    m_toolBar->addWidget(logoSeparator);

    createLogoSection();
}

void MainWindow::toggleMediaDock()
{
    if (m_mediaPeripheralsDockWidget->isVisible()) {
        m_mediaPeripheralsDockWidget->hide();
        m_mediaToggleButton->setChecked(false);
    } else {
        m_mediaPeripheralsDockWidget->show();
        m_mediaToggleButton->setChecked(true);
    }
}

void MainWindow::onDiskInserted(int driveNumber, const QString& diskPath)
{
    QFileInfo fileInfo(diskPath);
    statusBar()->showMessage(QString("Disk mounted to D%1: %2 - Try typing DIR from BASIC")
                            .arg(driveNumber).arg(fileInfo.fileName()), 5000);
    qDebug() << "Disk inserted in drive" << driveNumber << ":" << diskPath;

    // Save disk insertion to settings
    saveDiskToSettings(driveNumber, diskPath, false); // Assume not read-only for now
}

void MainWindow::onDiskEjected(int driveNumber)
{
    statusBar()->showMessage(QString("Disk ejected from D%1:").arg(driveNumber), 3000);
    qDebug() << "Disk ejected from drive" << driveNumber;

    // Clear disk from settings
    clearDiskFromSettings(driveNumber);
}

void MainWindow::onDriveStateChanged(int driveNumber, bool enabled)
{
    QString message = QString("Drive D%1: %2").arg(driveNumber).arg(enabled ? "On" : "Off");
    statusBar()->showMessage(message, 2000);
    qDebug() << "Drive" << driveNumber << "state changed to" << (enabled ? "on" : "off");

    // Handle drive state change in emulator core
    if (!enabled) {
        // Disable drive in libatari800 core (dismounts disk and sets status to OFF)
        m_emulator->disableDrive(driveNumber);
        qDebug() << "Drive D" << driveNumber << ": disabled in libatari800 core";
    }

    // Save drive state to settings
    saveDriveStateToSettings(driveNumber, enabled);
}

void MainWindow::onCassetteInserted(const QString& cassettePath)
{
    QFileInfo fileInfo(cassettePath);
    statusBar()->showMessage(QString("Cassette inserted: %1").arg(fileInfo.fileName()), 3000);
    qDebug() << "Cassette inserted:" << cassettePath;
}

void MainWindow::onCassetteEjected()
{
    statusBar()->showMessage("Cassette ejected", 2000);
    qDebug() << "Cassette ejected";
}

void MainWindow::onCassetteStateChanged(bool enabled)
{
    QString message = QString("Cassette recorder: %1").arg(enabled ? "On" : "Off");
    statusBar()->showMessage(message, 2000);
    qDebug() << "Cassette state changed to" << (enabled ? "on" : "off");
}

void MainWindow::onCartridgeInserted(const QString& cartridgePath)
{
    QFileInfo fileInfo(cartridgePath);
    statusBar()->showMessage(QString("Cartridge loaded: %1").arg(fileInfo.fileName()), 3000);
    qDebug() << "Cartridge inserted:" << cartridgePath;
    // Note: No manual reboot needed - CARTRIDGE_SetTypeAutoReboot handles the reboot
}

void MainWindow::onCartridgeEjected()
{
    statusBar()->showMessage("Cartridge ejected", 2000);
    qDebug() << "Cartridge ejected";
}

void MainWindow::loadInitialSettings()
{
    QSettings settings("8bitrelics", "Fujisan");

    // Debug checkbox state vs settings
    if (m_joystickEnabledCheck) {
        qDebug() << "=== JOYSTICK CHECKBOX STATE AT STARTUP ===";
        qDebug() << "Checkbox visual state:" << m_joystickEnabledCheck->isChecked();
        qDebug() << "Settings value:" << settings.value("input/joystickEnabled", true).toBool();
    }

    // Sync before reading to ensure we get fresh values from disk
    settings.sync();
    
#ifdef Q_OS_MACOS
    // Force refresh on macOS to avoid cached values
    QSettings::setDefaultFormat(QSettings::NativeFormat);
#endif
    
    // Load saved settings or use defaults
    QString machineType = settings.value("machine/type", "-xl").toString();
    QString videoSystem = settings.value("machine/videoSystem", "-pal").toString();
    bool basicEnabled = settings.value("machine/basicEnabled", true).toBool();
    bool altirraOSEnabled = settings.value("machine/altirraOS", false).toBool();
    bool altirraBASICEnabled = settings.value("machine/altirraBASIC", false).toBool();
    
    qDebug() << "MainWindow loading settings - BASIC:" << basicEnabled 
             << "Altirra OS:" << altirraOSEnabled 
             << "Altirra BASIC:" << altirraBASICEnabled
             << "Settings file:" << settings.fileName();
    bool audioEnabled = settings.value("audio/enabled", true).toBool();
    QString artifactMode = settings.value("video/artifacting", "none").toString();

    // Load display settings for initial setup - use "full" by default to avoid cropping
    QString horizontalArea = settings.value("video/horizontalArea", "full").toString();
    QString verticalArea = settings.value("video/verticalArea", "full").toString();
    int horizontalShift = settings.value("video/horizontalShift", 0).toInt();
    int verticalShift = settings.value("video/verticalShift", 0).toInt();
    QString fitScreen = settings.value("video/fitScreen", "both").toString();
    bool show80Column = settings.value("video/show80Column", false).toBool();
    bool vSyncEnabled = settings.value("video/vSyncEnabled", false).toBool();

    qDebug() << "Loading initial settings - Machine:" << machineType
             << "Video:" << videoSystem << "BASIC:" << basicEnabled << "Artifacts:" << artifactMode;

    // Load input settings for keyboard joystick emulation
    bool joystickEnabled = settings.value("input/joystickEnabled", true).toBool();  // Main joystick support
    bool kbdJoy0Saved = settings.value("input/kbdJoy0Enabled", true).toBool();     // Saved kbd joy0 state
    bool kbdJoy1Saved = settings.value("input/kbdJoy1Enabled", false).toBool();    // Saved kbd joy1 state
    bool swapJoysticks = settings.value("input/swapJoysticks", false).toBool();     // Default false: Joy0=Numpad, Joy1=WASD
    
    qDebug() << "=== LOADED JOYSTICK SETTINGS FROM QSETTINGS ===";
    qDebug() << "Main joystick enabled from settings:" << joystickEnabled;
    qDebug() << "Kbd Joy0 saved state:" << kbdJoy0Saved;
    qDebug() << "Kbd Joy1 saved state:" << kbdJoy1Saved;
    
    // Only enable keyboard joysticks if main joystick support is enabled
    bool kbdJoy0Enabled = joystickEnabled && kbdJoy0Saved;
    bool kbdJoy1Enabled = joystickEnabled && kbdJoy1Saved;

    // Load special device settings
    bool netSIOEnabled = settings.value("media/netSIOEnabled", false).toBool();
    bool rtimeEnabled = settings.value("media/rtimeEnabled", false).toBool();

    qDebug() << "Input settings - KbdJoy0:" << kbdJoy0Enabled << "KbdJoy1:" << kbdJoy1Enabled << "Swap:" << swapJoysticks;
    qDebug() << "Special devices - NetSIO:" << netSIOEnabled << "RTime:" << rtimeEnabled;

    // Load ROM paths BEFORE initialization
    QString osRomKey = QString("machine/osRom_%1").arg(machineType.mid(1)); // Remove the '-' prefix
    QString osRomPath = settings.value(osRomKey, "").toString();
    QString basicRomPath = settings.value("machine/basicRom", "").toString();

    // Set emulator settings before initialization
    m_emulator->setAltirraOSEnabled(altirraOSEnabled);
    m_emulator->setAltirraBASICEnabled(altirraBASICEnabled);
    m_emulator->setOSRomPath(osRomPath);
    m_emulator->setBasicRomPath(basicRomPath);
    m_emulator->enableAudio(audioEnabled);
    
    qDebug() << "ROM paths - OS:" << osRomPath << "BASIC:" << basicRomPath;
    qDebug() << "Altirra OS enabled:" << altirraOSEnabled;
    qDebug() << "Altirra BASIC enabled:" << altirraBASICEnabled;

    // Initialize emulator with loaded settings including display and input options
    if (!m_emulator->initializeWithNetSIOConfig(basicEnabled, machineType, videoSystem, artifactMode,
                                               horizontalArea, verticalArea, horizontalShift, verticalShift,
                                               fitScreen, show80Column, vSyncEnabled,
                                               kbdJoy0Enabled, kbdJoy1Enabled, swapJoysticks,
                                               netSIOEnabled, rtimeEnabled)) {
        QMessageBox::critical(this, "Error", "Failed to initialize Atari800 emulator");
        QApplication::quit();
        return;
    }
    
    // Ensure keyboard joystick state is properly applied after initialization
    // This is needed because the initial state from command line args might not match
    // the effective state we want (main joystick disabled should disable kbd joy)
    m_emulator->setKbdJoy0Enabled(kbdJoy0Enabled);
    m_emulator->setKbdJoy1Enabled(kbdJoy1Enabled);
    qDebug() << "Applied keyboard joystick state after init - Joy0:" << kbdJoy0Enabled << "Joy1:" << kbdJoy1Enabled;

#ifdef HAVE_SDL2_JOYSTICK
    // Load USB joystick device assignments
    // This ensures USB joysticks work on startup, not just after opening settings dialog
    QString joystick1Device = settings.value("input/joystick1Device", "keyboard").toString();
    QString joystick2Device = settings.value("input/joystick2Device", "keyboard").toString();

    m_emulator->setJoystick1Device(joystick1Device);
    m_emulator->setJoystick2Device(joystick2Device);

    // Enable SDL joystick manager if any SDL device is selected
    bool realJoysticksNeeded = joystick1Device.startsWith("sdl_") || joystick2Device.startsWith("sdl_");
    m_emulator->setRealJoysticksEnabled(realJoysticksNeeded);

    qDebug() << "Applied joystick device assignments - Joy1:" << joystick1Device
             << "Joy2:" << joystick2Device << "SDL enabled:" << realJoysticksNeeded;
#endif

    // Load and apply media settings (disk images, etc.)
    loadAndApplyMediaSettings();

    // Load and apply speed settings
    bool turboMode = settings.value("machine/turboMode", false).toBool();
    int speedIndex = settings.value("machine/emulationSpeedIndex", 1).toInt(); // Default to 1x (index 1)

    // Convert speed index to percentage
    int speedPercentage;
    if (turboMode) {
        // Turbo mode = host speed (unlimited)
        speedPercentage = 0;
    } else if (speedIndex == 0) {
        // Index 0 = 0.5x speed
        speedPercentage = 50;
    } else {
        // Index 1-10 = 1x-10x speed (index * 100)
        speedPercentage = speedIndex * 100;
    }

    m_emulator->setEmulationSpeed(speedPercentage);
    qDebug() << "Applied speed settings - Turbo:" << turboMode << "Index:" << speedIndex << "Percentage:" << speedPercentage;

    // Update toolbar to reflect loaded settings
    updateToolbarFromSettings();
}

void MainWindow::loadAndApplyMediaSettings()
{
    QSettings settings("8bitrelics", "Fujisan");

    qDebug() << "Loading and applying media settings...";

    // Load and apply cartridge settings first (before disk images)
    bool cartridgeEnabled = settings.value("machine/cartridgeEnabled", false).toBool();
    QString cartridgePath = settings.value("machine/cartridgePath", "").toString();

    if (cartridgeEnabled && !cartridgePath.isEmpty()) {
        qDebug() << "Auto-loading cartridge:" << cartridgePath;

        if (m_emulator->loadFile(cartridgePath)) {
            qDebug() << "Successfully auto-loaded cartridge:" << cartridgePath;
        } else {
            qDebug() << "Failed to auto-load cartridge:" << cartridgePath;
        }
    }

    // Load piggyback cartridge if enabled
    bool cartridge2Enabled = settings.value("machine/cartridge2Enabled", false).toBool();
    QString cartridge2Path = settings.value("machine/cartridge2Path", "").toString();

    if (cartridge2Enabled && !cartridge2Path.isEmpty()) {
        qDebug() << "Auto-loading piggyback cartridge:" << cartridge2Path;

        if (m_emulator->loadFile(cartridge2Path)) {
            qDebug() << "Successfully auto-loaded piggyback cartridge:" << cartridge2Path;
        } else {
            qDebug() << "Failed to auto-load piggyback cartridge:" << cartridge2Path;
        }
    }

    // Load and mount disk images for D1-D8
    for (int i = 0; i < 8; i++) {
        QString diskKey = QString("media/disk%1").arg(i + 1);
        bool diskEnabled = settings.value(diskKey + "Enabled", false).toBool();
        QString diskPath = settings.value(diskKey + "Path", "").toString();
        bool diskReadOnly = settings.value(diskKey + "ReadOnly", false).toBool();

        if (diskEnabled && !diskPath.isEmpty()) {
            qDebug() << QString("Auto-mounting D%1: %2 (read-only: %3)")
                        .arg(i + 1).arg(diskPath).arg(diskReadOnly);

            if (m_emulator->mountDiskImage(i + 1, diskPath, diskReadOnly)) {
                qDebug() << QString("Successfully auto-mounted D%1:").arg(i + 1);

                // Update the corresponding disk widget to reflect the mounted disk
                if (i + 1 == 1 && m_diskDrive1) {
                    // D1 is on toolbar
                    m_diskDrive1->setDriveEnabled(true);
                    m_diskDrive1->updateFromEmulator();
                } else if (i + 1 >= 2 && i + 1 <= 8 && m_mediaPeripheralsDock) {
                    // D2-D8 are in dock
                    DiskDriveWidget* driveWidget = m_mediaPeripheralsDock->getDriveWidget(i + 1);
                    if (driveWidget) {
                        driveWidget->setDriveEnabled(true);
                        driveWidget->updateFromEmulator();
                    }
                }
            } else {
                qDebug() << QString("Failed to auto-mount D%1:").arg(i + 1);
            }
        }
    }

    // Update cartridge widget from emulator state
    if (m_cartridgeWidget) {
        m_cartridgeWidget->updateFromEmulator();
    }

    // TODO: Load cassette settings
    bool cassetteEnabled = settings.value("media/cassetteEnabled", false).toBool();
    QString cassettePath = settings.value("media/cassettePath", "").toString();
    if (cassetteEnabled && !cassettePath.isEmpty()) {
        qDebug() << "Cassette auto-load not yet implemented";
    }

    // TODO: Load hard drive settings
    for (int i = 0; i < 4; i++) {
        QString hdKey = QString("media/hd%1").arg(i + 1);
        bool hdEnabled = settings.value(hdKey + "Enabled", false).toBool();
        QString hdPath = settings.value(hdKey + "Path", "").toString();

        if (hdEnabled && !hdPath.isEmpty()) {
            qDebug() << QString("H%1: hard drive auto-mount not yet implemented").arg(i + 1);
        }
    }

    qDebug() << "Media settings loaded and applied";
}

void MainWindow::saveDiskToSettings(int driveNumber, const QString& diskPath, bool readOnly)
{
    QSettings settings("8bitrelics", "Fujisan");
    QString diskKey = QString("media/disk%1").arg(driveNumber);

    settings.setValue(diskKey + "Enabled", true);
    settings.setValue(diskKey + "Path", diskPath);
    settings.setValue(diskKey + "ReadOnly", readOnly);
    settings.sync();

    qDebug() << QString("Saved D%1 to settings: %2 (read-only: %3)")
                .arg(driveNumber).arg(diskPath).arg(readOnly);
}

void MainWindow::clearDiskFromSettings(int driveNumber)
{
    QSettings settings("8bitrelics", "Fujisan");
    QString diskKey = QString("media/disk%1").arg(driveNumber);

    settings.setValue(diskKey + "Enabled", false);
    settings.setValue(diskKey + "Path", "");
    settings.setValue(diskKey + "ReadOnly", false);
    settings.sync();

    qDebug() << QString("Cleared D%1 from settings").arg(driveNumber);
}

void MainWindow::saveDriveStateToSettings(int driveNumber, bool enabled)
{
    QSettings settings("8bitrelics", "Fujisan");
    QString diskKey = QString("media/disk%1").arg(driveNumber);

    // Only update the enabled state, preserve existing path and read-only settings
    settings.setValue(diskKey + "Enabled", enabled);
    settings.sync();

    qDebug() << QString("Saved D%1 state to settings: %2").arg(driveNumber).arg(enabled ? "enabled" : "disabled");
}

void MainWindow::loadVideoSettings()
{
    QSettings settings("8bitrelics", "Fujisan");
    m_keepAspectRatio = settings.value("video/keepAspectRatio", true).toBool();
    m_startInFullscreen = settings.value("video/fullscreenMode", false).toBool();

    // Apply fullscreen setting based on preference
    if (m_startInFullscreen && !m_isInCustomFullscreen) {
        enterCustomFullscreen();
    } else if (!m_startInFullscreen && m_isInCustomFullscreen) {
        exitCustomFullscreen();
    }

    qDebug() << "Video settings loaded - Keep aspect ratio:" << m_keepAspectRatio
             << "Start fullscreen:" << m_startInFullscreen;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (m_keepAspectRatio && m_emulatorWidget) {
        // Calculate the 4:3 aspect ratio constraints
        const double targetAspectRatio = 4.0 / 3.0;

        // Get the current size of the central widget area (excluding toolbar/menubar)
        QSize currentSize = event->size();
        int toolbarHeight = m_toolBar->height();
        int menuHeight = menuBar()->height();
        int statusHeight = statusBar()->height();

        int availableWidth = currentSize.width();
        int availableHeight = currentSize.height() - toolbarHeight - menuHeight - statusHeight;

        // Calculate the optimal size maintaining 4:3 ratio
        int optimalWidth, optimalHeight;

        if ((double)availableWidth / availableHeight > targetAspectRatio) {
            // Window is too wide, constrain by height
            optimalHeight = availableHeight;
            optimalWidth = (int)(optimalHeight * targetAspectRatio);
        } else {
            // Window is too tall, constrain by width
            optimalWidth = availableWidth;
            optimalHeight = (int)(optimalWidth / targetAspectRatio);
        }

        // Calculate the total window size needed
        int totalWidth = optimalWidth;
        int totalHeight = optimalHeight + toolbarHeight + menuHeight + statusHeight;

        // Only resize if the current size doesn't match our target
        QSize targetSize(totalWidth, totalHeight);
        if (event->size() != targetSize) {
            // Temporarily disable aspect ratio to prevent infinite recursion
            bool wasKeepingAspect = m_keepAspectRatio;
            m_keepAspectRatio = false;
            resize(targetSize);
            m_keepAspectRatio = wasKeepingAspect;
        }
    }
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_fullscreenWidget && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // Handle fullscreen toggle shortcut
#ifdef Q_OS_MACOS
        if (keyEvent->modifiers() & Qt::ControlModifier && keyEvent->key() == Qt::Key_Return) {
#else
        if (keyEvent->key() == Qt::Key_F11) {
#endif
            toggleFullscreen();
            return true;
        }

        // Handle Escape key to exit fullscreen
        if (keyEvent->key() == Qt::Key_Escape) {
            exitCustomFullscreen();
            return true;
        }

        // Let the fullscreen emulator widget handle all other keys normally
        // Don't intercept - let the event propagate naturally
    }

    return QMainWindow::eventFilter(object, event);
}

void MainWindow::toggleDebugger()
{
    if (m_debuggerDock->isVisible()) {
        m_debuggerDock->hide();
    } else {
        m_debuggerDock->show();
        m_debuggerWidget->updateCPUState();
        m_debuggerWidget->updateMemoryView();
    }
}

void MainWindow::quickSaveState()
{
    // Update emulator with current profile name
    QString profileName = m_profileCombo->currentText();
    m_emulator->setCurrentProfileName(profileName);

    if (m_emulator->quickSaveState()) {
        statusBar()->showMessage("Quick state saved", 2000);
    } else {
        QMessageBox::warning(this, "Quick Save Failed", "Failed to save state");
    }

    // Restore focus to emulator widget (fixes Windows/Linux focus loss)
    if (m_emulatorWidget) {
        m_emulatorWidget->setFocus();
    }
}

void MainWindow::quickLoadState()
{
    if (m_emulator->quickLoadState()) {
        // Get the profile name from the loaded state
        QString profileName = m_emulator->getCurrentProfileName();

        // Try to select the profile in the combo box
        int index = m_profileCombo->findText(profileName);
        if (index >= 0) {
            m_profileCombo->setCurrentIndex(index);
        } else if (!profileName.isEmpty() && profileName != "Default") {
            statusBar()->showMessage(QString("Profile '%1' not found, using current").arg(profileName), 3000);
        }

        statusBar()->showMessage("Quick state loaded", 2000);
    } else {
        QMessageBox::warning(this, "Quick Load Failed", "No quick save state found");
    }

    // Restore focus to emulator widget (fixes Windows/Linux focus loss)
    if (m_emulatorWidget) {
        m_emulatorWidget->setFocus();
    }
}

void MainWindow::saveState()
{
    QString filename = QFileDialog::getSaveFileName(this, 
        "Save State", 
        QDir::homePath(), 
        "Atari State Files (*.a8s);;All Files (*)");
    
    if (!filename.isEmpty()) {
        // Ensure .a8s extension
        if (!filename.endsWith(".a8s", Qt::CaseInsensitive)) {
            filename += ".a8s";
        }
        
        // Update emulator with current profile name
        QString profileName = m_profileCombo->currentText();
        m_emulator->setCurrentProfileName(profileName);
        
        if (m_emulator->saveState(filename)) {
            statusBar()->showMessage(QString("State saved to %1").arg(QFileInfo(filename).fileName()), 3000);
        } else {
            QMessageBox::warning(this, "Save Failed", "Failed to save state");
        }
    }
}

void MainWindow::loadState()
{
    QString filename = QFileDialog::getOpenFileName(this, 
        "Load State", 
        QDir::homePath(), 
        "Atari State Files (*.a8s);;All Files (*)");
    
    if (!filename.isEmpty()) {
        if (m_emulator->loadState(filename)) {
            // Get the profile name from the loaded state
            QString profileName = m_emulator->getCurrentProfileName();
            
            // Try to select the profile in the combo box
            int index = m_profileCombo->findText(profileName);
            if (index >= 0) {
                m_profileCombo->setCurrentIndex(index);
            } else if (!profileName.isEmpty() && profileName != "Default") {
                statusBar()->showMessage(QString("Profile '%1' not found, using current").arg(profileName), 3000);
            }
            
            statusBar()->showMessage(QString("State loaded from %1").arg(QFileInfo(filename).fileName()), 3000);
        } else {
            QMessageBox::warning(this, "Load Failed", "Failed to load state");
        }
    }
}

void MainWindow::pasteText()
{
    if (!m_emulator) {
        return;
    }

    QClipboard* clipboard = QApplication::clipboard();
    QString text = clipboard->text();

    if (text.isEmpty()) {
        qDebug() << "Clipboard is empty, nothing to paste";
        return;
    }

    sendTextToEmulator(text);
}

void MainWindow::sendTextToEmulator(const QString& text)
{
    if (!m_emulator || text.isEmpty()) {
        return;
    }

    // Stop any existing paste operation
    if (m_pasteTimer->isActive()) {
        m_pasteTimer->stop();
        // Restore original speed if previous paste was interrupted
        m_emulator->setEmulationSpeed(m_originalEmulationSpeed);
    }

    // Save current emulation speed and boost to 200% for fast, reliable pasting
    m_originalEmulationSpeed = m_emulator->getCurrentEmulationSpeed();
    m_emulator->setEmulationSpeed(200);

    // Use fixed timer interval optimized for 200% speed
    m_pasteTimer->setInterval(37);  // ~37ms per character at 2x speed

    // Setup the paste buffer
    m_pasteBuffer = text;
    m_pasteIndex = 0;

    // Start the timer to send characters
    m_pasteTimer->start();

}

void MainWindow::sendNextCharacter()
{
    if (!m_emulator) {
        return;
    }

    // Check if we've finished all characters
    if (m_pasteIndex >= m_pasteBuffer.length()) {
        // Finished pasting - restore original emulation speed
        m_pasteTimer->stop();
        m_emulator->setEmulationSpeed(m_originalEmulationSpeed);
        m_pasteBuffer.clear();
        m_pasteIndex = 0;
        return;
    }

    // Send character and immediately clear in single phase
    QChar ch = m_pasteBuffer.at(m_pasteIndex);
    m_emulator->injectCharacter(ch.toLatin1());
    m_emulator->clearInput();
    m_pasteIndex++;

    // Allow some processing time
    QCoreApplication::processEvents();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Exit fullscreen if active
    if (m_isInCustomFullscreen) {
        exitCustomFullscreen();
    }

    // Stop paste timer if running and restore speed
    if (m_pasteTimer && m_pasteTimer->isActive()) {
        m_pasteTimer->stop();
        if (m_emulator) {
            m_emulator->setEmulationSpeed(m_originalEmulationSpeed);
        }
    }

    if (m_emulator) {
        m_emulator->shutdown();
    }
    event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    
    if (event->type() == QEvent::PaletteChange) {
        // Update the toolbar logo when the theme changes
        updateToolbarLogo();
        // Update button styles for the new theme
        updateToolbarButtonStyles();
    }
}

void MainWindow::refreshProfileList()
{
    if (!m_profileCombo || !m_profileManager) return;

    QString currentProfile = m_profileCombo->currentText();
    m_profileCombo->clear();
    
    m_profileManager->refreshProfileList();
    QStringList profiles = m_profileManager->getProfileNames();
    m_profileCombo->addItems(profiles);
    
    // Restore selection if possible
    if (!currentProfile.isEmpty()) {
        int index = m_profileCombo->findText(currentProfile);
        if (index >= 0) {
            m_profileCombo->setCurrentIndex(index);
        }
    }
    
    // Select current profile if none selected
    if (m_profileCombo->currentText().isEmpty()) {
        QString currentProfileName = m_profileManager->getCurrentProfileName();
        int index = m_profileCombo->findText(currentProfileName);
        if (index >= 0) {
            m_profileCombo->setCurrentIndex(index);
        }
    }
}

void MainWindow::onLoadProfile()
{
    if (!m_profileCombo || !m_profileManager) return;

    QString profileName = m_profileCombo->currentText();
    if (profileName.isEmpty()) return;

    qDebug() << "Loading profile:" << profileName;

    ConfigurationProfile profile = m_profileManager->loadProfile(profileName);
    if (profile.isValid()) {
        // Apply profile to emulator
        applyProfileToEmulator(profile);

        // Update current profile
        m_profileManager->setCurrentProfileName(profileName);

        qDebug() << "Profile loaded successfully:" << profileName;
    } else {
        qWarning() << "Failed to load profile:" << profileName;
    }

    // Restore focus to emulator widget (fixes Windows/Linux focus loss)
    if (m_emulatorWidget) {
        m_emulatorWidget->setFocus();
    }
}

void MainWindow::applyProfileToEmulator(const ConfigurationProfile& profile)
{
    if (!m_emulator) return;

    // Apply machine configuration
    m_emulator->setMachineType(profile.machineType);
    m_emulator->setVideoSystem(profile.videoSystem);
    m_emulator->setBasicEnabled(profile.basicEnabled);

    // Apply audio configuration
    m_emulator->enableAudio(profile.audioEnabled);
    m_emulator->setVolume(profile.audioVolume / 100.0);

    // Apply speed settings
    int speedPercentage;
    if (profile.turboMode) {
        // Turbo mode = host speed (unlimited)
        speedPercentage = 0;
    } else if (profile.emulationSpeedIndex == 0) {
        // Index 0 = 0.5x speed
        speedPercentage = 50;
    } else {
        // Index 1-10 = 1x-10x speed (index * 100)
        speedPercentage = profile.emulationSpeedIndex * 100;
    }
    m_emulator->setEmulationSpeed(speedPercentage);

    // Update toolbar speed toggle to reflect profile speed
    if (m_speedToggle) {
        m_speedToggle->blockSignals(true);
        m_speedToggle->setChecked(profile.turboMode); // Turbo mode = Full speed toggle ON
        m_speedToggle->blockSignals(false);
    }

    // Update UI to reflect changes
    updateToolbarFromSettings();

    // Cold boot to apply machine configuration changes
    coldBoot();

    qDebug() << "Applied profile to emulator and rebooted:" << profile.name
             << "Speed:" << speedPercentage << "% (Turbo:" << profile.turboMode << ")";
}

void MainWindow::onDiskDroppedOnEmulator(const QString& filename)
{
    if (!m_emulator || !m_diskDrive1) {
        qWarning() << "Cannot mount disk - emulator or D1 drive not initialized";
        return;
    }
    
    QFileInfo fileInfo(filename);
    qDebug() << "Mounting disk image dropped on emulator to D1:" << filename;
    
    // Enable D1 drive if it's currently disabled
    if (!m_diskDrive1->isDriveEnabled()) {
        qDebug() << "Enabling D1 drive for dropped disk";
        m_diskDrive1->setDriveEnabled(true);
        saveDriveStateToSettings(1, true);
    }
    
    // Mount the disk image to D1
    if (m_emulator->mountDiskImage(1, filename, false)) {
        qDebug() << "Successfully mounted disk to D1:" << fileInfo.fileName();
        
        // Save disk to settings
        saveDiskToSettings(1, filename, false);
        
        // Update D1 widget UI to reflect the new disk
        if (m_diskDrive1) {
            m_diskDrive1->updateFromEmulator();
            qDebug() << "Updated D1 widget state to reflect mounted disk";
        }
        
        // Show status message
        statusBar()->showMessage(QString("Disk mounted to D1: %1 - Rebooting...").arg(fileInfo.fileName()), 3000);
        
        // Perform cold restart to boot from the new disk
        m_emulator->coldRestart();
        qDebug() << "Cold restart triggered after mounting disk to D1";
        
    } else {
        qWarning() << "Failed to mount disk image to D1:" << filename;
        statusBar()->showMessage(QString("Failed to mount disk: %1").arg(fileInfo.fileName()), 5000);
    }
}

void MainWindow::onPrinterEnabledChanged(bool enabled)
{
    if (m_emulator) {
        m_emulator->setPrinterEnabled(enabled);
        
        if (enabled) {
            // Set up the printer output callback to capture text
            PrinterWidget* printerWidget = m_mediaPeripheralsDock->getPrinterWidget();
            if (printerWidget) {
                m_emulator->setPrinterOutputCallback([printerWidget](const QString& text) {
                    printerWidget->appendText(text);
                });
            }
            
            statusBar()->showMessage("Printer enabled - P: device ready", 2000);
            qDebug() << "Printer enabled via MainWindow";
        } else {
            // Clear the callback
            m_emulator->setPrinterOutputCallback(nullptr);
            statusBar()->showMessage("Printer disabled", 2000);
            qDebug() << "Printer disabled via MainWindow";
        }
    }
}

void MainWindow::onPrinterOutputFormatChanged(const QString& format)
{
    qDebug() << "Printer output format changed to:" << format;
    // Format changes are handled internally by PrinterWidget
}

void MainWindow::onPrinterTypeChanged(const QString& type)
{
    qDebug() << "Printer type changed to:" << type;
    // Printer type changes are handled internally by PrinterWidget for now
    // Future: could configure emulator for specific printer characteristics
}

void MainWindow::toggleTCPServer()
{
    if (m_tcpServer->isRunning()) {
        // Stop the TCP server
        m_tcpServer->stopServer();
        m_tcpServerAction->setChecked(false);
        m_tcpServerAction->setText("&TCP Server");
        m_tcpServerAction->setToolTip("Start TCP server for remote control");
        
        statusBar()->showMessage("TCP Server stopped", 3000);
        qDebug() << "TCP Server stopped by user";
    } else {
        // Start the TCP server
        QSettings settings;
        int tcpPort = settings.value("emulator/tcpServerPort", 6502).toInt();
        
        bool success = m_tcpServer->startServer(tcpPort);
        if (success) {
            m_tcpServerAction->setChecked(true);
            m_tcpServerAction->setText("&TCP Server (Running)");
            m_tcpServerAction->setToolTip(QString("Stop TCP server (currently running on localhost:%1)").arg(tcpPort));
            
            statusBar()->showMessage(QString("TCP Server started on localhost:%1").arg(tcpPort), 5000);
            qDebug() << "TCP Server started successfully on port" << tcpPort;
        } else {
            m_tcpServerAction->setChecked(false);
            statusBar()->showMessage(QString("Failed to start TCP Server on port %1").arg(tcpPort), 5000);
            qDebug() << "Failed to start TCP Server on port" << tcpPort;
        }
    }
}

void MainWindow::requestEmulatorRestart()
{
    // This is called by the TCP server to perform a proper restart with new configuration
    qDebug() << "TCP Server requested emulator restart with configuration changes";
    qDebug() << "Current BASIC setting before restart:" << m_emulator->isBasicEnabled();
    qDebug() << "Current machine type:" << m_emulator->getMachineType();
    qDebug() << "Current video system:" << m_emulator->getVideoSystem();
    restartEmulator();
}

bool MainWindow::insertDiskViaTCP(int driveNumber, const QString& diskPath)
{
    qDebug() << "MainWindow::insertDiskViaTCP called for drive" << driveNumber << "path:" << diskPath;

    if (driveNumber == 1 && m_diskDrive1) {
        // For D1, use the toolbar disk drive widget
        m_diskDrive1->setDriveEnabled(true);  // Enable the drive first
        m_diskDrive1->insertDisk(diskPath);   // Insert the disk
        return true;
    } else if (driveNumber >= 2 && driveNumber <= 8 && m_mediaPeripheralsDock) {
        // For D2-D8, use the widget from MediaPeripheralsDock
        DiskDriveWidget* widget = m_mediaPeripheralsDock->getDriveWidget(driveNumber);
        if (widget) {
            widget->setDriveEnabled(true);  // Enable the drive first
            widget->insertDisk(diskPath);   // Insert the disk
            return true;
        }
    }

    qDebug() << "MainWindow::insertDiskViaTCP failed - invalid drive or widget not available";
    return false;
}

bool MainWindow::ejectDiskViaTCP(int driveNumber)
{
    qDebug() << "MainWindow::ejectDiskViaTCP called for drive" << driveNumber;

    if (driveNumber == 1 && m_diskDrive1) {
        // For D1, use the toolbar disk drive widget
        m_diskDrive1->ejectDisk();
        return true;
    } else if (driveNumber >= 2 && driveNumber <= 8 && m_mediaPeripheralsDock) {
        // For D2-D8, use the widget from MediaPeripheralsDock
        DiskDriveWidget* widget = m_mediaPeripheralsDock->getDriveWidget(driveNumber);
        if (widget) {
            widget->ejectDisk();
            return true;
        }
    }

    qDebug() << "MainWindow::ejectDiskViaTCP failed - invalid drive or widget not available";
    return false;
}

bool MainWindow::enableDriveViaTCP(int driveNumber, bool enabled)
{
    qDebug() << "MainWindow::enableDriveViaTCP called for drive" << driveNumber << "enabled:" << enabled;

    if (driveNumber == 1 && m_diskDrive1) {
        // For D1, use the toolbar disk drive widget
        m_diskDrive1->setDriveEnabled(enabled);
        return true;
    } else if (driveNumber >= 2 && driveNumber <= 8 && m_mediaPeripheralsDock) {
        // For D2-D8, use the widget from MediaPeripheralsDock
        DiskDriveWidget* widget = m_mediaPeripheralsDock->getDriveWidget(driveNumber);
        if (widget) {
            widget->setDriveEnabled(enabled);
            return true;
        }
    }

    qDebug() << "MainWindow::enableDriveViaTCP failed - invalid drive or widget not available";
    return false;
}

bool MainWindow::insertCartridgeViaTCP(const QString& cartridgePath)
{
    qDebug() << "MainWindow::insertCartridgeViaTCP called with path:" << cartridgePath;
    
    if (m_cartridgeWidget) {
        m_cartridgeWidget->loadCartridge(cartridgePath);
        return true;
    }
    
    qDebug() << "MainWindow::insertCartridgeViaTCP failed - cartridge widget not available";
    return false;
}

bool MainWindow::ejectCartridgeViaTCP()
{
    qDebug() << "MainWindow::ejectCartridgeViaTCP called";
    
    if (m_cartridgeWidget) {
        m_cartridgeWidget->ejectCartridge();
        return true;
    }
    
    qDebug() << "MainWindow::ejectCartridgeViaTCP failed - cartridge widget not available";
    return false;
}

void MainWindow::applyProfileViaTCP(const ConfigurationProfile& profile)
{
    qDebug() << "MainWindow::applyProfileViaTCP called for profile:" << profile.name;
    
    // Apply the profile using the existing private method
    applyProfileToEmulator(profile);
    
    // Update current profile name in the manager
    if (m_profileManager) {
        m_profileManager->setCurrentProfileName(profile.name);
    }
    
    // Update the profile combo box to reflect the change
    if (m_profileCombo) {
        int index = m_profileCombo->findText(profile.name);
        if (index >= 0) {
            m_profileCombo->setCurrentIndex(index);
            qDebug() << "Updated profile combo box to:" << profile.name;
        } else {
            qWarning() << "Profile not found in combo box:" << profile.name;
        }
    }
}

void MainWindow::togglePause()
{
    if (m_emulator->isEmulationPaused()) {
        m_emulator->resumeEmulation();
        m_pauseButton->setText("PAUSE");
        m_pauseAction->setText("&Pause");
        m_pauseAction->setChecked(false);
        statusBar()->showMessage("Emulation resumed", 2000);
    } else {
        m_emulator->pauseEmulation();
        m_pauseButton->setText("RESUME");
        m_pauseAction->setText("&Resume");
        m_pauseAction->setChecked(true);
        statusBar()->showMessage("Emulation paused", 2000);
    }
}
