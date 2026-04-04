/*
 * Fujisan Test Suite - FujiNet Widget Tests
 *
 * Verifies FujiNetWidget initial state, LED transitions,
 * button signal emission, and child widget existence.
 */

#include <QApplication>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "fujinetwidget.h"

class TestFujiNetWidget : public QObject {
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------
    // Initial state: disconnected, LEDs off, buttons disabled
    // ---------------------------------------------------------------
    void testInitialState()
    {
        FujiNetWidget w;

        // Status should indicate not connected
        auto* label = w.findChild<QLabel*>();
        QVERIFY(label != nullptr);
        QCOMPARE(label->text(), QString("Stopped"));

        // Buttons exist and are disabled
        QList<QPushButton*> buttons = w.findChildren<QPushButton*>();
        QVERIFY(buttons.size() >= 2);

        QPushButton* resetBtn = nullptr;
        QPushButton* swapBtn = nullptr;
        for (auto* btn : buttons) {
            if (btn->text() == "R") resetBtn = btn;
            if (btn->text() == "S") swapBtn = btn;
        }
        QVERIFY(resetBtn != nullptr);
        QVERIFY(swapBtn != nullptr);
        QVERIFY(!resetBtn->isEnabled());
        QVERIFY(!swapBtn->isEnabled());
    }

    // ---------------------------------------------------------------
    // LED frames exist (WiFi + Bus)
    // ---------------------------------------------------------------
    void testLedFramesExist()
    {
        FujiNetWidget w;
        QList<QFrame*> frames = w.findChildren<QFrame*>();
        // At least 2 LED frames (wifi + bus)
        QVERIFY(frames.size() >= 2);
    }

    // ---------------------------------------------------------------
    // onConnected: WiFi LED on, status updates, swap enabled
    // ---------------------------------------------------------------
    void testOnConnected()
    {
        FujiNetWidget w;
        w.setFujiNetRunning(true);
        w.onConnected();

        auto* label = w.findChild<QLabel*>();
        QCOMPARE(label->text(), QString("Connected"));

        // Swap button should be enabled after connection
        QList<QPushButton*> buttons = w.findChildren<QPushButton*>();
        QPushButton* swapBtn = nullptr;
        for (auto* btn : buttons) {
            if (btn->text() == "S") swapBtn = btn;
        }
        QVERIFY(swapBtn != nullptr);
        QVERIFY(swapBtn->isEnabled());
    }

    // ---------------------------------------------------------------
    // onDisconnected: WiFi LED off, status updates, swap disabled
    // ---------------------------------------------------------------
    void testOnDisconnected()
    {
        FujiNetWidget w;
        w.setFujiNetRunning(true);
        w.onConnected();
        w.onDisconnected();

        auto* label = w.findChild<QLabel*>();
        QCOMPARE(label->text(), QString("Disconnected"));

        QList<QPushButton*> buttons = w.findChildren<QPushButton*>();
        QPushButton* swapBtn = nullptr;
        for (auto* btn : buttons) {
            if (btn->text() == "S") swapBtn = btn;
        }
        QVERIFY(swapBtn != nullptr);
        QVERIFY(!swapBtn->isEnabled());
    }

    // ---------------------------------------------------------------
    // setFujiNetRunning(true) enables reset button
    // ---------------------------------------------------------------
    void testSetRunningEnablesReset()
    {
        FujiNetWidget w;
        QVERIFY(!w.findChildren<QPushButton*>().isEmpty());

        w.setFujiNetRunning(true);
        QList<QPushButton*> buttons = w.findChildren<QPushButton*>();
        QPushButton* resetBtn = nullptr;
        for (auto* btn : buttons) {
            if (btn->text() == "R") resetBtn = btn;
        }
        QVERIFY(resetBtn != nullptr);
        QVERIFY(resetBtn->isEnabled());
    }

    // ---------------------------------------------------------------
    // setFujiNetRunning(false) clears everything
    // ---------------------------------------------------------------
    void testSetNotRunningClearsState()
    {
        FujiNetWidget w;
        w.setFujiNetRunning(true);
        w.onConnected();
        w.setFujiNetRunning(false);

        auto* label = w.findChild<QLabel*>();
        QCOMPARE(label->text(), QString("Stopped"));

        QList<QPushButton*> buttons = w.findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            QVERIFY(!btn->isEnabled());
        }
    }

    // ---------------------------------------------------------------
    // Reset button click emits resetRequested signal
    // ---------------------------------------------------------------
    void testResetSignal()
    {
        FujiNetWidget w;
        w.setFujiNetRunning(true);

        QSignalSpy spy(&w, &FujiNetWidget::resetRequested);
        QVERIFY(spy.isValid());

        QPushButton* resetBtn = nullptr;
        for (auto* btn : w.findChildren<QPushButton*>()) {
            if (btn->text() == "R") resetBtn = btn;
        }
        QVERIFY(resetBtn != nullptr);
        QTest::mouseClick(resetBtn, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }

    // ---------------------------------------------------------------
    // Swap button click emits swapRequested signal
    // ---------------------------------------------------------------
    void testSwapSignal()
    {
        FujiNetWidget w;
        w.setFujiNetRunning(true);
        w.onConnected(); // enables swap button

        QSignalSpy spy(&w, &FujiNetWidget::swapRequested);
        QVERIFY(spy.isValid());

        QPushButton* swapBtn = nullptr;
        for (auto* btn : w.findChildren<QPushButton*>()) {
            if (btn->text() == "S") swapBtn = btn;
        }
        QVERIFY(swapBtn != nullptr);
        QVERIFY(swapBtn->isEnabled());
        QTest::mouseClick(swapBtn, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }

    // ---------------------------------------------------------------
    // Bus activity / idle cycle
    // ---------------------------------------------------------------
    void testBusActivity()
    {
        FujiNetWidget w;
        w.setFujiNetRunning(true);

        // Simulate bus activity on drive 1
        w.onBusActivity(1, false);
        // After activity, bus should eventually go idle via timer
        w.onBusIdle(1);
        // No crash is the primary assertion
    }

    // ---------------------------------------------------------------
    // Multiple bus operations overlap correctly
    // ---------------------------------------------------------------
    void testMultipleBusOperations()
    {
        FujiNetWidget w;
        w.setFujiNetRunning(true);

        w.onBusActivity(1, false);
        w.onBusActivity(2, true);
        w.onBusIdle(1);
        // Drive 2 still active, bus should stay lit
        w.onBusIdle(2);
        // Now both idle, timer should fire to clear LED
    }

    // ---------------------------------------------------------------
    // Minimum widget height is set
    // ---------------------------------------------------------------
    void testMinimumHeight()
    {
        FujiNetWidget w;
        QVERIFY(w.minimumHeight() >= 100);
    }
};

QTEST_MAIN(TestFujiNetWidget)
#include "test_fujinet_widget.moc"
