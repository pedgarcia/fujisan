/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "volumeknob.h"
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <QApplication>

const double VolumeKnob::MIN_ANGLE = -135.0; // -135 degrees
const double VolumeKnob::MAX_ANGLE = 135.0;  // +135 degrees  
const double VolumeKnob::ANGLE_RANGE = 270.0; // 270 degrees total

VolumeKnob::VolumeKnob(QWidget *parent)
    : QWidget(parent)
    , m_value(75)
    , m_minimum(0)
    , m_maximum(100)
    , m_pressed(false)
    , m_hovered(false)
{
    setFixedSize(KNOB_SIZE, KNOB_SIZE);
    setCursor(Qt::PointingHandCursor);
    setToolTip("Volume control - drag to adjust, scroll wheel to fine-tune");
    setAttribute(Qt::WA_Hover, true);
}

void VolumeKnob::setValue(int value)
{
    int newValue = qBound(m_minimum, value, m_maximum);
    if (m_value != newValue) {
        m_value = newValue;
        update();
        emit valueChanged(m_value);
    }
}

void VolumeKnob::setMinimum(int minimum)
{
    m_minimum = minimum;
    constrainValue();
}

void VolumeKnob::setMaximum(int maximum)
{
    m_maximum = maximum;
    constrainValue();
}

void VolumeKnob::setRange(int minimum, int maximum)
{
    m_minimum = minimum;
    m_maximum = maximum;
    constrainValue();
}

void VolumeKnob::constrainValue()
{
    int newValue = qBound(m_minimum, m_value, m_maximum);
    if (m_value != newValue) {
        m_value = newValue;
        update();
        emit valueChanged(m_value);
    }
}

QSize VolumeKnob::sizeHint() const
{
    return QSize(KNOB_SIZE, KNOB_SIZE);
}

void VolumeKnob::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRectF knobRect = rect().adjusted(2, 2, -2, -2);
    QPointF center = knobRect.center();
    double radius = knobRect.width() / 2.0;
    
    // Draw outer ring (track)
    painter.setPen(QPen(QColor(180, 180, 180), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(knobRect);
    
    // Draw value arc
    double currentAngle = valueToAngle();
    QRectF arcRect = knobRect.adjusted(1, 1, -1, -1);
    
    if (m_value > m_minimum) {
        QPen arcPen(QColor(100, 150, 255), 3);
        painter.setPen(arcPen);
        painter.setBrush(Qt::NoBrush);
        
        // Arc from MIN_ANGLE to current angle
        double startAngle = MIN_ANGLE + 90; // QPainter uses 0° at 3 o'clock, adjust
        double spanAngle = currentAngle - MIN_ANGLE;
        
        painter.drawArc(arcRect, startAngle * 16, spanAngle * 16); // QPainter uses 1/16th degrees
    }
    
    // Draw knob body
    QRectF knobBodyRect = knobRect.adjusted(6, 6, -6, -6);
    
    QRadialGradient knobGradient(center, radius * 0.6);
    if (m_pressed) {
        knobGradient.setColorAt(0, QColor(220, 220, 220));
        knobGradient.setColorAt(1, QColor(160, 160, 160));
    } else if (m_hovered) {
        knobGradient.setColorAt(0, QColor(250, 250, 250));
        knobGradient.setColorAt(1, QColor(200, 200, 200));
    } else {
        knobGradient.setColorAt(0, QColor(240, 240, 240));
        knobGradient.setColorAt(1, QColor(180, 180, 180));
    }
    
    painter.setPen(QPen(QColor(140, 140, 140), 1));
    painter.setBrush(knobGradient);
    painter.drawEllipse(knobBodyRect);
    
    // Draw indicator line
    double indicatorAngle = qDegreesToRadians(currentAngle + 90); // Convert to standard math coordinates
    double indicatorLength = radius * 0.5;
    
    QPointF indicatorEnd(
        center.x() + indicatorLength * cos(indicatorAngle),
        center.y() + indicatorLength * sin(indicatorAngle)
    );
    
    painter.setPen(QPen(QColor(80, 80, 80), 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, indicatorEnd);
    
    // Draw center dot
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(100, 100, 100));
    painter.drawEllipse(center, 2, 2);
}

double VolumeKnob::valueToAngle() const
{
    if (m_maximum == m_minimum) return MIN_ANGLE;
    
    double normalizedValue = double(m_value - m_minimum) / double(m_maximum - m_minimum);
    return MIN_ANGLE + normalizedValue * ANGLE_RANGE;
}

void VolumeKnob::updateValueFromAngle(double angle)
{
    // Constrain angle to valid range
    angle = qBound(MIN_ANGLE, angle, MAX_ANGLE);
    
    // Convert angle to value
    double normalizedValue = (angle - MIN_ANGLE) / ANGLE_RANGE;
    int newValue = m_minimum + qRound(normalizedValue * (m_maximum - m_minimum));
    
    setValue(newValue);
}

void VolumeKnob::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_lastPos = event->pos();
        update();
    }
}

void VolumeKnob::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressed) {
        QPointF center = rect().center();
        QPointF mousePos = event->pos();
        
        // Calculate angle from center to mouse position
        double angle = qRadiansToDegrees(atan2(mousePos.y() - center.y(), mousePos.x() - center.x()));
        
        // Convert from standard math coordinates to our coordinate system
        angle -= 90;
        
        // Handle angle wrapping
        if (angle < -180) angle += 360;
        if (angle > 180) angle -= 360;
        
        updateValueFromAngle(angle);
        m_lastPos = event->pos();
    }
}

void VolumeKnob::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        update();
    }
}

void VolumeKnob::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    int step = 1;
    
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        step = 5; // Larger steps with Shift
    }
    
    if (delta > 0) {
        setValue(m_value + step);
    } else {
        setValue(m_value - step);
    }
    
    event->accept();
}

void VolumeKnob::enterEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void VolumeKnob::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    m_pressed = false;
    update();
}