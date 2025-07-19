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

extern "C" {
    extern int Colours_table[256];
}

EmulatorWidget::EmulatorWidget(QWidget *parent)
    : QWidget(parent)
    , m_emulator(nullptr)
    , m_screenImage(DISPLAY_WIDTH, DISPLAY_HEIGHT, QImage::Format_RGB32)
    , m_needsUpdate(true)
{
    setFocusPolicy(Qt::StrongFocus);
    
    // Allow the widget to expand to fill all available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
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

void EmulatorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    
    if (m_needsUpdate) {
        updateScreenTexture();
        m_needsUpdate = false;
    }
    
    // Fill the entire widget area with the emulator display
    QRect targetRect(0, 0, width(), height());
    
    // Use nearest neighbor scaling for crisp pixels
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
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