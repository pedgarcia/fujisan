/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPropertyAnimation>

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int offset READ offset WRITE setOffset)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
    void setLabels(const QString& onLabel, const QString& offLabel);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void setOffset(int offset);

private:
    int offset() const { return m_offset; }
    
    bool m_checked;
    bool m_hovered;
    int m_offset;
    QPropertyAnimation* m_animation;
    QString m_onLabel;
    QString m_offLabel;
    
    static const int SWITCH_WIDTH = 50;
    static const int SWITCH_HEIGHT = 22;
    static const int THUMB_SIZE = 18;
    static const int THUMB_MARGIN = 2;
};

#endif // TOGGLESWITCH_H