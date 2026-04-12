/*
 * Fujisan Test Suite — character injection / paste safety
 *
 * Ensures injectCharacter() can be called from the UI thread while the emulator
 * runs on a worker thread: it must not call libatari800_next_frame() off the
 * emulator thread (regression from moveToThread + synchronous frame stepping).
 */

#include <QCoreApplication>
#include <QMetaObject>
#include <QSettings>
#include <QThread>
#include <QTemporaryDir>
#include <QtWidgets/QApplication>
#include <QtTest/QtTest>

#include "atariemulator.h"

class TestCharacterInjection : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("8bitrelics"));
        QCoreApplication::setApplicationName(QStringLiteral("Fujisan"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tempDir.path());
    }

    void testInjectCharacterFromNonEmulatorThreadDoesNotCrash()
    {
        QThread thread;
        auto* emu = new AtariEmulator(nullptr);

        emu->moveToThread(&thread);
        thread.start();

        bool initOk = false;
        QMetaObject::invokeMethod(
            emu,
            [&initOk, emu]() {
                initOk = emu->initializeWithDisplayConfig(
                    true,
                    QStringLiteral("-xl"),
                    QStringLiteral("-pal"),
                    QStringLiteral("none"),
                    QStringLiteral("tv"),
                    QStringLiteral("tv"),
                    0,
                    0,
                    QStringLiteral("both"),
                    false,
                    false);
                if (initOk) {
                    emu->pauseEmulation();
                }
            },
            Qt::BlockingQueuedConnection);

        QVERIFY2(initOk, "libatari800 init failed (see build logs / ROM paths)");

        QVERIFY2(QThread::currentThread() != emu->thread(),
                 "Test requires emulator on a different thread than caller");

        emu->injectCharacter('A');

        int remaining = -1;
        QMetaObject::invokeMethod(
            emu,
            [&remaining, emu]() { remaining = emu->injectedKeyFramesRemainingForTest(); },
            Qt::BlockingQueuedConnection);
        QCOMPARE(remaining, 3);

        for (int i = 0; i < 3; ++i) {
            QMetaObject::invokeMethod(emu, "processFrame", Qt::BlockingQueuedConnection);
        }

        QMetaObject::invokeMethod(
            emu,
            [&remaining, emu]() { remaining = emu->injectedKeyFramesRemainingForTest(); },
            Qt::BlockingQueuedConnection);
        QCOMPARE(remaining, 0);

        QMetaObject::invokeMethod(emu, "shutdown", Qt::BlockingQueuedConnection);
        // Destroy on the emulator thread so QTimer children match the owning thread
        QMetaObject::invokeMethod(
            emu, [emu]() { delete emu; }, Qt::BlockingQueuedConnection);

        thread.quit();
        QVERIFY2(thread.wait(5000), "emulator thread must stop before scope exit");
    }

    void testInjectCharacterUnknownCharClearsPendingFrames()
    {
        QThread thread;
        auto* emu = new AtariEmulator(nullptr);
        emu->moveToThread(&thread);
        thread.start();

        bool initOk = false;
        QMetaObject::invokeMethod(
            emu,
            [&initOk, emu]() {
                initOk = emu->initializeWithDisplayConfig(
                    true,
                    QStringLiteral("-xl"),
                    QStringLiteral("-pal"),
                    QStringLiteral("none"),
                    QStringLiteral("tv"),
                    QStringLiteral("tv"),
                    0,
                    0,
                    QStringLiteral("both"),
                    false,
                    false);
                if (initOk) {
                    emu->pauseEmulation();
                }
            },
            Qt::BlockingQueuedConnection);
        QVERIFY(initOk);

        emu->injectCharacter('\x7F');  // not mapped — early return

        int remaining = -1;
        QMetaObject::invokeMethod(
            emu,
            [&remaining, emu]() { remaining = emu->injectedKeyFramesRemainingForTest(); },
            Qt::BlockingQueuedConnection);
        QCOMPARE(remaining, 0);

        QMetaObject::invokeMethod(emu, "shutdown", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(
            emu, [emu]() { delete emu; }, Qt::BlockingQueuedConnection);
        thread.quit();
        QVERIFY2(thread.wait(5000), "emulator thread must stop before scope exit");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestCharacterInjection test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_character_injection.moc"
