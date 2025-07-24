/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef DISKDRIVEWIDGET_H
#define DISKDRIVEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QString>

class AtariEmulator;

class DiskDriveWidget : public QWidget
{
    Q_OBJECT

public:
    enum DriveState {
        Off,        // Drive is off (atari810off.png)
        Empty,      // Drive is on but empty (atari810emtpy.png) 
        Closed,     // Drive has disk inserted (atari810closed.png)
        Reading,    // Drive is reading (atari810read.png)
        Writing     // Drive is writing (atari810write.png)
    };

    explicit DiskDriveWidget(int driveNumber, AtariEmulator* emulator, QWidget *parent = nullptr, bool isDrawerDrive = false);
    
    // State management
    void setState(DriveState state);
    DriveState getState() const { return m_currentState; }
    
    // Disk operations
    void insertDisk(const QString& diskPath);
    void ejectDisk();
    void setDriveEnabled(bool enabled);
    bool isDriveEnabled() const { return m_driveEnabled; }
    
    // Disk info
    QString getDiskPath() const { return m_diskPath; }
    bool hasDisk() const { return !m_diskPath.isEmpty(); }
    
    // Visual feedback for I/O operations
    void showReadActivity();  // Blinking (legacy)
    void showWriteActivity(); // Blinking (legacy)
    
    // Solid LED control (realistic)
    void turnOnReadLED();
    void turnOnWriteLED();
    void turnOffActivityLED();
    
    // Size management
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void updateFromEmulator();

signals:
    void diskInserted(int driveNumber, const QString& diskPath);
    void diskEjected(int driveNumber);
    void driveStateChanged(int driveNumber, bool enabled);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    
private slots:
    void onToggleDrive();
    void onInsertDisk();
    void onEjectDisk();
    void onBlinkTimer();
    void onLedDebounceTimeout();
    
private:
    void setupUI();
    void loadImages();
    void updateDisplay();
    void createContextMenu();
    void updateTooltip();
    void startBlinking(DriveState blinkState);
    void stopBlinking();
    bool isValidDiskFile(const QString& fileName) const;
    
    // Properties
    int m_driveNumber;
    AtariEmulator* m_emulator;
    DriveState m_currentState;
    DriveState m_baseState;  // State to return to when blinking stops
    bool m_driveEnabled;
    QString m_diskPath;
    bool m_isDrawerDrive;  // True for D3-D8 drives (larger size)
    
    // UI Components
    QLabel* m_imageLabel;
    QMenu* m_contextMenu;
    QAction* m_toggleAction;
    QAction* m_insertAction;
    QAction* m_ejectAction;
    
    // Images for different states
    QPixmap m_offImage;
    QPixmap m_emptyImage;
    QPixmap m_closedImage;
    QPixmap m_readImage;
    QPixmap m_writeImage;
    
    // Blinking animation
    QTimer* m_blinkTimer;
    bool m_blinkVisible;
    DriveState m_blinkState;
    
    // LED debounce mechanism
    QTimer* m_ledDebounceTimer;
    DriveState m_pendingOffState;
    
    // Constants
    static const int DISK_WIDTH = 94;  // 30% wider than previous (72 * 1.3)
    static const int DISK_HEIGHT = 62; // 30% wider than previous (48 * 1.3)
    static const int BLINK_INTERVAL = 150; // ms
};

#endif // DISKDRIVEWIDGET_H