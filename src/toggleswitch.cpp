/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "toggleswitch.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QWidget(parent)
    , m_checked(false)
    , m_hovered(false)
    , m_offset(THUMB_MARGIN)
    , m_onLabel("ON")
    , m_offLabel("OFF")
{
    setFixedSize(SWITCH_WIDTH, SWITCH_HEIGHT);
    setCursor(Qt::PointingHandCursor);
    
    m_animation = new QPropertyAnimation(this, "offset", this);
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

void ToggleSwitch::setChecked(bool checked)
{
    if (m_checked != checked) {
        m_checked = checked;
        
        int startOffset = m_offset;
        int endOffset = checked ? (SWITCH_WIDTH - THUMB_SIZE - THUMB_MARGIN) : THUMB_MARGIN;
        
        m_animation->setStartValue(startOffset);
        m_animation->setEndValue(endOffset);
        m_animation->start();
        
        emit toggled(checked);
    }
}

void ToggleSwitch::setLabels(const QString& onLabel, const QString& offLabel)
{
    m_onLabel = onLabel;
    m_offLabel = offLabel;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background track
    QColor trackColor = m_checked ? QColor(52, 199, 89) : QColor(120, 120, 128);
    if (m_hovered) {
        trackColor = trackColor.lighter(110);
    }
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(0, 0, SWITCH_WIDTH, SWITCH_HEIGHT, SWITCH_HEIGHT/2, SWITCH_HEIGHT/2);
    
    // Labels
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(8);
    font.setBold(true);
    painter.setFont(font);
    
    // Draw appropriate label based on state
    if (m_checked) {
        painter.drawText(QRect(THUMB_MARGIN, 0, SWITCH_WIDTH/2, SWITCH_HEIGHT), 
                        Qt::AlignCenter, m_onLabel);
    } else {
        painter.drawText(QRect(SWITCH_WIDTH/2, 0, SWITCH_WIDTH/2 - THUMB_MARGIN, SWITCH_HEIGHT), 
                        Qt::AlignCenter, m_offLabel);
    }
    
    // Thumb (sliding circle)
    painter.setPen(QPen(QColor(0, 0, 0, 30), 1));
    painter.setBrush(Qt::white);
    
    int thumbY = THUMB_MARGIN;
    painter.drawEllipse(m_offset, thumbY, THUMB_SIZE, THUMB_SIZE);
    
    // Add subtle shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 20));
    painter.drawEllipse(m_offset + 1, thumbY + 1, THUMB_SIZE, THUMB_SIZE);
}

void ToggleSwitch::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        setChecked(!m_checked);
    }
    QWidget::mousePressEvent(event);
}

void ToggleSwitch::enterEvent(QEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void ToggleSwitch::setOffset(int offset)
{
    m_offset = offset;
    update();
}