/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "fujinetwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDebug>

FujiNetWidget::FujiNetWidget(QWidget *parent)
    : QWidget(parent)
    , m_resetButton(nullptr)
    , m_swapButton(nullptr)
    , m_statusLabel(nullptr)
    , m_wifiLed(nullptr)
    , m_busLed(nullptr)
    , m_wifiConnected(false)
    , m_busActive(false)
    , m_fujinetRunning(false)
    , m_busFlashTimer(new QTimer(this))
    , m_activeBusDrives(0)
{
    setupUI();

    m_busFlashTimer->setSingleShot(true);
    m_busFlashTimer->setInterval(150);
    connect(m_busFlashTimer, &QTimer::timeout, this, [this]() {
        m_activeBusDrives = 0;
        setBusActive(false);
    });
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static QString ledStyle(const QString& color, bool lit)
{
    if (lit) {
        return QString(
            "QFrame {"
            "  background-color: %1;"
            "  border-radius: 4px;"
            "  border: 1px solid rgba(255,255,255,40);"
            "}"
        ).arg(color);
    }
    return QString(
        "QFrame {"
        "  background-color: #2a2a2a;"
        "  border-radius: 4px;"
        "  border: 1px solid rgba(255,255,255,15);"
        "}"
    );
}

void FujiNetWidget::applyLedStyle(QFrame* led, const QString& color, bool lit)
{
    if (!led) return;
    led->setStyleSheet(ledStyle(color, lit));
}

// ---------------------------------------------------------------------------
// setupUI
// ---------------------------------------------------------------------------

void FujiNetWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ---- Status row: label + LEDs ----
    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(4);

    m_statusLabel = new QLabel("Stopped", this);
    m_statusLabel->setStyleSheet("QLabel { color: #888888; font-size: 9px; }");
    m_statusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    statusRow->addWidget(m_statusLabel, 1);

    // WiFi LED (green)
    m_wifiLed = new QFrame(this);
    m_wifiLed->setFixedSize(9, 9);
    applyLedStyle(m_wifiLed, "#00c853", false);
    m_wifiLed->setToolTip("WiFi / FujiNet connection");
    statusRow->addWidget(m_wifiLed);

    // Bus LED (yellow)
    m_busLed = new QFrame(this);
    m_busLed->setFixedSize(9, 9);
    applyLedStyle(m_busLed, "#ffca28", false);
    m_busLed->setToolTip("SIO Bus activity");
    statusRow->addWidget(m_busLed);

    mainLayout->addLayout(statusRow);

    // ---- Button row: Reset + Swap ----
    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(4);

    m_resetButton = new QPushButton("Reset", this);
    m_resetButton->setToolTip("Restart FujiNet-PC (returns to CONFIG app)");
    m_resetButton->setFixedHeight(22);
    m_resetButton->setEnabled(false);
    connect(m_resetButton, &QPushButton::clicked, this, &FujiNetWidget::resetRequested);
    buttonRow->addWidget(m_resetButton);

    m_swapButton = new QPushButton("Swap", this);
    m_swapButton->setToolTip("Rotate disk images (equivalent to FujiNet Button A)");
    m_swapButton->setFixedHeight(22);
    m_swapButton->setEnabled(false);
    connect(m_swapButton, &QPushButton::clicked, this, &FujiNetWidget::swapRequested);
    buttonRow->addWidget(m_swapButton);

    mainLayout->addLayout(buttonRow);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void FujiNetWidget::setWifiConnected(bool connected)
{
    if (m_wifiConnected == connected)
        return;
    m_wifiConnected = connected;
    applyLedStyle(m_wifiLed, "#00c853", connected);
}

void FujiNetWidget::setBusActive(bool active)
{
    if (m_busActive == active)
        return;
    m_busActive = active;
    applyLedStyle(m_busLed, "#ffca28", active);
}

void FujiNetWidget::setFujiNetRunning(bool running)
{
    m_fujinetRunning = running;
    if (m_resetButton) m_resetButton->setEnabled(running);
    if (!running) {
        // When process stops, clear all indicators
        setWifiConnected(false);
        setBusActive(false);
        m_activeBusDrives = 0;
        m_busFlashTimer->stop();
        if (m_statusLabel) {
            m_statusLabel->setText("Stopped");
            m_statusLabel->setStyleSheet("QLabel { color: #888888; font-size: 9px; }");
        }
        if (m_swapButton) m_swapButton->setEnabled(false);
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void FujiNetWidget::onConnected()
{
    setWifiConnected(true);
    if (m_statusLabel) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { color: #4caf50; font-size: 9px; }");
    }
    if (m_swapButton) m_swapButton->setEnabled(true);
}

void FujiNetWidget::onDisconnected()
{
    setWifiConnected(false);
    if (m_statusLabel) {
        m_statusLabel->setText("Disconnected");
        m_statusLabel->setStyleSheet("QLabel { color: #f44336; font-size: 9px; }");
    }
    if (m_swapButton) m_swapButton->setEnabled(false);
}

void FujiNetWidget::onBusActivity(int driveNumber, bool isWriting)
{
    Q_UNUSED(driveNumber);
    Q_UNUSED(isWriting);
    m_activeBusDrives++;
    m_busFlashTimer->stop();
    setBusActive(true);
}

void FujiNetWidget::onBusIdle(int driveNumber)
{
    Q_UNUSED(driveNumber);
    if (m_activeBusDrives > 0)
        m_activeBusDrives--;
    if (m_activeBusDrives <= 0) {
        m_busFlashTimer->start();
    }
}
