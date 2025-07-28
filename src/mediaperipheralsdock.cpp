/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "mediaperipheralsdock.h"
#include "atariemulator.h"
#include <QPushButton>
#include <QStyle>

MediaPeripheralsDock::MediaPeripheralsDock(AtariEmulator* emulator, QWidget* parent)
    : QWidget(parent)
    , m_emulator(emulator)
    , m_mainLayout(nullptr)
    , m_cartridgeGroup(nullptr)
    , m_cassetteGroup(nullptr)
    , m_diskDrivesGroup(nullptr)
    , m_printerGroup(nullptr)
    , m_driveContainer(nullptr)
    , m_driveButtonsLayout(nullptr)
    , m_addDriveButton(nullptr)
    , m_removeDriveButton(nullptr)
    , m_cartridgeWidget(nullptr)
    , m_cassetteWidget(nullptr)
    , m_printerWidget(nullptr)
{
    // Initialize drive widgets array
    for (int i = 0; i < 7; i++) {
        m_driveWidgets[i] = nullptr;
    }
    
    // Start with 3 visible drives (D2-D4)
    m_visibleDrives = 3;
    
    setupUI();
    connectSignals();
    updateAllDevices();
}

void MediaPeripheralsDock::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(SECTION_SPACING);
    
    // New order: Drives first, then Cassette, then Printer (Cartridge moved to toolbar)
    createDiskDrivesSection();
    // createCartridgeSection(); // Cartridge moved to main toolbar
    createCassetteSection();
    createPrinterSection();
    
    // Add stretch to push everything to the top
    m_mainLayout->addStretch();
}

void MediaPeripheralsDock::createCartridgeSection()
{
    // Cartridge section moved to main toolbar
    m_cartridgeWidget = nullptr; // Cartridge is now on main toolbar
    m_cartridgeGroup = nullptr;  // No cartridge group in dock anymore
}

void MediaPeripheralsDock::createCassetteSection()
{
    m_cassetteGroup = new QGroupBox("Cassette Recorder", this);
    QVBoxLayout* cassetteLayout = new QVBoxLayout(m_cassetteGroup);
    cassetteLayout->setContentsMargins(WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING);
    
    m_cassetteWidget = new CassetteWidget(m_emulator, this);
    cassetteLayout->addWidget(m_cassetteWidget, 0, Qt::AlignCenter);
    
    m_mainLayout->addWidget(m_cassetteGroup);
}

void MediaPeripheralsDock::createDiskDrivesSection()
{
    m_diskDrivesGroup = new QGroupBox("Disk Drives", this);
    QVBoxLayout* diskGroupLayout = new QVBoxLayout(m_diskDrivesGroup);
    diskGroupLayout->setContentsMargins(WIDGET_SPACING, 2, WIDGET_SPACING, WIDGET_SPACING); // Reduced top margin
    diskGroupLayout->setSpacing(WIDGET_SPACING);
    
    // Create container for drives
    m_driveContainer = new QWidget();
    QVBoxLayout* driveLayout = new QVBoxLayout(m_driveContainer);
    driveLayout->setContentsMargins(0, 0, 0, 0);
    driveLayout->setSpacing(WIDGET_SPACING);
    
    // Create all drives D2-D8
    for (int i = 0; i < 7; i++) {
        int driveNumber = i + 2; // D2-D8
        
        m_driveWidgets[i] = new DiskDriveWidget(driveNumber, m_emulator, this, true); // true for drawer drives
        
        // Create a container for better alignment
        QWidget* driveContainer = new QWidget();
        QHBoxLayout* driveContainerLayout = new QHBoxLayout(driveContainer);
        driveContainerLayout->setContentsMargins(0, 0, 0, 0);
        driveContainerLayout->addWidget(m_driveWidgets[i], 0, Qt::AlignCenter);
        
        driveLayout->addWidget(driveContainer);
        
        // Initially hide drives D5-D8 (indices 3-6)
        if (i >= 3) {
            driveContainer->setVisible(false);
        }
    }
    
    diskGroupLayout->addWidget(m_driveContainer);
    
    // Create +/- buttons
    QWidget* buttonContainer = new QWidget();
    m_driveButtonsLayout = new QHBoxLayout(buttonContainer);
    m_driveButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_driveButtonsLayout->setSpacing(5);
    
    m_addDriveButton = new QPushButton("+", this);
    m_addDriveButton->setFixedSize(32, 24);
    m_addDriveButton->setStyleSheet("font-size: 14px; font-weight: bold;");
    m_addDriveButton->setToolTip("Add drive");
    
    m_removeDriveButton = new QPushButton("−", this);
    m_removeDriveButton->setFixedSize(32, 24);
    m_removeDriveButton->setStyleSheet("font-size: 14px; font-weight: bold;");
    m_removeDriveButton->setToolTip("Remove drive");
    m_removeDriveButton->setEnabled(false); // Initially disabled
    
    m_driveButtonsLayout->addStretch();
    m_driveButtonsLayout->addWidget(m_addDriveButton);
    m_driveButtonsLayout->addWidget(m_removeDriveButton);
    m_driveButtonsLayout->addStretch();
    
    diskGroupLayout->addWidget(buttonContainer);
    
    // Connect button signals
    connect(m_addDriveButton, &QPushButton::clicked, this, &MediaPeripheralsDock::onAddDrive);
    connect(m_removeDriveButton, &QPushButton::clicked, this, &MediaPeripheralsDock::onRemoveDrive);
    
    m_mainLayout->addWidget(m_diskDrivesGroup);
}

void MediaPeripheralsDock::createPrinterSection()
{
    // Printer widget - DISABLED (P: device not working in atari800 core)
    // TODO: Re-enable when P: device emulation is fixed
    /*
    m_printerGroup = new QGroupBox("Printer (P:)", this);
    QVBoxLayout* printerLayout = new QVBoxLayout(m_printerGroup);
    printerLayout->setContentsMargins(2, WIDGET_SPACING, 2, WIDGET_SPACING); // Reduced left/right margins
    
    // Create printer widget
    m_printerWidget = new PrinterWidget(this);
    printerLayout->addWidget(m_printerWidget);
    
    m_mainLayout->addWidget(m_printerGroup);
    */
}

void MediaPeripheralsDock::connectSignals()
{
    // Cartridge signals now handled by main toolbar cartridge widget
    // connect(m_cartridgeWidget, &CartridgeWidget::cartridgeInserted,
    //         this, &MediaPeripheralsDock::onCartridgeInserted);
    // connect(m_cartridgeWidget, &CartridgeWidget::cartridgeEjected,
    //         this, &MediaPeripheralsDock::onCartridgeEjected);
    
    // Connect cassette signals
    connect(m_cassetteWidget, &CassetteWidget::cassetteInserted,
            this, &MediaPeripheralsDock::onCassetteInserted);
    connect(m_cassetteWidget, &CassetteWidget::cassetteEjected,
            this, &MediaPeripheralsDock::onCassetteEjected);
    connect(m_cassetteWidget, &CassetteWidget::cassetteStateChanged,
            this, &MediaPeripheralsDock::onCassetteStateChanged);
    
    // Connect printer signals - DISABLED
    /*
    connect(m_printerWidget, &PrinterWidget::printerEnabledChanged,
            this, &MediaPeripheralsDock::onPrinterEnabledChanged);
    connect(m_printerWidget, &PrinterWidget::outputFormatChanged,
            this, &MediaPeripheralsDock::onPrinterOutputFormatChanged);
    connect(m_printerWidget, &PrinterWidget::printerTypeChanged,
            this, &MediaPeripheralsDock::onPrinterTypeChanged);
    */
    
    // Connect disk drive signals
    for (int i = 0; i < 7; i++) {
        if (m_driveWidgets[i]) {
            connect(m_driveWidgets[i], &DiskDriveWidget::diskInserted,
                    this, &MediaPeripheralsDock::onDiskInserted);
            connect(m_driveWidgets[i], &DiskDriveWidget::diskEjected,
                    this, &MediaPeripheralsDock::onDiskEjected);
            connect(m_driveWidgets[i], &DiskDriveWidget::driveStateChanged,
                    this, &MediaPeripheralsDock::onDriveStateChanged);
        }
    }
}

DiskDriveWidget* MediaPeripheralsDock::getDriveWidget(int driveNumber)
{
    if (driveNumber >= 2 && driveNumber <= 8) {
        int index = driveNumber - 2; // Convert D2-D8 to index 0-6
        return m_driveWidgets[index];
    }
    return nullptr;
}

void MediaPeripheralsDock::updateAllDevices()
{
    // Update cartridge
    if (m_cartridgeWidget) {
        m_cartridgeWidget->updateFromEmulator();
    }
    
    // Update cassette
    if (m_cassetteWidget) {
        m_cassetteWidget->updateFromEmulator();
    }
    
    // Update printer - DISABLED
    /*
    if (m_printerWidget) {
        m_printerWidget->loadSettings();
    }
    */
    
    // Update disk drives
    for (int i = 0; i < 7; i++) {
        if (m_driveWidgets[i]) {
            m_driveWidgets[i]->updateFromEmulator();
        }
    }
}

void MediaPeripheralsDock::onDiskInserted(int driveNumber, const QString& diskPath)
{
    emit diskInserted(driveNumber, diskPath);
}

void MediaPeripheralsDock::onDiskEjected(int driveNumber)
{
    emit diskEjected(driveNumber);
}

void MediaPeripheralsDock::onDriveStateChanged(int driveNumber, bool enabled)
{
    emit driveStateChanged(driveNumber, enabled);
}

void MediaPeripheralsDock::onCassetteInserted(const QString& cassettePath)
{
    emit cassetteInserted(cassettePath);
}

void MediaPeripheralsDock::onCassetteEjected()
{
    emit cassetteEjected();
}

void MediaPeripheralsDock::onCassetteStateChanged(bool enabled)
{
    emit cassetteStateChanged(enabled);
}

void MediaPeripheralsDock::onCartridgeInserted(const QString& cartridgePath)
{
    emit cartridgeInserted(cartridgePath);
}

void MediaPeripheralsDock::onCartridgeEjected()
{
    emit cartridgeEjected();
}

void MediaPeripheralsDock::onPrinterEnabledChanged(bool enabled)
{
    emit printerEnabledChanged(enabled);
}

void MediaPeripheralsDock::onPrinterOutputFormatChanged(const QString& format)
{
    emit printerOutputFormatChanged(format);
}

void MediaPeripheralsDock::onPrinterTypeChanged(const QString& type)
{
    emit printerTypeChanged(type);
}

void MediaPeripheralsDock::onAddDrive()
{
    if (m_visibleDrives < 7) {
        // Show the next drive (D5-D8)
        int driveIndex = m_visibleDrives; // Current number of visible drives = index of next drive
        if (m_driveWidgets[driveIndex]) {
            m_driveWidgets[driveIndex]->parentWidget()->setVisible(true);
            m_visibleDrives++;
            updateDriveButtonStates();
        }
    }
}

void MediaPeripheralsDock::onRemoveDrive()
{
    if (m_visibleDrives > 3) {
        // Hide the last drive (D8 down to D5)
        int driveIndex = m_visibleDrives - 1; // Index of last visible drive
        if (m_driveWidgets[driveIndex]) {
            m_driveWidgets[driveIndex]->parentWidget()->setVisible(false);
            m_visibleDrives--;
            updateDriveButtonStates();
        }
    }
}

void MediaPeripheralsDock::updateDriveButtonStates()
{
    // Enable/disable buttons based on current state
    m_addDriveButton->setEnabled(m_visibleDrives < 7);    // Can add if less than 7 drives visible
    m_removeDriveButton->setEnabled(m_visibleDrives > 3); // Can remove if more than 3 drives visible
}