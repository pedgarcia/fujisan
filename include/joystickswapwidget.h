/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef JOYSTICKSWAPWIDGET_H
#define JOYSTICKSWAPWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QPropertyAnimation>

class JoystickSwapWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int animationProgress READ animationProgress WRITE setAnimationProgress)

public:
    explicit JoystickSwapWidget(QWidget *parent = nullptr, bool compact = false);
    
    bool isSwapped() const { return m_swapped; }
    void setSwapped(bool swapped);
    void setCompactMode(bool compact);

signals:
    void toggled(bool swapped);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;

private slots:
    void onAnimationFinished();

private:
    void drawJoystickBox(QPainter &painter, const QRect &rect, const QString &title, 
                        const QStringList &keys, bool isActive, bool isPlayer1);
    void drawSwapArrows(QPainter &painter, const QRect &rect, bool swapped);
    void drawCompactSwap(QPainter &painter);
    void updateTooltip();
    
    int animationProgress() const { return m_animationProgress; }
    void setAnimationProgress(int progress) { m_animationProgress = progress; update(); }

    bool m_swapped;
    bool m_pressed;
    bool m_hovered;
    bool m_animating;
    bool m_compactMode;
    int m_animationProgress; // 0-100
    QPropertyAnimation* m_animation;
    
    static const int WIDGET_HEIGHT = 80;
    static const int WIDGET_WIDTH = 320;
    static const int BOX_WIDTH = 100;
    static const int ARROW_ZONE_WIDTH = 120;
    static const int COMPACT_WIDTH = 60;
    static const int COMPACT_HEIGHT = 20;
};

#endif // JOYSTICKSWAPWIDGET_H