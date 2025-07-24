/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "mediaperipheralsdock.h"
#include "atariemulator.h"
#include <QDebug>

MediaPeripheralsDock::MediaPeripheralsDock(AtariEmulator* emulator, QWidget* parent)
    : QWidget(parent)
    , m_emulator(emulator)
    , m_mainLayout(nullptr)
    , m_cartridgeGroup(nullptr)
    , m_cassetteGroup(nullptr)
    , m_diskDrivesGroup(nullptr)
    , m_printerGroup(nullptr)
    , m_cartridgeWidget(nullptr)
    , m_cassetteWidget(nullptr)
    , m_printerWidget(nullptr)
{
    // Initialize drive widgets array
    for (int i = 0; i < 7; i++) {
        m_driveWidgets[i] = nullptr;
    }
    
    setupUI();
    connectSignals();
    updateAllDevices();
}

void MediaPeripheralsDock::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(SECTION_SPACING);
    
    // New order: Drives first, then Cartridge, then Cassette, then Printer
    createDiskDrivesSection();
    createCartridgeSection();
    createCassetteSection();
    createPrinterSection();
    
    // Add stretch to push everything to the top
    m_mainLayout->addStretch();
}

void MediaPeripheralsDock::createCartridgeSection()
{
    m_cartridgeGroup = new QGroupBox("Cartridge", this);
    QVBoxLayout* cartridgeLayout = new QVBoxLayout(m_cartridgeGroup);
    cartridgeLayout->setContentsMargins(WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING);
    
    m_cartridgeWidget = new CartridgeWidget(m_emulator, this);
    cartridgeLayout->addWidget(m_cartridgeWidget, 0, Qt::AlignCenter);
    
    m_mainLayout->addWidget(m_cartridgeGroup);
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
    QVBoxLayout* diskLayout = new QVBoxLayout(m_diskDrivesGroup);
    diskLayout->setContentsMargins(WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING);
    diskLayout->setSpacing(WIDGET_SPACING);
    
    // Create drives D2-D8 vertically stacked for narrower dock
    for (int i = 0; i < 7; i++) {
        int driveNumber = i + 2; // D2-D8
        
        m_driveWidgets[i] = new DiskDriveWidget(driveNumber, m_emulator, this, true); // true for drawer drives
        
        // Create a container for better alignment
        QWidget* driveContainer = new QWidget();
        QHBoxLayout* driveContainerLayout = new QHBoxLayout(driveContainer);
        driveContainerLayout->setContentsMargins(0, 0, 0, 0);
        driveContainerLayout->addWidget(m_driveWidgets[i], 0, Qt::AlignCenter);
        
        diskLayout->addWidget(driveContainer);
    }
    
    m_mainLayout->addWidget(m_diskDrivesGroup);
}

void MediaPeripheralsDock::createPrinterSection()
{
    m_printerGroup = new QGroupBox("Printer", this);
    QVBoxLayout* printerLayout = new QVBoxLayout(m_printerGroup);
    printerLayout->setContentsMargins(WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING, WIDGET_SPACING);
    
    // Placeholder for printer widget
    m_printerWidget = new QLabel("Printer: Not Implemented", this);
    m_printerWidget->setAlignment(Qt::AlignCenter);
    m_printerWidget->setStyleSheet("color: gray; font-style: italic;");
    printerLayout->addWidget(m_printerWidget);
    
    m_mainLayout->addWidget(m_printerGroup);
}

void MediaPeripheralsDock::connectSignals()
{
    // Connect cartridge signals
    connect(m_cartridgeWidget, &CartridgeWidget::cartridgeInserted,
            this, &MediaPeripheralsDock::onCartridgeInserted);
    connect(m_cartridgeWidget, &CartridgeWidget::cartridgeEjected,
            this, &MediaPeripheralsDock::onCartridgeEjected);
    
    // Connect cassette signals
    connect(m_cassetteWidget, &CassetteWidget::cassetteInserted,
            this, &MediaPeripheralsDock::onCassetteInserted);
    connect(m_cassetteWidget, &CassetteWidget::cassetteEjected,
            this, &MediaPeripheralsDock::onCassetteEjected);
    connect(m_cassetteWidget, &CassetteWidget::cassetteStateChanged,
            this, &MediaPeripheralsDock::onCassetteStateChanged);
    
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
    
    // Update disk drives
    for (int i = 0; i < 7; i++) {
        if (m_driveWidgets[i]) {
            m_driveWidgets[i]->updateFromEmulator();
        }
    }
}

void MediaPeripheralsDock::onDiskInserted(int driveNumber, const QString& diskPath)
{
    qDebug() << "Media dock: Disk inserted in drive" << driveNumber << ":" << diskPath;
    emit diskInserted(driveNumber, diskPath);
}

void MediaPeripheralsDock::onDiskEjected(int driveNumber)
{
    qDebug() << "Media dock: Disk ejected from drive" << driveNumber;
    emit diskEjected(driveNumber);
}

void MediaPeripheralsDock::onDriveStateChanged(int driveNumber, bool enabled)
{
    qDebug() << "Media dock: Drive" << driveNumber << "state changed to" << (enabled ? "on" : "off");
    emit driveStateChanged(driveNumber, enabled);
}

void MediaPeripheralsDock::onCassetteInserted(const QString& cassettePath)
{
    qDebug() << "Media dock: Cassette inserted:" << cassettePath;
    emit cassetteInserted(cassettePath);
}

void MediaPeripheralsDock::onCassetteEjected()
{
    qDebug() << "Media dock: Cassette ejected";
    emit cassetteEjected();
}

void MediaPeripheralsDock::onCassetteStateChanged(bool enabled)
{
    qDebug() << "Media dock: Cassette state changed to" << (enabled ? "on" : "off");
    emit cassetteStateChanged(enabled);
}

void MediaPeripheralsDock::onCartridgeInserted(const QString& cartridgePath)
{
    qDebug() << "Media dock: Cartridge inserted:" << cartridgePath;
    emit cartridgeInserted(cartridgePath);
}

void MediaPeripheralsDock::onCartridgeEjected()
{
    qDebug() << "Media dock: Cartridge ejected";
    emit cartridgeEjected();
}