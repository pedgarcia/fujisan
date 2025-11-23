/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "diskdrivewidget.h"
#include "atariemulator.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QSettings>
#include <QMimeData>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>

DiskDriveWidget::DiskDriveWidget(int driveNumber, AtariEmulator* emulator, QWidget *parent, bool isDrawerDrive)
    : QWidget(parent)
    , m_driveNumber(driveNumber)
    , m_emulator(emulator)
    , m_currentState(Off)
    , m_baseState(Off)
    , m_driveEnabled(false)
    , m_imageLabel(nullptr)
    , m_progressLabel(nullptr)
    , m_contextMenu(nullptr)
    , m_blinkTimer(new QTimer(this))
    , m_blinkVisible(true)
    , m_blinkState(Off)
    , m_ledDebounceTimer(new QTimer(this))
    , m_pendingOffState(Off)
    , m_isDrawerDrive(isDrawerDrive)
    , m_driveMode(LOCAL)
    , m_showingCopyProgress(false)
{
    setupUI();
    loadImages();
    createContextMenu();

    // Enable drag and drop
    setAcceptDrops(true);

    // Setup blinking timer
    m_blinkTimer->setInterval(BLINK_INTERVAL);
    connect(m_blinkTimer, &QTimer::timeout, this, &DiskDriveWidget::onBlinkTimer);

    // Setup LED debounce timer (750ms delay)
    m_ledDebounceTimer->setSingleShot(true);
    m_ledDebounceTimer->setInterval(750);
    connect(m_ledDebounceTimer, &QTimer::timeout, this, &DiskDriveWidget::onLedDebounceTimeout);

    // Initial state - explicitly set drive as disabled (off) and force display update
    m_driveEnabled = false;
    setState(Off);
    updateDisplay(); // Force initial display update
    updateFromEmulator();
}

void DiskDriveWidget::setupUI()
{
    // D1 drive (toolbar) is 20% smaller than dock drives for better fit
    int width, height;
    int margin;
    if (m_isDrawerDrive) {
        // Dock drives (D2-D8) - full size but slightly shorter
        width = DISK_WIDTH * 0.85;
        height = DISK_HEIGHT * 0.84; // Dock drives slightly shorter
        margin = 0; // No extra margin for dock drives
    } else {
        // Toolbar drive (D1) - 20% smaller image + 5px margin on all sides
        int imageWidth = DISK_WIDTH * 0.8;    // 75px image width
        int imageHeight = DISK_HEIGHT * 0.8;  // 50px image height
        margin = 5; // 5px margin around the image
        width = imageWidth + (margin * 2);    // 85px total width
        height = imageHeight + (margin * 2);  // 60px total height
    }

    setFixedSize(width, height);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(0);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(true);
    m_imageLabel->setContentsMargins(0, 0, 0, 0);  // No margins on the image label
    layout->addWidget(m_imageLabel);

    // Create progress label for file copy operations (hidden by default)
    m_progressLabel = new QLabel(this);
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->setText("Copying...");
    m_progressLabel->setStyleSheet("background-color: rgba(0, 0, 0, 150); color: white; font-weight: bold;");
    m_progressLabel->setVisible(false);
    m_progressLabel->raise();  // Ensure it's on top

}

void DiskDriveWidget::loadImages()
{
    // Try multiple relative paths to find the images
    QStringList imagePaths = {
        "./images/",
        "../images/",
        QApplication::applicationDirPath() + "/images/",
        QApplication::applicationDirPath() + "/../images/",
#ifdef Q_OS_MAC
        QApplication::applicationDirPath() + "/../Resources/images/",
#endif
#ifdef Q_OS_LINUX
        "/usr/share/fujisan/images/",
        QApplication::applicationDirPath() + "/../share/images/",
#endif
        ":/images/"
    };

    for (const QString& path : imagePaths) {
        QString offPath = path + "atari810off.png";
        if (QFileInfo::exists(offPath)) {
            bool success = true;
            success &= m_offImage.load(offPath);
            success &= m_emptyImage.load(path + "atari810emtpy.png");  // Note: keeping original filename with typo
            success &= m_closedImage.load(path + "atari810closed.png");
            success &= m_readImage.load(path + "atari810read.png");
            success &= m_writeImage.load(path + "atari810write.png");

            if (success) {
                return;
            } else {
            }
        }
    }


    // Create fallback placeholder images if loading fails
    if (m_offImage.isNull()) {
        m_offImage = QPixmap(72, 48);
        m_offImage.fill(Qt::gray);
    }
}

void DiskDriveWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &DiskDriveWidget::onToggleDrive);
    m_contextMenu->addAction(m_toggleAction);

    m_contextMenu->addSeparator();

    m_insertAction = new QAction("Insert Disk Image...", this);
    connect(m_insertAction, &QAction::triggered, this, &DiskDriveWidget::onInsertDisk);
    m_contextMenu->addAction(m_insertAction);

    m_ejectAction = new QAction("Eject", this);
    connect(m_ejectAction, &QAction::triggered, this, &DiskDriveWidget::onEjectDisk);
    m_contextMenu->addAction(m_ejectAction);
}

void DiskDriveWidget::setState(DriveState state)
{
    if (m_currentState != state) {
        m_currentState = state;
        m_baseState = state;
        stopBlinking();
        updateDisplay();
        updateTooltip();
    }
}

void DiskDriveWidget::insertDisk(const QString& diskPath)
{
    if (m_driveMode == FUJINET) {
        // In FujiNet mode, just emit the signal
        // MainWindow will handle copying to SD and calling FujiNet API
        emit diskInserted(m_driveNumber, diskPath);
    } else {
        // Local mode - mount to emulator
        if (m_emulator && m_emulator->mountDiskImage(m_driveNumber, diskPath, false)) {
            m_diskPath = diskPath;
            setState(m_driveEnabled ? Closed : Off);
            updateTooltip();
            emit diskInserted(m_driveNumber, diskPath);
        }
    }
}

void DiskDriveWidget::ejectDisk()
{
    if (m_driveMode == FUJINET) {
        // In FujiNet mode, just emit the signal
        // MainWindow will handle calling FujiNet API
        emit diskEjected(m_driveNumber);
    } else {
        // Local mode - dismount from emulator
        if (m_emulator && hasDisk()) {
            // Properly dismount disk from emulator
            m_emulator->dismountDiskImage(m_driveNumber);
            m_diskPath.clear();
            setState(m_driveEnabled ? Empty : Off);
            updateTooltip();
            emit diskEjected(m_driveNumber);
        }
    }
}

void DiskDriveWidget::setDriveEnabled(bool enabled)
{
    if (m_driveEnabled != enabled) {
        m_driveEnabled = enabled;

        // Update state based on whether we have a disk or not
        if (enabled) {
            setState(hasDisk() ? Closed : Empty);
        } else {
            setState(Off);
        }

        emit driveStateChanged(m_driveNumber, enabled);
    }
}

void DiskDriveWidget::showReadActivity()
{
    if (m_currentState == Closed) {
        startBlinking(Reading);
    }
}

void DiskDriveWidget::showWriteActivity()
{
    if (m_currentState == Closed) {
        startBlinking(Writing);
    }
}

void DiskDriveWidget::turnOnReadLED()
{
    if (m_currentState == Closed || m_currentState == Empty || m_currentState == Writing) {
        // Cancel any pending LED off timer
        m_ledDebounceTimer->stop();

        stopBlinking();
        m_currentState = Reading;
        updateDisplay();
    }
}

void DiskDriveWidget::turnOnWriteLED()
{
    if (m_currentState == Closed || m_currentState == Empty || m_currentState == Reading) {
        // Cancel any pending LED off timer
        m_ledDebounceTimer->stop();

        stopBlinking();
        m_currentState = Writing;
        updateDisplay();
    }
}

void DiskDriveWidget::turnOffActivityLED()
{
    if (m_currentState == Reading || m_currentState == Writing) {
        // Don't turn off immediately - start debounce timer
        m_pendingOffState = m_baseState; // Remember what state to return to
        m_ledDebounceTimer->start();
    }
}

void DiskDriveWidget::updateFromEmulator()
{
    if (!m_emulator) return;

    // Get current disk image path from emulator
    QString currentPath = m_emulator->getDiskImagePath(m_driveNumber);

    if (!currentPath.isEmpty() && currentPath != m_diskPath) {
        m_diskPath = currentPath;
        if (m_driveEnabled) {
            setState(Closed);
        }
    } else if (currentPath.isEmpty() && !m_diskPath.isEmpty()) {
        m_diskPath.clear();
        setState(m_driveEnabled ? Empty : Off);
    }

    updateTooltip();
}

void DiskDriveWidget::updateDisplay()
{
    if (!m_imageLabel) return;

    QPixmap currentImage;
    QString imageName;
    switch (m_currentState) {
        case Off:
            currentImage = m_offImage;
            imageName = "atari810off.png";
            break;
        case Empty:
            currentImage = m_emptyImage;
            imageName = "atari810emtpy.png";
            break;
        case Closed:
            currentImage = m_closedImage;
            imageName = "atari810closed.png";
            break;
        case Reading:
            currentImage = m_readImage;
            imageName = "atari810read.png";
            break;
        case Writing:
            currentImage = m_writeImage;
            imageName = "atari810write.png";
            break;
    }

    // Always display something, even if the image is null
    if (!currentImage.isNull()) {
        // Scale image to fit widget while maintaining aspect ratio
        QPixmap scaledImage = currentImage.scaled(
            size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaledImage);
        QString stateName = (m_currentState == Off) ? "OFF" :
                           (m_currentState == Empty) ? "EMPTY" :
                           (m_currentState == Closed) ? "CLOSED" :
                           (m_currentState == Reading) ? "READING" : "WRITING";

    } else {
        // Create a placeholder if image is missing
        QPixmap placeholder(size());
        placeholder.fill(Qt::lightGray);
        QPainter painter(&placeholder);
        painter.setPen(Qt::black);
        painter.drawText(placeholder.rect(), Qt::AlignCenter, QString("D%1\n%2").arg(m_driveNumber).arg(
            m_currentState == Off ? "OFF" :
            m_currentState == Empty ? "EMPTY" :
            m_currentState == Closed ? "CLOSED" :
            m_currentState == Reading ? "READ" : "WRITE"));
        m_imageLabel->setPixmap(placeholder);
    }
}

void DiskDriveWidget::updateTooltip()
{
    QString tooltip;

    if (m_driveMode == FUJINET) {
        // FujiNet mode tooltip
        if (m_fujinetDriveInfo.isEmpty) {
            tooltip = QString("D%1: Empty\nDrag disk image here to mount to FujiNet")
                .arg(m_driveNumber);
        } else {
            QString mode = m_fujinetDriveInfo.isReadOnly ? "R/O" : "R/W";
            tooltip = QString("D%1: %2 (%3)\nHost: SD\nStatus: Connected\nClick to eject")
                .arg(m_driveNumber)
                .arg(m_fujinetDriveInfo.filename)
                .arg(mode);
        }
    } else {
        // Local mode tooltip (original behavior)
        tooltip = QString("Drive D%1:").arg(m_driveNumber);

        switch (m_currentState) {
            case Off:
                tooltip += " Off";
                break;
            case Empty:
                tooltip += " On (Empty)";
                break;
            case Closed:
            case Reading:
            case Writing:
                if (hasDisk()) {
                    QFileInfo fileInfo(m_diskPath);
                    tooltip += QString(" %1").arg(fileInfo.fileName());
                } else {
                    tooltip += " On (Disk Inserted)";
                }
                break;
        }
    }

    setToolTip(tooltip);
}

void DiskDriveWidget::startBlinking(DriveState blinkState)
{
    m_blinkState = blinkState;
    m_blinkVisible = true;
    m_blinkTimer->start();
}

void DiskDriveWidget::stopBlinking()
{
    m_blinkTimer->stop();
    m_currentState = m_baseState;
    updateDisplay();
}

QSize DiskDriveWidget::sizeHint() const
{
    int width = DISK_WIDTH;   // All drives use same width now
    int height = m_isDrawerDrive ? (DISK_HEIGHT * 0.84) : DISK_HEIGHT; // Dock drives slightly shorter
    return QSize(width, height);
}

QSize DiskDriveWidget::minimumSizeHint() const
{
    int width = DISK_WIDTH;   // All drives use same width now
    int height = m_isDrawerDrive ? (DISK_HEIGHT * 0.84) : DISK_HEIGHT; // Dock drives slightly shorter
    return QSize(width, height);
}

void DiskDriveWidget::contextMenuEvent(QContextMenuEvent* event)
{
    // Update menu items based on current state
    if (m_driveEnabled) {
        m_toggleAction->setText("Turn Off");
    } else {
        m_toggleAction->setText("Turn On");
    }

    m_insertAction->setEnabled(m_driveEnabled);
    m_ejectAction->setEnabled(m_driveEnabled && hasDisk());

    m_contextMenu->exec(event->globalPos());
}

void DiskDriveWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        // Context menu is handled by contextMenuEvent
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void DiskDriveWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
}

void DiskDriveWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // Check if we have file URLs
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidDiskFile(fileName)) {
                event->acceptProposedAction();
                setStyleSheet("QWidget { border: 2px dashed #0078d4; background-color: rgba(0, 120, 212, 0.1); }");
                return;
            }
        }
    }
    event->ignore();
}

void DiskDriveWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidDiskFile(fileName)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void DiskDriveWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    // Clear visual feedback when drag leaves the widget
    setStyleSheet("");
    QWidget::dragLeaveEvent(event);
}

void DiskDriveWidget::dropEvent(QDropEvent* event)
{
    // Clear visual feedback
    setStyleSheet("");

    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidDiskFile(fileName)) {
                // Auto-enable drive if disabled when dropping a disk
                if (!m_driveEnabled) {
                    qDebug() << "Auto-enabling drive" << m_driveNumber << "for dropped disk:" << fileName;
                    setDriveEnabled(true);
                }
                insertDisk(fileName);
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

bool DiskDriveWidget::isValidDiskFile(const QString& fileName) const
{
    QFileInfo fileInfo(fileName);
    QString extension = fileInfo.suffix().toLower();

    // Valid Atari disk image extensions
    QStringList validExtensions = {"atr", "xfd", "dcm"};

    return validExtensions.contains(extension);
}

void DiskDriveWidget::onToggleDrive()
{
    setDriveEnabled(!m_driveEnabled);
}

void DiskDriveWidget::onInsertDisk()
{
    if (!m_driveEnabled) return;

    // Determine starting directory based on drive mode
    QString startDir;
    if (m_driveMode == FUJINET) {
        // In FujiNet mode, default to SD card path
        QSettings settings;
        QString sdPath = settings.value("fujinet/sdCardPath", "").toString();
        if (!sdPath.isEmpty() && QDir(sdPath).exists()) {
            startDir = sdPath;
        } else {
            // Fallback to home if SD path not configured
            startDir = QDir::homePath();
        }
    } else {
        // In LOCAL mode, use Qt's default (last used directory)
        startDir = QString();
    }

    QString fileName = QFileDialog::getOpenFileName(
        this,
        QString("Select Disk Image for Drive D%1:").arg(m_driveNumber),
        startDir,
        "Atari Disk Images (*.atr *.ATR *.xfd *.XFD *.dcm *.DCM);;All Files (*)"
    );

    if (!fileName.isEmpty()) {
        insertDisk(fileName);
    }
}

void DiskDriveWidget::onEjectDisk()
{
    if (m_driveEnabled && hasDisk()) {
        ejectDisk();
    }
}

void DiskDriveWidget::onBlinkTimer()
{
    m_blinkVisible = !m_blinkVisible;

    if (m_blinkVisible) {
        m_currentState = m_blinkState;
    } else {
        m_currentState = m_baseState;
    }

    updateDisplay();

    // Stop blinking after a reasonable time (2 seconds)
    static int blinkCount = 0;
    if (++blinkCount >= (2000 / BLINK_INTERVAL)) {
        blinkCount = 0;
        stopBlinking();
    }
}

void DiskDriveWidget::onLedDebounceTimeout()
{
    // Debounce timer expired - actually turn off the LED now
    if (m_currentState == Reading || m_currentState == Writing) {
        stopBlinking();
        m_currentState = m_pendingOffState; // Return to saved state
        updateDisplay();
    }
}

// FujiNet mode methods

void DiskDriveWidget::setDriveMode(DriveMode mode)
{
    if (m_driveMode == mode) {
        return;  // No change
    }

    m_driveMode = mode;

    // Update visual indicator based on mode
    if (mode == FUJINET) {
        // Apply subtle blue tint for FujiNet mode
        setStyleSheet("background-color: rgba(100, 150, 255, 30);");
    } else {
        // Clear style for local mode
        setStyleSheet("");
    }

    // Update tooltip to show current mode
    updateTooltip();
}

void DiskDriveWidget::updateFromFujiNet(const FujiNetDrive& driveInfo)
{
    // Only update if in FujiNet mode
    if (m_driveMode != FUJINET) {
        return;
    }

    m_fujinetDriveInfo = driveInfo;

    // FujiNet drives should be enabled when we receive status from FujiNet
    // Set the flag directly without emitting signal to avoid triggering config updates
    if (!m_driveEnabled) {
        m_driveEnabled = true;
    }

    // Update drive state based on FujiNet info
    if (driveInfo.isEmpty) {
        if (m_currentState != Empty) {
            setState(Empty);
        }
        m_diskPath = QString();
    } else {
        m_diskPath = driveInfo.filename;
        if (m_currentState != Closed) {
            setState(Closed);
        }
    }

    updateTooltip();
}

void DiskDriveWidget::showCopyProgress(bool show)
{
    m_showingCopyProgress = show;

    if (m_progressLabel) {
        m_progressLabel->setVisible(show);

        // Position progress label to cover the drive image
        if (show) {
            m_progressLabel->setGeometry(rect());
        }
    }
}
