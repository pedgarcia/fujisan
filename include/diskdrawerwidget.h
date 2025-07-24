/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef DISKDRAWERWIDGET_H
#define DISKDRAWERWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QFrame>
#include "diskdrivewidget.h"

class AtariEmulator;

class DiskDrawerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiskDrawerWidget(AtariEmulator* emulator, QWidget* parent = nullptr);
    
    // Show/hide the drawer
    void showDrawer();
    void hideDrawer();
    bool isDrawerVisible() const { return isVisible(); }
    
    // Position the drawer relative to a target widget
    void positionRelativeTo(QWidget* targetWidget);
    
    // Get disk drive widgets
    DiskDriveWidget* getDriveWidget(int driveNumber);
    
    // Update all drives from emulator state
    void updateAllDrives();

signals:
    void drawerVisibilityChanged(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void onDiskInserted(int driveNumber, const QString& diskPath);
    void onDiskEjected(int driveNumber);  
    void onDriveStateChanged(int driveNumber, bool enabled);

private:
    void setupUI();
    void connectSignals();
    void installGlobalEventFilter();
    void removeGlobalEventFilter();
    
    AtariEmulator* m_emulator;
    
    // UI Components
    QFrame* m_containerFrame;
    QGridLayout* m_gridLayout;
    
    // Disk drive widgets (D3-D8)
    DiskDriveWidget* m_driveWidgets[6]; // D3, D4, D5, D6, D7, D8
    
    // Parent widget for positioning
    QWidget* m_targetWidget;
    
    // Event filtering
    bool m_eventFilterInstalled;
    
    // Layout constants
    static const int DRIVES_PER_ROW = 3;
    static const int DRAWER_MARGIN = 8;
    static const int DRIVE_SPACING = 4;
};

#endif // DISKDRAWERWIDGET_H