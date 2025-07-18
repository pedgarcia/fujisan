#ifndef EMULATORWIDGET_H
#define EMULATORWIDGET_H

#include <QWidget>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QImage>
#include "atariemulator.h"

class EmulatorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EmulatorWidget(QWidget *parent = nullptr);
    
    void setEmulator(AtariEmulator* emulator);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void updateDisplay();

private:
    void updateScreenTexture();
    
    AtariEmulator* m_emulator;
    QImage m_screenImage;
    bool m_needsUpdate;
    
    // Screen buffer constants
    static const int SCREEN_WIDTH = 384;
    static const int SCREEN_HEIGHT = 240;
    static const int BORDER_LEFT = 24;
    static const int BORDER_TOP = 24;
    static const int DISPLAY_WIDTH = 336;
    static const int DISPLAY_HEIGHT = 192;
};

#endif // EMULATORWIDGET_H