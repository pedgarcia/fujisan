/*
 * Fujisan Test Suite — TCP JSON API (FujisanClient / FastBasic debugger paths)
 *
 * Spins up a real MainWindow (hidden), TCP server on an ephemeral port, and a
 * local QTcpSocket client. Covers protocol errors plus commands used by
 * vscode-fastbasic-debugger (fujisanClient.ts): status.get_state, config.set_hard_drive,
 * media.load_xex, debug.load_xex_for_debug, system.get_speed, and input.send_text validation.
 * One hidden MainWindow is shared across all slots (avoids repeated libatari800 teardown).
 */

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "mainwindow.h"
#include "tcpserver.h"

class TestTcpCommands : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;
    QByteArray m_readBuffer;
    MainWindow* m_mainWindow = nullptr;
    TCPServer* m_tcp = nullptr;
    QTcpSocket* m_socket = nullptr;
    quint16 m_port = 0;

    void drainConnectedEvent()
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 15000) {
            QCoreApplication::processEvents();
            if (m_socket->waitForReadyRead(50) || m_socket->bytesAvailable() > 0) {
                m_readBuffer += m_socket->readAll();
            }
            while (true) {
                const int nl = m_readBuffer.indexOf('\n');
                if (nl < 0) {
                    break;
                }
                QByteArray line = m_readBuffer.left(nl);
                m_readBuffer.remove(0, nl + 1);
                QJsonParseError err;
                const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
                if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                    continue;
                }
                const QJsonObject o = doc.object();
                if (o.value(QStringLiteral("type")).toString() == QStringLiteral("event")
                    && o.value(QStringLiteral("event")).toString() == QStringLiteral("connected")) {
                    const QJsonObject data = o.value(QStringLiteral("data")).toObject();
                    QVERIFY(data.contains(QStringLiteral("capabilities")));
                    const QJsonArray caps = data.value(QStringLiteral("capabilities")).toArray();
                    bool hasDebug = false;
                    for (const QJsonValue& v : caps) {
                        if (v.toString() == QStringLiteral("debug")) {
                            hasDebug = true;
                            break;
                        }
                    }
                    QVERIFY2(hasDebug, "connected event should advertise debug capability");
                    return;
                }
            }
        }
        QFAIL("Timed out waiting for connected event");
    }

    QJsonObject waitForResponse(const QString& id, int timeoutMs = 20000)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            QCoreApplication::processEvents();
            if (m_socket->waitForReadyRead(50) || m_socket->bytesAvailable() > 0) {
                m_readBuffer += m_socket->readAll();
            }
            while (true) {
                const int nl = m_readBuffer.indexOf('\n');
                if (nl < 0) {
                    break;
                }
                QByteArray line = m_readBuffer.left(nl);
                m_readBuffer.remove(0, nl + 1);
                QJsonParseError err;
                const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
                if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                    continue;
                }
                const QJsonObject o = doc.object();
                if (o.value(QStringLiteral("type")).toString() == QStringLiteral("response")
                    && o.value(QStringLiteral("id")).toString() == id) {
                    return o;
                }
            }
        }
        return QJsonObject();
    }

    QJsonObject sendCommand(const QString& command, const QString& id,
                            const QJsonObject& params = QJsonObject())
    {
        QJsonObject req;
        req[QStringLiteral("command")] = command;
        req[QStringLiteral("id")] = id;
        if (!params.isEmpty()) {
            req[QStringLiteral("params")] = params;
        }
        m_socket->write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
        m_socket->flush();
        return waitForResponse(id);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("8bitrelics"));
        QCoreApplication::setApplicationName(QStringLiteral("Fujisan"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tempDir.path());

        {
            QSettings s;
            s.setValue(QStringLiteral("emulator/tcpServerEnabled"), false);
            s.sync();
        }

        m_mainWindow = new MainWindow();
        m_mainWindow->hide();

        m_tcp = m_mainWindow->findChild<TCPServer*>();
        QVERIFY(m_tcp != nullptr);

        if (m_tcp->isRunning()) {
            m_tcp->stopServer();
        }
        QVERIFY(m_tcp->startServer(0));
        m_port = m_tcp->serverPort();
        QVERIFY(m_port > 0);

        m_socket = new QTcpSocket();
        m_socket->connectToHost(QHostAddress::LocalHost, m_port);
        {
            QElapsedTimer connTimer;
            connTimer.start();
            while (m_socket->state() != QAbstractSocket::ConnectedState && connTimer.elapsed() < 20000) {
                QCoreApplication::processEvents();
                if (m_socket->waitForConnected(50)) {
                    break;
                }
            }
            QVERIFY2(m_socket->state() == QAbstractSocket::ConnectedState,
                     "Client could not connect to TCP server");
        }

        drainConnectedEvent();
    }

    void cleanupTestCase()
    {
        if (m_socket) {
            m_socket->disconnectFromHost();
            if (m_socket->state() != QAbstractSocket::UnconnectedState) {
                m_socket->waitForDisconnected(2000);
            }
            delete m_socket;
            m_socket = nullptr;
        }
        if (m_tcp && m_tcp->isRunning()) {
            m_tcp->stopServer();
        }
        delete m_mainWindow;
        m_mainWindow = nullptr;
        m_tcp = nullptr;
    }

    void init()
    {
        m_readBuffer.clear();
    }

    void testStatusGetState()
    {
        const QJsonObject resp = sendCommand(QStringLiteral("status.get_state"), QStringLiteral("s1"));
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("success"));
        const QJsonObject res = resp.value(QStringLiteral("result")).toObject();
        QVERIFY(res.contains(QStringLiteral("running")));
        QVERIFY(res.contains(QStringLiteral("pc")));
        QCOMPARE(res.value(QStringLiteral("server_port")).toInt(), static_cast<int>(m_port));
    }

    void testSystemGetSpeed()
    {
        const QJsonObject resp = sendCommand(QStringLiteral("system.get_speed"), QStringLiteral("g1"));
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("success"));
        const QJsonObject res = resp.value(QStringLiteral("result")).toObject();
        QVERIFY(res.contains(QStringLiteral("speed")) || res.contains(QStringLiteral("percentage")));
    }

    void testMissingCommand()
    {
        QJsonObject req;
        req[QStringLiteral("id")] = QStringLiteral("bad1");
        m_socket->write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
        m_socket->flush();
        const QJsonObject resp = waitForResponse(QStringLiteral("bad1"));
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
    }

    void testInvalidCommandFormat()
    {
        const QJsonObject resp = sendCommand(QStringLiteral("nope"), QStringLiteral("bad2"));
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
    }

    void testUnknownCategory()
    {
        const QJsonObject resp = sendCommand(QStringLiteral("nope.cmd"), QStringLiteral("bad3"));
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
    }

    void testInputSendTextEmpty()
    {
        QJsonObject params;
        params[QStringLiteral("text")] = QString();
        const QJsonObject resp = sendCommand(QStringLiteral("input.send_text"), QStringLiteral("i1"), params);
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
    }

    void testMediaLoadXexMissingFile()
    {
        QJsonObject params;
        params[QStringLiteral("path")] = m_tempDir.path() + QStringLiteral("/does-not-exist.xex");
        const QJsonObject resp = sendCommand(QStringLiteral("media.load_xex"), QStringLiteral("x1"), params);
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
    }

    void testDebugLoadXexForDebugMissingFile()
    {
        QJsonObject params;
        params[QStringLiteral("path")] = m_tempDir.path() + QStringLiteral("/missing-debug.xex");
        const QJsonObject resp =
            sendCommand(QStringLiteral("debug.load_xex_for_debug"), QStringLiteral("d1"), params);
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
    }

    void testConfigSetHardDriveH4()
    {
        const QString hdir = m_tempDir.path() + QStringLiteral("/h4host");
        QVERIFY(QDir().mkpath(hdir));

        QJsonObject params;
        params[QStringLiteral("drive")] = 4;
        params[QStringLiteral("path")] = hdir;
        const QJsonObject resp = sendCommand(QStringLiteral("config.set_hard_drive"), QStringLiteral("h4"), params);
        QVERIFY(!resp.isEmpty());
        QCOMPARE(resp.value(QStringLiteral("status")).toString(), QStringLiteral("success"));
        const QJsonObject res = resp.value(QStringLiteral("result")).toObject();
        QCOMPARE(res.value(QStringLiteral("drive")).toInt(), 4);
        QVERIFY(res.value(QStringLiteral("path")).toString().contains(QStringLiteral("h4host")));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestTcpCommands test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_tcp_commands.moc"
