/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "joystickswapwidget.h"
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QApplication>

JoystickSwapWidget::JoystickSwapWidget(QWidget *parent, bool compact)
    : QWidget(parent)
    , m_swapped(false)
    , m_pressed(false)
    , m_hovered(false)
    , m_animating(false)
    , m_compactMode(compact)
    , m_animationProgress(0)
    , m_animation(new QPropertyAnimation(this, "animationProgress"))
{
    if (m_compactMode) {
        setFixedSize(COMPACT_WIDTH, COMPACT_HEIGHT);
    } else {
        setFixedSize(WIDGET_WIDTH, WIDGET_HEIGHT);
    }
    setCursor(Qt::PointingHandCursor);
    updateTooltip();
    
    // Setup animation
    m_animation->setDuration(200); // Faster for compact mode
    m_animation->setStartValue(0);
    m_animation->setEndValue(100);
    connect(m_animation, &QPropertyAnimation::finished, this, &JoystickSwapWidget::onAnimationFinished);
}

void JoystickSwapWidget::setSwapped(bool swapped)
{
    if (m_swapped != swapped) {
        m_swapped = swapped;
        updateTooltip();
        update();
    }
}

void JoystickSwapWidget::setCompactMode(bool compact)
{
    if (m_compactMode != compact) {
        m_compactMode = compact;
        if (m_compactMode) {
            setFixedSize(COMPACT_WIDTH, COMPACT_HEIGHT);
        } else {
            setFixedSize(WIDGET_WIDTH, WIDGET_HEIGHT);
        }
        updateTooltip();
        update();
    }
}

void JoystickSwapWidget::updateTooltip()
{
    if (m_compactMode) {
        QString tooltip = QString("Click to swap joystick assignments\n\n");
        if (!m_swapped) {
            tooltip += "Current (Normal):\n";
            tooltip += "• J1 (Player 1): Numpad (↑←↓→, RCtrl)\n";
            tooltip += "• J2 (Player 2): WASD (WASD, Space)\n\n";
            tooltip += "Click to swap to:\n";
            tooltip += "• J1 (Player 1): WASD (WASD, Space)\n";
            tooltip += "• J2 (Player 2): Numpad (↑←↓→, RCtrl)";
        } else {
            tooltip += "Current (Swapped):\n";
            tooltip += "• J1 (Player 1): WASD (WASD, Space)\n";
            tooltip += "• J2 (Player 2): Numpad (↑←↓→, RCtrl)\n\n";
            tooltip += "Click to swap to:\n";
            tooltip += "• J1 (Player 1): Numpad (↑←↓→, RCtrl)\n";
            tooltip += "• J2 (Player 2): WASD (WASD, Space)";
        }
        setToolTip(tooltip);
    } else {
        setToolTip("Click to swap joystick assignments\n"
                   "Default: WASD→J2, Numpad→J1\n"
                   "Swapped: WASD→J1, Numpad→J2");
    }
}

QSize JoystickSwapWidget::sizeHint() const
{
    if (m_compactMode) {
        return QSize(COMPACT_WIDTH, COMPACT_HEIGHT);
    }
    return QSize(WIDGET_WIDTH, WIDGET_HEIGHT);
}

void JoystickSwapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    if (m_compactMode) {
        drawCompactSwap(painter);
        return;
    }
    
    // Full mode - existing implementation
    // Background
    QLinearGradient backgroundGradient(0, 0, 0, height());
    if (m_hovered) {
        backgroundGradient.setColorAt(0, QColor(250, 250, 250));
        backgroundGradient.setColorAt(1, QColor(240, 240, 240));
    } else {
        backgroundGradient.setColorAt(0, QColor(245, 245, 245));
        backgroundGradient.setColorAt(1, QColor(235, 235, 235));
    }
    
    painter.fillRect(rect(), backgroundGradient);
    
    // Draw border
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    
    // Calculate layout
    int boxY = 10;
    int boxHeight = height() - 20;
    
    QRect leftBox, rightBox, arrowZone;
    
    if (!m_swapped) {
        // Normal: WASD on right (Joy1), Numpad on left (Joy0)
        leftBox = QRect(10, boxY, BOX_WIDTH, boxHeight);
        arrowZone = QRect(leftBox.right(), boxY, ARROW_ZONE_WIDTH, boxHeight);
        rightBox = QRect(arrowZone.right(), boxY, BOX_WIDTH, boxHeight);
    } else {
        // Swapped: WASD on left (Joy0), Numpad on right (Joy1)
        leftBox = QRect(10, boxY, BOX_WIDTH, boxHeight);
        arrowZone = QRect(leftBox.right(), boxY, ARROW_ZONE_WIDTH, boxHeight);
        rightBox = QRect(arrowZone.right(), boxY, BOX_WIDTH, boxHeight);
    }
    
    // Draw joystick boxes
    if (!m_swapped) {
        drawJoystickBox(painter, leftBox, "Player 1", {"↑", "←↓→", "RCtrl"}, true, true);
        drawJoystickBox(painter, rightBox, "Player 2", {"W", "ASD", "Space"}, true, false);
    } else {
        drawJoystickBox(painter, leftBox, "Player 1", {"W", "ASD", "Space"}, true, true);
        drawJoystickBox(painter, rightBox, "Player 2", {"↑", "←↓→", "RCtrl"}, true, false);
    }
    
    // Draw swap arrows
    drawSwapArrows(painter, arrowZone, m_swapped);
}

void JoystickSwapWidget::drawJoystickBox(QPainter &painter, const QRect &rect, const QString &title, 
                                        const QStringList &keys, bool isActive, bool isPlayer1)
{
    // Box background
    QLinearGradient boxGradient(rect.topLeft(), rect.bottomLeft());
    QColor playerColor = isPlayer1 ? QColor(100, 150, 255) : QColor(255, 150, 100);
    
    if (isActive) {
        boxGradient.setColorAt(0, playerColor.lighter(130));
        boxGradient.setColorAt(1, playerColor);
    } else {
        boxGradient.setColorAt(0, QColor(220, 220, 220));
        boxGradient.setColorAt(1, QColor(200, 200, 200));
    }
    
    painter.fillRect(rect, boxGradient);
    
    // Box border
    painter.setPen(QPen(isActive ? playerColor.darker(120) : QColor(150, 150, 150), 2));
    painter.drawRect(rect);
    
    // Title
    painter.setPen(Qt::white);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter.setFont(titleFont);
    
    QRect titleRect = rect.adjusted(2, 2, -2, -rect.height() + 20);
    painter.drawText(titleRect, Qt::AlignCenter, title);
    
    // Keys
    painter.setPen(Qt::white);
    QFont keyFont = painter.font();
    keyFont.setBold(false);
    keyFont.setPointSize(8);
    painter.setFont(keyFont);
    
    QRect keysRect = rect.adjusted(2, 22, -2, -2);
    for (int i = 0; i < keys.size(); ++i) {
        QRect keyLineRect = QRect(keysRect.x(), keysRect.y() + i * 14, keysRect.width(), 12);
        painter.drawText(keyLineRect, Qt::AlignCenter, keys[i]);
    }
}

void JoystickSwapWidget::drawSwapArrows(QPainter &painter, const QRect &rect, bool swapped)
{
    painter.setPen(QPen(QColor(100, 100, 100), 2));
    
    int centerX = rect.center().x();
    int centerY = rect.center().y();
    int arrowSize = 15;
    
    // Animated rotation based on swap state and animation progress
    painter.save();
    painter.translate(centerX, centerY);
    
    if (m_animating) {
        // During animation, rotate based on progress
        double rotationAngle = (m_animationProgress / 100.0) * 180.0;
        if (swapped) rotationAngle = 180.0 - rotationAngle;
        painter.rotate(rotationAngle);
    } else {
        // Static state
        if (swapped) painter.rotate(180);
    }
    
    // Draw swap arrows
    painter.setPen(QPen(QColor(80, 80, 80), 3));
    
    // Upper arrow (pointing right)
    QPolygon upperArrow;
    upperArrow << QPoint(-30, -8) << QPoint(20, -8) << QPoint(15, -15) << QPoint(30, 0) << QPoint(15, -1);
    painter.drawPolyline(upperArrow);
    
    // Lower arrow (pointing left)  
    QPolygon lowerArrow;
    lowerArrow << QPoint(30, 8) << QPoint(-20, 8) << QPoint(-15, 15) << QPoint(-30, 0) << QPoint(-15, 1);
    painter.drawPolyline(lowerArrow);
    
    painter.restore();
    
    // Status text
    painter.setPen(QColor(60, 60, 60));
    QFont statusFont = painter.font();
    statusFont.setBold(true);
    statusFont.setPointSize(9);
    painter.setFont(statusFont);
    
    QString statusText = swapped ? "SWAPPED" : "NORMAL";
    QRect statusRect = rect.adjusted(0, 35, 0, 0);
    painter.drawText(statusRect, Qt::AlignCenter, statusText);
}

void JoystickSwapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
}

void JoystickSwapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_pressed) {
        m_pressed = false;
        
        if (!m_animating && rect().contains(event->pos())) {
            // Start animation
            m_animating = true;
            m_animation->start();
        }
        update();
    }
}

void JoystickSwapWidget::enterEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void JoystickSwapWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    m_pressed = false;
    update();
}

void JoystickSwapWidget::drawCompactSwap(QPainter &painter)
{
    // Compact background with subtle gradient
    QLinearGradient gradient(0, 0, 0, height());
    if (m_hovered || m_pressed) {
        gradient.setColorAt(0, QColor(240, 240, 255));
        gradient.setColorAt(1, QColor(220, 220, 240));
    } else {
        gradient.setColorAt(0, QColor(248, 248, 248));
        gradient.setColorAt(1, QColor(235, 235, 235));
    }
    
    painter.fillRect(rect(), gradient);
    
    // Border
    painter.setPen(QPen(QColor(180, 180, 180), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    
    // Text content
    painter.setPen(QColor(50, 50, 50));
    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    
    QString text;
    if (!m_swapped) {
        text = "J1⇄J2";  // Normal state
    } else {
        text = "J2⇄J1";  // Swapped state  
    }
    
    // Add flip animation effect
    if (m_animating) {
        painter.save();
        painter.translate(width() / 2, height() / 2);
        double angle = (m_animationProgress / 100.0) * 180.0;
        painter.rotate(angle);
        painter.translate(-width() / 2, -height() / 2);
    }
    
    QRect textRect = rect().adjusted(2, 1, -2, -1);
    painter.drawText(textRect, Qt::AlignCenter, text);
    
    if (m_animating) {
        painter.restore();
    }
    
    // Small indicator dot
    QColor dotColor = m_swapped ? QColor(255, 150, 100) : QColor(100, 150, 255);
    painter.setPen(Qt::NoPen);
    painter.setBrush(dotColor);
    painter.drawEllipse(width() - 8, 2, 4, 4);
}

void JoystickSwapWidget::onAnimationFinished()
{
    m_animating = false;
    m_swapped = !m_swapped;
    m_animationProgress = 0;
    updateTooltip(); // Update tooltip after swap
    emit toggled(m_swapped);
    update();
}