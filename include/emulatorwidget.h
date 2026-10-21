/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef EMULATORWIDGET_H
#define EMULATORWIDGET_H

#include <QWidget>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QImage>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include "atariemulator.h"
#include "crtrenderer.h"

class EmulatorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EmulatorWidget(QWidget *parent = nullptr);

    void setEmulator(AtariEmulator* emulator);
    void setScalingSettings(bool integerScaling, bool scalingFilter, const QString& fitScreen, bool keepAspectRatio, double overscanFactor);
    void setCrtSettings(const CrtSettings& settings);
    const CrtSettings& crtSettings() const { return m_crtRenderer.settings(); }

signals:
    void diskDroppedOnEmulator(const QString& filename);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void updateDisplay(const QImage& image);

private:
    QRect calculateDisplayRect() const;
    bool isValidExecutableFile(const QString& fileName) const;
    bool isValidDiskFile(const QString& fileName) const;

    AtariEmulator* m_emulator;
    QImage m_screenImage;

    // Video scaling settings
    bool m_integerScaling;
    bool m_scalingFilter;
    QString m_fitScreen;
    bool m_keepAspectRatio;
    double m_overscanFactor;

    CrtRenderer m_crtRenderer;
    
    // Screen buffer constants - show full screen without cropping
    static const int SCREEN_WIDTH = 384;
    static const int SCREEN_HEIGHT = 240;
    static const int BORDER_LEFT = 0;   // No horizontal cropping
    static const int BORDER_TOP = 0;    // No vertical cropping
    static const int DISPLAY_WIDTH = 384;   // Full width
    static const int DISPLAY_HEIGHT = 240;  // Full height
    static constexpr double MIN_OVERSCAN_FACTOR = 0.95;
    static constexpr double MAX_OVERSCAN_FACTOR = 1.05;
    static constexpr double DEFAULT_OVERSCAN_FACTOR = 1.0;
    // Bounds the per-frame CPU cost of the CRT effect on very large displays;
    // beyond this the rendered image is scaled up by the painter.
    static const int MAX_CRT_HEIGHT = 1600;
};

#endif // EMULATORWIDGET_H