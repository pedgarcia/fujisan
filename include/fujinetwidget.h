/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef FUJINETWIDGET_H
#define FUJINETWIDGET_H

#include <QWidget>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

/**
 * FujiNetWidget — hardware-inspired control panel for FujiNet-PC.
 *
 * Maps to real FujiNet physical controls:
 *   Reset button  = Button C ("Safe Reset") — kills + restarts FujiNet-PC
 *   Swap  button  = Button A (short press)  — rotates disk images (GET /swap)
 *   WiFi  LED     = green when HTTP health check succeeds (connected)
 *   Bus   LED     = yellow during SIO disk activity
 *
 * Signals emitted to MainWindow:
 *   resetRequested() — user wants to restart FujiNet-PC
 *   swapRequested()  — user wants to rotate disk images
 */
class FujiNetWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FujiNetWidget(QWidget *parent = nullptr);

    // LED control (driven by MainWindow via FujiNetService / FujiNetProcessManager signals)
    void setWifiConnected(bool connected);
    void setBusActive(bool active);

    // State
    void setFujiNetRunning(bool running);  // Enable/disable buttons

signals:
    void resetRequested();  // User pressed Reset button
    void swapRequested();   // User pressed Swap button

public slots:
    void onConnected();
    void onDisconnected();
    void onBusActivity(int driveNumber, bool isWriting);
    void onBusIdle(int driveNumber);

private:
    void setupUI();
    void applyLedStyle(QFrame* led, const QString& color, bool lit);

    QPushButton* m_resetButton;
    QPushButton* m_swapButton;
    QLabel*      m_statusLabel;
    QFrame*      m_wifiLed;   // LED circle for WiFi (QFrame + border-radius stylesheet)
    QFrame*      m_busLed;    // LED circle for Bus activity

    bool m_wifiConnected;
    bool m_busActive;
    bool m_fujinetRunning;

    QTimer* m_busFlashTimer;  // Auto-clears bus LED after brief inactivity
    int     m_activeBusDrives; // Count of currently active bus operations
};

#endif // FUJINETWIDGET_H
