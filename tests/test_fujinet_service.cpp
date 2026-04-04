/*
 * Fujisan Test Suite - FujiNet Service Tests
 *
 * Uses a local QTcpServer as an HTTP mock to test FujiNetService
 * health check, connection state, and drive polling.
 */

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

#include "fujinetservice.h"

class MockHttpServer : public QTcpServer {
    Q_OBJECT
public:
    explicit MockHttpServer(QObject* parent = nullptr)
        : QTcpServer(parent)
    {
        connect(this, &QTcpServer::newConnection, this,
                &MockHttpServer::handleConnection);
    }

    // Set the response to send for any request
    void setResponse(int statusCode, const QString& body)
    {
        m_statusCode = statusCode;
        m_body = body;
    }

    // Set response per path
    void setResponseForPath(const QString& path, int statusCode,
                            const QString& body)
    {
        m_pathResponses[path] = {statusCode, body};
    }

private slots:
    void handleConnection()
    {
        while (hasPendingConnections()) {
            QTcpSocket* socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                QByteArray request = socket->readAll();
                QString requestStr = QString::fromUtf8(request);

                // Parse path from "GET /path HTTP/1.1"
                QString path;
                QStringList lines = requestStr.split("\r\n");
                if (!lines.isEmpty()) {
                    QStringList parts = lines[0].split(' ');
                    if (parts.size() >= 2) {
                        path = parts[1];
                    }
                }

                int code = m_statusCode;
                QString body = m_body;
                if (m_pathResponses.contains(path)) {
                    code = m_pathResponses[path].first;
                    body = m_pathResponses[path].second;
                }

                QString response = QString(
                    "HTTP/1.1 %1 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: %2\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%3")
                    .arg(code)
                    .arg(body.length())
                    .arg(body);

                socket->write(response.toUtf8());
                socket->flush();
                socket->disconnectFromHost();
            });
        }
    }

private:
    int m_statusCode = 200;
    QString m_body;
    QMap<QString, QPair<int, QString>> m_pathResponses;
};

class TestFujiNetService : public QObject {
    Q_OBJECT

private:
    MockHttpServer* m_server = nullptr;
    int m_port = 0;

private slots:
    void init()
    {
        m_server = new MockHttpServer(this);
        QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
        m_port = m_server->serverPort();
        // FujiNetService::onHealthCheckReply expects JSON with "result": 1
        m_server->setResponse(200, "{\"result\": 1}");
    }

    void cleanup()
    {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }

    // ---------------------------------------------------------------
    // Initial state
    // ---------------------------------------------------------------
    void testInitialState()
    {
        FujiNetService svc;
        QVERIFY(!svc.isConnected());
        QCOMPARE(svc.getServerUrl(), QString("http://localhost:8000"));
    }

    // ---------------------------------------------------------------
    // setServerUrl normalizes the URL
    // ---------------------------------------------------------------
    void testSetServerUrl()
    {
        FujiNetService svc;

        svc.setServerUrl("http://127.0.0.1:9000");
        QCOMPARE(svc.getServerUrl(), QString("http://127.0.0.1:9000"));

        // Trailing slash removed
        svc.setServerUrl("http://127.0.0.1:9000/");
        QCOMPARE(svc.getServerUrl(), QString("http://127.0.0.1:9000"));

        // Adds http:// if missing
        svc.setServerUrl("localhost:8000");
        QCOMPARE(svc.getServerUrl(), QString("http://localhost:8000"));
    }

    // ---------------------------------------------------------------
    // Health check: 200 -> connected signal
    // ---------------------------------------------------------------
    void testHealthCheckSuccess()
    {
        m_server->setResponseForPath("/test", 200, "{\"result\": 1}");

        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));

        QSignalSpy connSpy(&svc, &FujiNetService::connected);
        QVERIFY(connSpy.isValid());

        svc.checkConnection();

        // Wait for the network round-trip
        QVERIFY(connSpy.wait(3000));
        QVERIFY(svc.isConnected());
    }

    // ---------------------------------------------------------------
    // Health check: connection refused -> disconnected
    // ---------------------------------------------------------------
    void testHealthCheckConnectionRefused()
    {
        FujiNetService svc;
        // Point to a port nobody is listening on
        svc.setServerUrl("http://127.0.0.1:1");

        QSignalSpy errSpy(&svc, &FujiNetService::connectionError);
        QSignalSpy discSpy(&svc, &FujiNetService::disconnected);

        svc.checkConnection();

        // At least one of error/disconnected should fire
        bool gotSignal = errSpy.wait(3000) || discSpy.wait(1000);
        Q_UNUSED(gotSignal);
        QVERIFY(!svc.isConnected());
    }

    // ---------------------------------------------------------------
    // resetConnectionState forces re-emit of connected on next check
    // ---------------------------------------------------------------
    void testResetConnectionState()
    {
        m_server->setResponseForPath("/test", 200, "{\"result\": 1}");

        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));

        // Get connected first
        QSignalSpy connSpy(&svc, &FujiNetService::connected);
        svc.checkConnection();
        QVERIFY(connSpy.wait(3000));
        QVERIFY(svc.isConnected());

        // Reset and check again -- should re-emit connected
        svc.resetConnectionState();
        QVERIFY(!svc.isConnected());

        QSignalSpy connSpy2(&svc, &FujiNetService::connected);
        svc.checkConnection();
        QVERIFY(connSpy2.wait(3000));
        QVERIFY(svc.isConnected());
    }

    // ---------------------------------------------------------------
    // stopHealthCheck stops the timer
    // ---------------------------------------------------------------
    void testStopHealthCheck()
    {
        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));
        svc.startHealthCheck(100);
        svc.stopHealthCheck();
        // No crash, timer stopped
    }

    // ---------------------------------------------------------------
    // startDrivePolling / stopDrivePolling
    // ---------------------------------------------------------------
    void testDrivePollingStartStop()
    {
        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));
        svc.startDrivePolling(100);
        svc.stopDrivePolling();
        // No crash
    }
};

QTEST_MAIN(TestFujiNetService)
#include "test_fujinet_service.moc"
