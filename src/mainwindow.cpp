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
#include <QStatusBar>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_emulator(new AtariEmulator(this))
    , m_emulatorWidget(nullptr)
{
    setWindowTitle("Fujisan");
    setMinimumSize(800, 600);
    resize(1280, 960);
    
    createMenus();
    createToolBar();
    createEmulatorWidget();
    
    // Initialize the emulator
    if (!m_emulator->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize Atari800 emulator");
        QApplication::quit();
        return;
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
    
    m_loadRomAction = new QAction("&Load ROM...", this);
    m_loadRomAction->setShortcut(QKeySequence::Open);
    connect(m_loadRomAction, &QAction::triggered, this, &MainWindow::loadRom);
    fileMenu->addAction(m_loadRomAction);
    
    fileMenu->addSeparator();
    
    m_coldBootAction = new QAction("&Cold Boot", this);
    connect(m_coldBootAction, &QAction::triggered, this, &MainWindow::coldBoot);
    fileMenu->addAction(m_coldBootAction);
    
    m_warmBootAction = new QAction("&Warm Boot", this);
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
    
    m_autoRestartAction = new QAction("Auto-&restart on changes", this);
    m_autoRestartAction->setCheckable(true);
    m_autoRestartAction->setChecked(m_autoRestart);
    connect(m_autoRestartAction, &QAction::toggled, this, &MainWindow::toggleAutoRestart);
    systemMenu->addAction(m_autoRestartAction);
    
    systemMenu->addSeparator();
    
    m_restartAction = new QAction("&Restart with Current Settings", this);
    connect(m_restartAction, &QAction::triggered, this, &MainWindow::restartEmulator);
    systemMenu->addAction(m_restartAction);
    
    // Machine menu
    QMenu* machineMenu = menuBar()->addMenu("&Machine");
    
    m_machine800Action = new QAction("Atari &400/800", this);
    m_machine800Action->setCheckable(true);
    connect(m_machine800Action, &QAction::triggered, this, &MainWindow::setMachine800);
    machineMenu->addAction(m_machine800Action);
    
    m_machineXLAction = new QAction("Atari 800&XL", this);
    m_machineXLAction->setCheckable(true);
    m_machineXLAction->setChecked(true); // Default
    connect(m_machineXLAction, &QAction::triggered, this, &MainWindow::setMachineXL);
    machineMenu->addAction(m_machineXLAction);
    
    m_machineXEAction = new QAction("Atari 130X&E", this);
    m_machineXEAction->setCheckable(true);
    connect(m_machineXEAction, &QAction::triggered, this, &MainWindow::setMachineXE);
    machineMenu->addAction(m_machineXEAction);
    
    m_machine5200Action = new QAction("Atari &5200", this);
    m_machine5200Action->setCheckable(true);
    connect(m_machine5200Action, &QAction::triggered, this, &MainWindow::setMachine5200);
    machineMenu->addAction(m_machine5200Action);
    
    machineMenu->addSeparator();
    
    m_videoNTSCAction = new QAction("&NTSC Video", this);
    m_videoNTSCAction->setCheckable(true);
    connect(m_videoNTSCAction, &QAction::triggered, this, &MainWindow::setVideoNTSC);
    machineMenu->addAction(m_videoNTSCAction);
    
    m_videoPALAction = new QAction("&PAL Video", this);
    m_videoPALAction->setCheckable(true);
    m_videoPALAction->setChecked(true); // Default
    connect(m_videoPALAction, &QAction::triggered, this, &MainWindow::setVideoPAL);
    machineMenu->addAction(m_videoPALAction);
    
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
    
    // Add Cold Reset button
    QAction* coldResetAction = new QAction("Cold Reset", this);
    coldResetAction->setToolTip("Perform a cold reset of the Atari system");
    coldResetAction->setIcon(QIcon::fromTheme("view-refresh")); // Use system refresh icon
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
    QString message = enabled ? "BASIC enabled" : "BASIC disabled";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::toggleAltirraOS(bool enabled)
{
    m_emulator->setAltirraOSEnabled(enabled);
    QString message = enabled ? "Altirra OS enabled" : "Original Atari OS enabled";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::toggleAutoRestart(bool enabled)
{
    m_autoRestart = enabled;
    QString message = enabled ? "Auto-restart enabled" : "Auto-restart disabled";
    statusBar()->showMessage(message, 2000);
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

void MainWindow::setMachine800()
{
    // Uncheck other machine options
    m_machineXLAction->setChecked(false);
    m_machineXEAction->setChecked(false);
    m_machine5200Action->setChecked(false);
    
    m_emulator->setMachineType("-atari");
    QString message = "Machine set to Atari 400/800";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::setMachineXL()
{
    // Uncheck other machine options
    m_machine800Action->setChecked(false);
    m_machineXEAction->setChecked(false);
    m_machine5200Action->setChecked(false);
    
    m_emulator->setMachineType("-xl");
    QString message = "Machine set to Atari 800XL";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::setMachineXE()
{
    // Uncheck other machine options
    m_machine800Action->setChecked(false);
    m_machineXLAction->setChecked(false);
    m_machine5200Action->setChecked(false);
    
    m_emulator->setMachineType("-xe");
    QString message = "Machine set to Atari 130XE";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::setMachine5200()
{
    // Uncheck other machine options
    m_machine800Action->setChecked(false);
    m_machineXLAction->setChecked(false);
    m_machineXEAction->setChecked(false);
    
    m_emulator->setMachineType("-5200");
    QString message = "Machine set to Atari 5200";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::setVideoNTSC()
{
    // Uncheck PAL option
    m_videoPALAction->setChecked(false);
    
    m_emulator->setVideoSystem("-ntsc");
    QString message = "Video system set to NTSC (59.92 fps)";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::setVideoPAL()
{
    // Uncheck NTSC option
    m_videoNTSCAction->setChecked(false);
    
    m_emulator->setVideoSystem("-pal");
    QString message = "Video system set to PAL (49.86 fps)";
    if (m_autoRestart) {
        message += " - restarting...";
        statusBar()->showMessage(message, 3000);
        restartEmulator();
    } else {
        message += " - restart to apply";
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Fujisan", 
        "Fujisan - Modern Atari Emulator\n\n"
        "A modern Qt5 frontend for the Atari800 emulator.\n"
        "Features:\n"
        "• Full keyboard input support\n"
        "• BASIC enable/disable\n"
        "• Cold/warm boot options\n"
        "• Native file dialogs\n"
        "• Authentic Atari colors\n"
        "• Pixel-perfect scaling");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_emulator) {
        m_emulator->shutdown();
    }
    event->accept();
}