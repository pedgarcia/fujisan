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

Q_DECLARE_METATYPE(QVector<FujiNetDrive>)

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

    QStringList requestedPaths() const { return m_requestedPaths; }

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
                m_requestedPaths.append(path);

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
    QStringList m_requestedPaths;
};

// Abridged GET /api/v1/drives reply from FujiNet-PC (fujinet-firmware 26694b1).
static const char* kDrivesJson =
    "[{\"slot\":1,\"path\":\"/ANIMALS.XEX\",\"mounted\":true,\"mode\":\"r\",\"host_slot\":0,\"host\":\"SD\"},"
    "{\"slot\":2,\"path\":\"/GAMES/DEMO.ATR\",\"mounted\":false,\"mode\":\"w\",\"host_slot\":1,\"host\":\"fujinet.online\"},"
    "{\"slot\":3,\"path\":\"\",\"mounted\":false,\"mode\":\"r\",\"host_slot\":-1},"
    "{\"slot\":4,\"path\":\"/STALE.ATR\",\"mounted\":false,\"mode\":\"r\",\"host_slot\":255},"
    "{\"slot\":9,\"path\":\"/IGNORED.ATR\",\"mounted\":false,\"mode\":\"r\",\"host_slot\":0}]";

// The drive divs of the / status page for the same configuration.
static const char* kStatusPageHtml =
    "<div class=\"deth detlinecol\" data-mountslot=\"0\" data-mountslotname=\"Drive Slot 1\" data-mount=\"/ANIMALS.XEX (R)\"></div>"
    "<div class=\"deth detlinecol\" data-mountslot=\"1\" data-mountslotname=\"Drive Slot 2\" data-mount=\"/GAMES/DEMO.ATR (W)\"></div>"
    "<div class=\"deth detlinecol\" data-mountslot=\"2\" data-mountslotname=\"Drive Slot 3\" data-mount=\"(Empty)\"></div>";

static void verifyExpectedDrives(const QVector<FujiNetDrive>& drives)
{
    QCOMPARE(drives.size(), 8);
    for (int i = 0; i < drives.size(); i++) {
        QCOMPARE(drives[i].slotNumber, i);
    }

    QVERIFY(!drives[0].isEmpty);
    QCOMPARE(drives[0].filename, QString("/ANIMALS.XEX"));
    QVERIFY(drives[0].isReadOnly);

    QVERIFY(!drives[1].isEmpty);
    QCOMPARE(drives[1].filename, QString("/GAMES/DEMO.ATR"));
    QVERIFY(!drives[1].isReadOnly);

    for (int i = 2; i < drives.size(); i++) {
        QVERIFY(drives[i].isEmpty);
        QVERIFY(drives[i].filename.isEmpty());
    }
}

class TestFujiNetService : public QObject {
    Q_OBJECT

private:
    MockHttpServer* m_server = nullptr;
    int m_port = 0;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QVector<FujiNetDrive>>();
    }

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

    // ---------------------------------------------------------------
    // Drive status comes from /api/v1/drives; the / status page (which
    // lists the SD card on every render) is not requested.
    // ---------------------------------------------------------------
    void testDriveStatusUsesJsonApi()
    {
        m_server->setResponseForPath("/api/v1/drives", 200, kDrivesJson);
        m_server->setResponseForPath("/", 200, kStatusPageHtml);

        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));
        QSignalSpy spy(&svc, &FujiNetService::driveStatusUpdated);

        svc.queryDriveStatus();
        QVERIFY(spy.wait(3000));
        verifyExpectedDrives(spy.takeFirst().at(0).value<QVector<FujiNetDrive>>());

        svc.queryDriveStatus();
        QVERIFY(spy.wait(3000));
        QCOMPARE(m_server->requestedPaths(),
                 QStringList({"/api/v1/drives", "/api/v1/drives"}));
    }

    // ---------------------------------------------------------------
    // Older FujiNet-PC (no /api/v1/drives -> 404): fall back to the /
    // status page, stay on it, and re-probe after setServerUrl.
    // ---------------------------------------------------------------
    void testDriveStatusFallsBackToStatusPage()
    {
        m_server->setResponseForPath("/api/v1/drives", 404, "Not found");
        m_server->setResponseForPath("/", 200, kStatusPageHtml);

        FujiNetService svc;
        const QString url = QString("http://127.0.0.1:%1").arg(m_port);
        svc.setServerUrl(url);
        QSignalSpy spy(&svc, &FujiNetService::driveStatusUpdated);
        QSignalSpy errSpy(&svc, &FujiNetService::connectionError);

        svc.queryDriveStatus();
        QVERIFY(spy.wait(3000));
        verifyExpectedDrives(spy.takeFirst().at(0).value<QVector<FujiNetDrive>>());
        QCOMPARE(errSpy.count(), 0);
        QCOMPARE(m_server->requestedPaths(), QStringList({"/api/v1/drives", "/"}));

        svc.queryDriveStatus();
        QVERIFY(spy.wait(3000));
        QCOMPARE(m_server->requestedPaths(), QStringList({"/api/v1/drives", "/", "/"}));

        svc.setServerUrl(url);
        svc.queryDriveStatus();
        QVERIFY(spy.wait(3000));
        QCOMPARE(m_server->requestedPaths(),
                 QStringList({"/api/v1/drives", "/", "/", "/api/v1/drives", "/"}));
    }

    // ---------------------------------------------------------------
    // A server error is not taken as "endpoint missing".
    // ---------------------------------------------------------------
    void testDriveStatusServerErrorKeepsJsonApi()
    {
        m_server->setResponseForPath("/api/v1/drives", 500, "{\"error\":\"out of memory\"}");

        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));
        QSignalSpy spy(&svc, &FujiNetService::driveStatusUpdated);
        QSignalSpy errSpy(&svc, &FujiNetService::connectionError);

        svc.queryDriveStatus();
        QVERIFY(errSpy.wait(3000));
        QCOMPARE(spy.count(), 0);

        m_server->setResponseForPath("/api/v1/drives", 200, kDrivesJson);
        svc.queryDriveStatus();
        QVERIFY(spy.wait(3000));
        verifyExpectedDrives(spy.takeFirst().at(0).value<QVector<FujiNetDrive>>());
        QCOMPARE(m_server->requestedPaths(),
                 QStringList({"/api/v1/drives", "/api/v1/drives"}));
    }

    // ---------------------------------------------------------------
    // A poll while the previous one is in flight is skipped.
    // ---------------------------------------------------------------
    void testDriveStatusSkipsOverlappingPolls()
    {
        m_server->setResponseForPath("/api/v1/drives", 200, kDrivesJson);

        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));
        QSignalSpy spy(&svc, &FujiNetService::driveStatusUpdated);

        svc.queryDriveStatus();
        svc.queryDriveStatus();
        QVERIFY(spy.wait(3000));
        QTest::qWait(200);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m_server->requestedPaths(), QStringList({"/api/v1/drives"}));
    }

    // ---------------------------------------------------------------
    // stopDrivePolling drops a reply that is still in flight.
    // ---------------------------------------------------------------
    void testStopDrivePollingDiscardsPendingReply()
    {
        m_server->setResponseForPath("/api/v1/drives", 200, kDrivesJson);

        FujiNetService svc;
        svc.setServerUrl(QString("http://127.0.0.1:%1").arg(m_port));
        QSignalSpy spy(&svc, &FujiNetService::driveStatusUpdated);

        svc.startDrivePolling(60000);
        svc.stopDrivePolling();
        QVERIFY(!spy.wait(500));
    }
};

QTEST_MAIN(TestFujiNetService)
#include "test_fujinet_service.moc"
