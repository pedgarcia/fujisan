/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef VOLUMEKNOB_H
#define VOLUMEKNOB_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPoint>
#include <QTimer>

class VolumeKnob : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

public:
    explicit VolumeKnob(QWidget *parent = nullptr);
    
    int value() const { return m_value; }
    void setValue(int value);
    
    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    
    void setRange(int minimum, int maximum);

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;

private:
    void updateValueFromAngle(double angle);
    double valueToAngle() const;
    void constrainValue();
    
    int m_value;
    int m_minimum;
    int m_maximum;
    bool m_pressed;
    bool m_hovered;
    QPoint m_lastPos;
    
    static const int KNOB_SIZE = 42;
    static const double MIN_ANGLE; // -135 degrees
    static const double MAX_ANGLE; // +135 degrees
    static const double ANGLE_RANGE; // 270 degrees total
};

#endif // VOLUMEKNOB_H