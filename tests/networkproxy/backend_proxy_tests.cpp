#include "backendproxyfactory.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest>

class BackendProxyTests : public QObject {
    Q_OBJECT

private slots:
    void cleanup()
    {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
    }

    void backendBypassesBrokenProxy()
    {
        QTcpServer backend;
        QTcpServer proxy;
        QVERIFY(backend.listen(QHostAddress::LocalHost));
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        connect(&backend, &QTcpServer::newConnection, &backend, [&backend]() {
            QTcpSocket* socket = backend.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
                socket->disconnectFromHost();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        const QNetworkProxy brokenProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), proxy.serverPort());
        const QUrl backendUrl(QStringLiteral("http://127.0.0.1:%1").arg(backend.serverPort()));
        QNetworkProxyFactory::setApplicationProxyFactory(
            new BackendProxyFactory(backendUrl, [brokenProxy](const QNetworkProxyQuery&) {
                return QList<QNetworkProxy>{brokenProxy};
            }));
        QNetworkAccessManager manager;
        QNetworkReply* reply = manager.get(QNetworkRequest(backendUrl.resolved(QUrl("/api/config/getversion"))));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(4000));
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        QCOMPARE(reply->readAll(), QByteArray("OK"));
        QVERIFY(!proxy.hasPendingConnections());
        reply->deleteLater();
    }

    void unrelatedHostStillUsesProxy()
    {
        QTcpServer proxy;
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&proxy]() {
            QTcpSocket* socket = proxy.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
                socket->write("HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        const QNetworkProxy brokenProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), proxy.serverPort());
        QNetworkProxyFactory::setApplicationProxyFactory(
            new BackendProxyFactory(QUrl("http://backend.example"), [brokenProxy](const QNetworkProxyQuery&) {
                return QList<QNetworkProxy>{brokenProxy};
            }));
        QNetworkAccessManager manager;
        // A suffix-sharing host must not accidentally inherit the backend bypass.
        QNetworkReply* reply = manager.get(QNetworkRequest(QUrl("http://backend.example.invalid/api")));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(4000));
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 502);
        reply->deleteLater();
    }

    void liveBackend()
    {
        const QUrl backendUrl(qEnvironmentVariable("TNP_LIVE_BACKEND"));
        if (backendUrl.isEmpty()) {
            QSKIP("Set TNP_LIVE_BACKEND to run the public API integration check.");
        }
        QNetworkProxyFactory::setApplicationProxyFactory(new BackendProxyFactory(backendUrl));
        QNetworkAccessManager manager;
        for (const QString& route : {QStringLiteral("/api/config/getversion"), QStringLiteral("/api/config/base")}) {
            QNetworkRequest request(backendUrl.resolved(QUrl(route)));
            request.setTransferTimeout(10000);
            QNetworkReply* reply = manager.get(request);
            QSignalSpy finished(reply, &QNetworkReply::finished);
            QVERIFY(finished.wait(12000));
            QCOMPARE(reply->error(), QNetworkReply::NoError);
            QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
            const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
            QCOMPARE(body.value(QStringLiteral("code")).toInt(), 200);
            reply->deleteLater();
        }

        // Empty credentials exercise the login route without attempting a real account.
        QNetworkRequest loginRequest(backendUrl.resolved(QUrl("/api/user/login")));
        loginRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        loginRequest.setTransferTimeout(10000);
        QNetworkReply* reply = manager.post(loginRequest, "{\"username\":\"\",\"password\":\"\"}");
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(12000));
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QVERIFY(status >= 200 && status < 500);
        const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        QVERIFY(!body.isEmpty());
        QVERIFY(body.contains(QStringLiteral("code")));
        qInfo() << "Login route reached backend validation, HTTP" << status
                << "business code" << body.value(QStringLiteral("code")).toInt();
        reply->deleteLater();
    }
};

QTEST_GUILESS_MAIN(BackendProxyTests)
#include "backend_proxy_tests.moc"
