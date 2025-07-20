/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "mainwindow.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_emulator(new AtariEmulator(this))
    , m_emulatorWidget(nullptr)
    , m_keepAspectRatio(true)
    , m_startInFullscreen(false)
    , m_isInCustomFullscreen(false)
    , m_fullscreenWidget(nullptr)
{
    setWindowTitle("Fujisan");
    setMinimumSize(800, 600);
    resize(1280, 960);
    
    createMenus();
    createToolBar();
    createEmulatorWidget();
    
    // Load initial settings and initialize emulator with them
    loadInitialSettings();
    loadVideoSettings();
    
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
    
    m_loadRomAction = new QAction("&Load ROM...", this);
    m_loadRomAction->setShortcut(QKeySequence::Open);
    connect(m_loadRomAction, &QAction::triggered, this, &MainWindow::loadRom);
    fileMenu->addAction(m_loadRomAction);
    
    m_loadDiskAction = new QAction("Load &Disk Image...", this);
    m_loadDiskAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(m_loadDiskAction, &QAction::triggered, this, &MainWindow::loadDiskImage);
    fileMenu->addAction(m_loadDiskAction);
    
    fileMenu->addSeparator();
    
    m_coldBootAction = new QAction("&Cold Reset", this);
    connect(m_coldBootAction, &QAction::triggered, this, &MainWindow::coldBoot);
    fileMenu->addAction(m_coldBootAction);
    
    m_warmBootAction = new QAction("&Warm Reset", this);
    connect(m_warmBootAction, &QAction::triggered, this, &MainWindow::warmBoot);
    fileMenu->addAction(m_warmBootAction);
    
    fileMenu->addSeparator();
    
    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(m_exitAction);
    
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
    
    systemMenu->addSeparator();
    
    m_settingsAction = new QAction("&Settings...", this);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    systemMenu->addAction(m_settingsAction);
    
    // Machine menu (now just for video system)
    QMenu* machineMenu = menuBar()->addMenu("&Machine");
    
    m_videoNTSCAction = new QAction("&NTSC Video", this);
    m_videoNTSCAction->setCheckable(true);
    connect(m_videoNTSCAction, &QAction::triggered, [this]() { 
        m_videoToggle->setChecked(false); // NTSC = OFF position
        onVideoSystemToggled(false); 
    });
    machineMenu->addAction(m_videoNTSCAction);
    
    m_videoPALAction = new QAction("&PAL Video", this);
    m_videoPALAction->setCheckable(true);
    m_videoPALAction->setChecked(true); // Default
    connect(m_videoPALAction, &QAction::triggered, [this]() { 
        m_videoToggle->setChecked(true); // PAL = ON position
        onVideoSystemToggled(true); 
    });
    machineMenu->addAction(m_videoPALAction);
    
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
    
    // Increase toolbar height to accommodate multiple controls
    m_toolBar->setMinimumHeight(70);
    
    // Add spacer to push controls to the right
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spacer);
    
    // Machine type dropdown in its own column
    QWidget* machineContainer = new QWidget();
    QVBoxLayout* machineLayout = new QVBoxLayout(machineContainer);
    machineLayout->setContentsMargins(5, 5, 5, 5);
    machineLayout->setAlignment(Qt::AlignCenter);
    
    m_machineCombo = new QComboBox();
    m_machineCombo->setIconSize(QSize(32, 20)); // Set icon display size
    m_machineCombo->setMinimumWidth(150); // Slightly wider for icons
    
    // Create simple machine icons programmatically
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
    
    // Add items with custom icons
    m_machineCombo->addItem(createMachineIcon(QColor(139, 69, 19), "400"), "Atari 400/800");     // Brown
    m_machineCombo->addItem(createMachineIcon(QColor(169, 169, 169), "1200"), "Atari 1200XL");   // Dark Gray
    m_machineCombo->addItem(createMachineIcon(QColor(192, 192, 192), "XL"), "Atari 800XL");      // Silver
    m_machineCombo->addItem(createMachineIcon(QColor(105, 105, 105), "XE"), "Atari 130XE");      // Dark Gray
    m_machineCombo->addItem(createMachineIcon(QColor(85, 85, 85), "320C"), "Atari 320XE (Compy-Shop)"); // Darker Gray
    m_machineCombo->addItem(createMachineIcon(QColor(75, 75, 75), "320R"), "Atari 320XE (Rambo XL)");   // Very Dark Gray
    m_machineCombo->addItem(createMachineIcon(QColor(65, 65, 65), "576"), "Atari 576XE");        // Almost Black
    m_machineCombo->addItem(createMachineIcon(QColor(55, 55, 55), "1088"), "Atari 1088XE");      // Black
    m_machineCombo->addItem(createMachineIcon(QColor(128, 0, 128), "XEGS"), "Atari XEGS");       // Purple
    m_machineCombo->addItem(createMachineIcon(QColor(70, 130, 180), "5200"), "Atari 5200");      // Steel Blue
    
    m_machineCombo->setCurrentIndex(2); // Default to 800XL (updated index)
    connect(m_machineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onMachineTypeChanged);
    
    machineLayout->addWidget(m_machineCombo);
    
    m_toolBar->addWidget(machineContainer);
    
    // Disk drive button
    QPixmap diskPixmap(32, 32);
    diskPixmap.fill(Qt::transparent);
    QPainter diskPainter(&diskPixmap);
    diskPainter.setRenderHint(QPainter::Antialiasing);
    diskPainter.setPen(QPen(Qt::darkBlue, 2));
    diskPainter.setBrush(QBrush(QColor(100, 100, 100)));
    diskPainter.drawRoundedRect(4, 8, 24, 16, 2, 2);
    diskPainter.setBrush(QBrush(Qt::black));
    diskPainter.drawRect(6, 12, 20, 2);
    diskPainter.setPen(QPen(Qt::white, 1));
    diskPainter.drawText(QRect(8, 10, 16, 8), Qt::AlignCenter, "D1");
    QIcon diskIcon(diskPixmap);
    
    QAction* loadDiskAction = new QAction("Load Disk", this);
    loadDiskAction->setToolTip("Load disk image to drive D1: (Ctrl+D)");
    loadDiskAction->setIcon(diskIcon);
    connect(loadDiskAction, &QAction::triggered, this, &MainWindow::loadDiskImage);
    m_toolBar->addAction(loadDiskAction);
    
    // Toggle switches container - stacked vertically on far right
    QWidget* togglesContainer = new QWidget();
    QVBoxLayout* togglesLayout = new QVBoxLayout(togglesContainer);
    togglesLayout->setContentsMargins(5, 5, 5, 5);
    togglesLayout->setSpacing(4);
    togglesLayout->setAlignment(Qt::AlignCenter);
    
    // BASIC toggle switch
    QWidget* basicContainer = new QWidget();
    QHBoxLayout* basicLayout = new QHBoxLayout(basicContainer);
    basicLayout->setContentsMargins(0, 0, 0, 0);
    basicLayout->setSpacing(6);
    
    QLabel* basicLabel = new QLabel("BASIC:");
    basicLabel->setMinimumWidth(40);
    m_basicToggle = new ToggleSwitch();
    m_basicToggle->setLabels("ON", "OFF");
    m_basicToggle->setChecked(m_emulator->isBasicEnabled());
    connect(m_basicToggle, &ToggleSwitch::toggled, this, &MainWindow::toggleBasic);
    
    basicLayout->addWidget(basicLabel);
    basicLayout->addWidget(m_basicToggle);
    
    // Video system toggle switch
    QWidget* videoContainer = new QWidget();
    QHBoxLayout* videoLayout = new QHBoxLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(6);
    
    QLabel* videoLabel = new QLabel("Video:");
    videoLabel->setMinimumWidth(40);
    m_videoToggle = new ToggleSwitch();
    m_videoToggle->setLabels("PAL", "NTSC");
    m_videoToggle->setChecked(true); // Default to PAL (ON position)
    connect(m_videoToggle, &ToggleSwitch::toggled, this, &MainWindow::onVideoSystemToggled);
    
    videoLayout->addWidget(videoLabel);
    videoLayout->addWidget(m_videoToggle);
    
    // Add both toggle switches to the container
    togglesLayout->addWidget(basicContainer);
    togglesLayout->addWidget(videoContainer);
    
    m_toolBar->addWidget(togglesContainer);
    
    // Add small separator space
    QWidget* separator = new QWidget();
    separator->setFixedWidth(15);
    m_toolBar->addWidget(separator);
    
    // Create reset icons
    QPixmap resetPixmap(32, 32);
    resetPixmap.fill(Qt::transparent);
    QPainter painter(&resetPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::darkBlue, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(4, 4, 24, 24);
    painter.drawLine(16, 4, 16, 16);
    
    QIcon resetIcon;
    if (QIcon::hasThemeIcon("system-reboot")) {
        resetIcon = QIcon::fromTheme("system-reboot");
    } else if (QIcon::hasThemeIcon("view-refresh")) {
        resetIcon = QIcon::fromTheme("view-refresh");
    } else {
        resetIcon = QIcon(resetPixmap);
    }
    
    QPixmap warmPixmap(32, 32);
    warmPixmap.fill(Qt::transparent);
    QPainter warmPainter(&warmPixmap);
    warmPainter.setRenderHint(QPainter::Antialiasing);
    warmPainter.setPen(QPen(Qt::darkRed, 1.5));
    warmPainter.setBrush(Qt::NoBrush);
    warmPainter.drawEllipse(4, 4, 24, 24);
    warmPainter.drawLine(16, 8, 16, 16);
    QIcon warmIcon(warmPixmap);
    
    // Add reset buttons
    QAction* warmResetAction = new QAction("Warm Reset", this);
    warmResetAction->setToolTip("Perform a warm reset of the Atari system (F5)");
    warmResetAction->setIcon(warmIcon);
    connect(warmResetAction, &QAction::triggered, this, &MainWindow::warmBoot);
    m_toolBar->addAction(warmResetAction);
    
    QAction* coldResetAction = new QAction("Cold Reset", this);
    coldResetAction->setToolTip("Perform a cold reset of the Atari system (Shift+F5)");
    coldResetAction->setIcon(resetIcon);
    connect(coldResetAction, &QAction::triggered, this, &MainWindow::coldBoot);
    m_toolBar->addAction(coldResetAction);
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

void MainWindow::loadRom()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load Atari ROM",
        QString(),
        "ROM Files (*.rom *.bin *.car *.atr);;All Files (*)"
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

void MainWindow::loadDiskImage()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load Disk Image to D1:",
        QString(),
        "Disk Images (*.atr *.xfd *.dcm *.pro *.atx *.atr.gz *.xfd.gz);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        if (m_emulator->mountDiskImage(1, fileName, false)) {
            qDebug() << "Successfully mounted disk:" << fileName;
            statusBar()->showMessage("Mounted to D1: " + fileName + " - Try typing DIR from BASIC", 5000);
            
            // Show a helpful message to the user
            QMessageBox::information(this, "Disk Mounted", 
                "Disk image mounted to D1:\n\n" + fileName + 
                "\n\nTo access the disk:\n" +
                "• From BASIC: Type DIR or LOAD \"D1:filename\"\n" +
                "• Boot from disk: Use Cold Reset to boot from the disk\n" +
                "• DOS: Type DOS to access disk commands");
        } else {
            QMessageBox::warning(this, "Error", "Failed to mount disk image: " + fileName);
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
}

void MainWindow::toggleAltirraOS(bool enabled)
{
    m_emulator->setAltirraOSEnabled(enabled);
    QString message = enabled ? "Altirra OS enabled - restarting..." : "Original Atari OS enabled - restarting...";
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
    
    if (m_emulator->initializeWithConfig(m_emulator->isBasicEnabled(), 
                                       m_emulator->getMachineType(), 
                                       m_emulator->getVideoSystem())) {
        QString message = QString("Emulator restarted: %1 %2 with BASIC %3")
                         .arg(m_emulator->getMachineType())
                         .arg(m_emulator->getVideoSystem())
                         .arg(m_emulator->isBasicEnabled() ? "enabled" : "disabled");
        statusBar()->showMessage(message, 3000);
        qDebug() << message;
    } else {
        QMessageBox::critical(this, "Error", "Failed to restart emulator");
    }
}


void MainWindow::onVideoSystemToggled(bool isPAL)
{
    if (isPAL) {
        // PAL mode (toggle ON)
        m_videoPALAction->setChecked(true);
        m_videoNTSCAction->setChecked(false);
        m_emulator->setVideoSystem("-pal");
        statusBar()->showMessage("Video system set to PAL (49.86 fps) - restarting...", 3000);
    } else {
        // NTSC mode (toggle OFF) 
        m_videoPALAction->setChecked(false);
        m_videoNTSCAction->setChecked(true);
        m_emulator->setVideoSystem("-ntsc");
        statusBar()->showMessage("Video system set to NTSC (59.92 fps) - restarting...", 3000);
    }
    restartEmulator();
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(m_emulator, this);
    connect(&dialog, &SettingsDialog::settingsChanged, this, &MainWindow::onSettingsChanged);
    dialog.exec();
}

void MainWindow::onSettingsChanged()
{
    qDebug() << "Settings changed - updating toolbar and video settings";
    updateToolbarFromSettings();
    loadVideoSettings();
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
    
    // Update menu actions
    m_basicAction->blockSignals(true);
    m_basicAction->setChecked(m_emulator->isBasicEnabled());
    m_basicAction->blockSignals(false);
    
    m_altirraOSAction->blockSignals(true);
    m_altirraOSAction->setChecked(m_emulator->isAltirraOSEnabled());
    m_altirraOSAction->blockSignals(false);
    
    m_videoPALAction->blockSignals(true);
    m_videoPALAction->setChecked(isPAL);
    m_videoPALAction->blockSignals(false);
    
    m_videoNTSCAction->blockSignals(true);
    m_videoNTSCAction->setChecked(!isPAL);
    m_videoNTSCAction->blockSignals(false);
    
    qDebug() << "Toolbar updated - Machine:" << machineType << "BASIC:" << m_emulator->isBasicEnabled() 
             << "Video:" << m_emulator->getVideoSystem();
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

    QMessageBox::about(this, "About Fujisan", 
        QString("Fujisan - Modern Atari Emulator\n\n"
        "A modern Qt5 frontend for the Atari800 emulator.\n"
        "Features:\n"
        "• Full keyboard input support\n"
        "• BASIC enable/disable\n"
        "• Cold/warm boot options\n"
        "• Native file dialogs\n"
        "• Authentic Atari colors\n"
        "• Pixel-perfect scaling\n"
        "• 4:3 aspect ratio lock\n"
        "• Immersive fullscreen mode (%1)").arg(shortcut));
}

void MainWindow::loadInitialSettings()
{
    QSettings settings("8bitrelics", "Fujisan");
    
    // Load saved settings or use defaults
    QString machineType = settings.value("machine/type", "-xl").toString();
    QString videoSystem = settings.value("machine/videoSystem", "-pal").toString();
    bool basicEnabled = settings.value("machine/basicEnabled", true).toBool();
    bool altirraOSEnabled = settings.value("machine/altirraOS", false).toBool();
    bool audioEnabled = settings.value("audio/enabled", true).toBool();
    
    qDebug() << "Loading initial settings - Machine:" << machineType 
             << "Video:" << videoSystem << "BASIC:" << basicEnabled;
    
    // Initialize emulator with loaded settings
    if (!m_emulator->initializeWithConfig(basicEnabled, machineType, videoSystem)) {
        QMessageBox::critical(this, "Error", "Failed to initialize Atari800 emulator");
        QApplication::quit();
        return;
    }
    
    // Set additional settings
    m_emulator->setAltirraOSEnabled(altirraOSEnabled);
    m_emulator->enableAudio(audioEnabled);
    
    // Load and apply media settings (disk images, etc.)
    loadAndApplyMediaSettings();
    
    // Update toolbar to reflect loaded settings
    updateToolbarFromSettings();
}

void MainWindow::loadAndApplyMediaSettings()
{
    QSettings settings("8bitrelics", "Fujisan");
    
    qDebug() << "Loading and applying media settings...";
    
    // Load and mount disk images for D1-D4
    for (int i = 0; i < 4; i++) {
        QString diskKey = QString("media/disk%1").arg(i + 1);
        bool diskEnabled = settings.value(diskKey + "Enabled", false).toBool();
        QString diskPath = settings.value(diskKey + "Path", "").toString();
        bool diskReadOnly = settings.value(diskKey + "ReadOnly", false).toBool();
        
        if (diskEnabled && !diskPath.isEmpty()) {
            qDebug() << QString("Auto-mounting D%1: %2 (read-only: %3)")
                        .arg(i + 1).arg(diskPath).arg(diskReadOnly);
            
            if (m_emulator->mountDiskImage(i + 1, diskPath, diskReadOnly)) {
                qDebug() << QString("Successfully auto-mounted D%1:").arg(i + 1);
            } else {
                qDebug() << QString("Failed to auto-mount D%1:").arg(i + 1);
            }
        }
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Exit fullscreen if active
    if (m_isInCustomFullscreen) {
        exitCustomFullscreen();
    }
    
    if (m_emulator) {
        m_emulator->shutdown();
    }
    event->accept();
}