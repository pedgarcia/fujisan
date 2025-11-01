/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "emulatorwidget.h"
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>

extern "C" {
    extern int Colours_table[256];
}

EmulatorWidget::EmulatorWidget(QWidget *parent)
    : QWidget(parent)
    , m_emulator(nullptr)
    , m_screenImage(DISPLAY_WIDTH, DISPLAY_HEIGHT, QImage::Format_RGB32)
    , m_needsUpdate(true)
    , m_integerScaling(false)
    , m_scalingFilter(true)
    , m_fitScreen("both")
    , m_keepAspectRatio(true)
{
    setFocusPolicy(Qt::StrongFocus);

    // Allow the widget to expand to fill all available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Enable drag and drop for XEX files
    setAcceptDrops(true);

    // Fill with black initially
    m_screenImage.fill(Qt::black);
}

void EmulatorWidget::setEmulator(AtariEmulator* emulator)
{
    m_emulator = emulator;
    if (m_emulator) {
        connect(m_emulator, &AtariEmulator::frameReady, this, &EmulatorWidget::updateDisplay);
    }
}

void EmulatorWidget::setScalingSettings(bool integerScaling, bool scalingFilter, const QString& fitScreen, bool keepAspectRatio)
{
    m_integerScaling = integerScaling;
    m_scalingFilter = scalingFilter;
    m_fitScreen = fitScreen;
    m_keepAspectRatio = keepAspectRatio;
    update(); // Trigger repaint with new settings
}

void EmulatorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    
    if (m_needsUpdate) {
        updateScreenTexture();
        m_needsUpdate = false;
    }
    
    // Fill widget background with black (overscan area)
    painter.fillRect(rect(), Qt::black);

    // Calculate authentic Atari display area with proper aspect ratio
    QRect targetRect = calculateDisplayRect();

    // Apply scaling filter setting (smooth vs nearest-neighbor)
    // Integer scaling always uses nearest-neighbor for pixel-perfect display
    bool useSmoothScaling = m_scalingFilter && !m_integerScaling;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, useSmoothScaling);
    painter.drawImage(targetRect, m_screenImage);
}

void EmulatorWidget::updateDisplay()
{
    m_needsUpdate = true;
    update(); // Trigger a repaint
}

void EmulatorWidget::updateScreenTexture()
{
    if (!m_emulator) {
        return;
    }
    
    const unsigned char* screen = m_emulator->getScreen();
    if (!screen) {
        return;
    }
    
    // Convert Atari screen buffer to QImage
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        QRgb* scanLine = reinterpret_cast<QRgb*>(m_screenImage.scanLine(y));
        
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            // Direct pixel mapping from display area
            int srcX = BORDER_LEFT + x;
            int srcY = BORDER_TOP + y;
            
            // Bounds check
            if (srcX >= SCREEN_WIDTH || srcY >= SCREEN_HEIGHT) {
                continue;
            }
            
            // Get the color index from the screen buffer
            unsigned char colorIndex = screen[srcY * SCREEN_WIDTH + srcX];
            
            // Use the actual Atari color table from the emulator
            // Colours_table[colorIndex] contains RGB in format 0x00RRGGBB
            int rgbValue = Colours_table[colorIndex];
            
            // Extract RGB components
            unsigned char r = (rgbValue >> 16) & 0xFF;
            unsigned char g = (rgbValue >> 8) & 0xFF;
            unsigned char b = rgbValue & 0xFF;
            
            scanLine[x] = qRgb(r, g, b);
        }
    }
}

void EmulatorWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_emulator) {
        m_emulator->handleKeyPress(event);
    }
    QWidget::keyPressEvent(event);
}

void EmulatorWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (m_emulator) {
        m_emulator->handleKeyRelease(event);
    }
    QWidget::keyReleaseEvent(event);
}

void EmulatorWidget::focusInEvent(QFocusEvent *event)
{
    qDebug() << "EmulatorWidget gained focus";
    QWidget::focusInEvent(event);
}

void EmulatorWidget::mousePressEvent(QMouseEvent *event)
{
    // Grab focus when clicked
    setFocus();
    QWidget::mousePressEvent(event);
}

QRect EmulatorWidget::calculateDisplayRect() const
{
    // Get current video system from emulator to apply correct pixel aspect ratio
    double pixelAspectRatio = 1.0; // Default to square pixels

    if (m_emulator && m_keepAspectRatio) {
        QString videoSystem = m_emulator->getVideoSystem();

        if (videoSystem == "-ntsc") {
            // NTSC: Compensates for 14.31818 MHz actual vs ideal 12+3/11 MHz pixel clock
            pixelAspectRatio = (12.0 + 3.0/11.0) / 14.31818; // ≈ 0.857
        } else if (videoSystem == "-pal") {
            // PAL: Use similar compression as NTSC for consistent appearance
            // Both NTSC and PAL target 4:3 TVs, so should look very similar
            pixelAspectRatio = 0.87; // Slightly less compression than NTSC
        }
    }

    // Apply pixel aspect ratio correction to source dimensions
    const double correctedWidth = DISPLAY_WIDTH * pixelAspectRatio;
    const double correctedHeight = DISPLAY_HEIGHT;

    // Get available widget dimensions
    const int widgetWidth = width();
    const int widgetHeight = height();

    int displayWidth, displayHeight;

    if (m_integerScaling) {
        // INTEGER SCALING MODE - Based on atari800 videomode.c algorithm
        // Calculate maximum integer multipliers for each dimension
        int multW = static_cast<int>(widgetWidth / correctedWidth);
        int multH = static_cast<int>(widgetHeight / correctedHeight);

        // Ensure minimum 1x scaling
        if (multW == 0) multW = 1;
        if (multH == 0) multH = 1;

        // Apply fitScreen strategy
        int finalMult;
        if (m_fitScreen == "width") {
            // Fit to width - use width multiplier for both dimensions
            finalMult = multW;
        } else if (m_fitScreen == "height") {
            // Fit to height - use height multiplier for both dimensions
            finalMult = multH;
        } else { // "both" or default
            // Fit both - use smaller multiplier to ensure it fits
            finalMult = qMin(multW, multH);
        }

        // Calculate final integer-scaled dimensions
        displayWidth = static_cast<int>(correctedWidth * finalMult);
        displayHeight = static_cast<int>(correctedHeight * finalMult);

    } else {
        // SMOOTH SCALING MODE - Original floating-point scaling
        // Calculate scaling to fit within widget while preserving aspect ratio
        const double scaleX = widgetWidth / correctedWidth;
        const double scaleY = widgetHeight / correctedHeight;
        const double scale = qMin(scaleX, scaleY);

        // Use more screen space while keeping small border for clean look
        const double overscanFactor = 0.98;
        const double finalScale = scale * overscanFactor;

        // Calculate final display dimensions
        displayWidth = static_cast<int>(correctedWidth * finalScale);
        displayHeight = static_cast<int>(correctedHeight * finalScale);
    }

    // Center the display within the widget (letterboxing/pillarboxing)
    const int x = (widgetWidth - displayWidth) / 2;
    const int y = (widgetHeight - displayHeight) / 2;

    return QRect(x, y, displayWidth, displayHeight);
}

void EmulatorWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // Check if we have file URLs
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidExecutableFile(fileName)) {
                event->acceptProposedAction();
                setStyleSheet("QWidget { border: 3px dashed #FFD700; background-color: rgba(255, 215, 0, 0.1); }");
                return;
            } else if (isValidDiskFile(fileName)) {
                event->acceptProposedAction();
                setStyleSheet("QWidget { border: 3px dashed #00BFFF; background-color: rgba(0, 191, 255, 0.1); }");
                return;
            }
        }
    }
    event->ignore();
}

void EmulatorWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidExecutableFile(fileName) || isValidDiskFile(fileName)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void EmulatorWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    // Clear visual feedback when drag leaves the widget
    setStyleSheet("");
    QWidget::dragLeaveEvent(event);
}

void EmulatorWidget::dropEvent(QDropEvent* event)
{
    // Clear visual feedback
    setStyleSheet("");
    
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidExecutableFile(fileName) && m_emulator) {
                qDebug() << "Loading executable file:" << fileName;
                bool success = m_emulator->loadFile(fileName);
                if (success) {
                    qDebug() << "Successfully loaded and executed:" << fileName;
                    // Emit signal to parent window for status update
                    QFileInfo fileInfo(fileName);
                    // Note: We could emit a signal here if we want to show status in the main window
                    // For now, the debug output and emulator state change is sufficient
                } else {
                    qDebug() << "Failed to load executable:" << fileName;
                }
                event->acceptProposedAction();
                return;
            } else if (isValidDiskFile(fileName)) {
                qDebug() << "Disk image dropped on emulator screen:" << fileName;
                // Emit signal to MainWindow to handle D1 mounting and reboot
                emit diskDroppedOnEmulator(fileName);
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

bool EmulatorWidget::isValidExecutableFile(const QString& fileName) const
{
    QFileInfo fileInfo(fileName);
    QString extension = fileInfo.suffix().toLower();
    
    // Valid Atari executable extensions
    QStringList validExtensions = {"xex", "exe", "com"};
    
    return validExtensions.contains(extension);
}

bool EmulatorWidget::isValidDiskFile(const QString& fileName) const
{
    QFileInfo fileInfo(fileName);
    QString extension = fileInfo.suffix().toLower();
    
    // Valid Atari disk image extensions
    QStringList validExtensions = {"atr", "xfd", "dcm"};
    
    return validExtensions.contains(extension);
}