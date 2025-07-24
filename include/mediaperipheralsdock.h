/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef MEDIAPERIPHERALSDOCK_H
#define MEDIAPERIPHERALSDOCK_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include "diskdrivewidget.h"
#include "cassettewidget.h"
#include "cartridgewidget.h"

class AtariEmulator;

class MediaPeripheralsDock : public QWidget
{
    Q_OBJECT

public:
    explicit MediaPeripheralsDock(AtariEmulator* emulator, QWidget* parent = nullptr);
    
    // Get specific widgets
    DiskDriveWidget* getDriveWidget(int driveNumber);
    CassetteWidget* getCassetteWidget() { return m_cassetteWidget; }
    CartridgeWidget* getCartridgeWidget() { return m_cartridgeWidget; }
    
    // Update all devices from emulator state
    void updateAllDevices();

signals:
    void diskInserted(int driveNumber, const QString& diskPath);
    void diskEjected(int driveNumber);
    void driveStateChanged(int driveNumber, bool enabled);
    void cassetteInserted(const QString& cassettePath);
    void cassetteEjected();
    void cassetteStateChanged(bool enabled);
    void cartridgeInserted(const QString& cartridgePath);
    void cartridgeEjected();

private slots:
    void onDiskInserted(int driveNumber, const QString& diskPath);
    void onDiskEjected(int driveNumber);
    void onDriveStateChanged(int driveNumber, bool enabled);
    void onCassetteInserted(const QString& cassettePath);
    void onCassetteEjected();
    void onCassetteStateChanged(bool enabled);
    void onCartridgeInserted(const QString& cartridgePath);
    void onCartridgeEjected();

private:
    void setupUI();
    void createCartridgeSection();
    void createCassetteSection();
    void createDiskDrivesSection();
    void createPrinterSection();
    void connectSignals();
    
    AtariEmulator* m_emulator;
    
    // Main layout
    QVBoxLayout* m_mainLayout;
    
    // Device sections
    QGroupBox* m_cartridgeGroup;
    QGroupBox* m_cassetteGroup;
    QGroupBox* m_diskDrivesGroup;
    QGroupBox* m_printerGroup;
    
    // Device widgets
    CartridgeWidget* m_cartridgeWidget;
    CassetteWidget* m_cassetteWidget;
    DiskDriveWidget* m_driveWidgets[7]; // D2-D8 (index 0 = D2, index 6 = D8)
    QLabel* m_printerWidget; // Placeholder for now
    
    // Layout constants
    static const int SECTION_SPACING = 10;
    static const int WIDGET_SPACING = 5;
};

#endif // MEDIAPERIPHERALSDOCK_H