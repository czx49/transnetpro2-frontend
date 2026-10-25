#include "qgisbackendproxy.h"
#include "qgsapplication.h"
#include "qgsfeatureiterator.h"
#include "qgsfeaturerequest.h"
#include "qgsproject.h"
#include "qgsvectorlayer.h"

#include <QEventLoop>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QtTest>

class QgisProxyTests : public QObject {
    Q_OBJECT
    QString m_preprocessor;

    static void serve(QTcpServer& server, const QByteArray& response)
    {
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, response]() {
            QTcpSocket* socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, response]() {
                socket->readAll();
                socket->write(response);
                socket->disconnectFromHost();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
    }

private slots:
    void cleanup()
    {
        if (!m_preprocessor.isEmpty()) {
            QgsNetworkAccessManager::removeRequestPreprocessor(m_preprocessor);
            m_preprocessor.clear();
        }
        QgsNetworkAccessManager::instance()->setFallbackProxyAndExcludes(QNetworkProxy(), {}, {});
    }

    void qgisBackendBypassesFallbackInMainAndWorkerThreads()
    {
        QTcpServer backend;
        QTcpServer proxy;
        QVERIFY(backend.listen(QHostAddress::LocalHost));
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        serve(backend, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
        serve(proxy, "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        const QUrl backendUrl(QStringLiteral("http://127.0.0.1:%1/wfs").arg(backend.serverPort()));
        const QNetworkProxy brokenProxy(QNetworkProxy::HttpProxy, "127.0.0.1", proxy.serverPort());
        auto* manager = QgsNetworkAccessManager::instance();
        manager->setFallbackProxyAndExcludes(brokenProxy, {}, {});
        m_preprocessor = QgisBackendProxy::install(backendUrl);

        QNetworkReply* reply = manager->get(QNetworkRequest(backendUrl));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        QCOMPARE(reply->readAll(), QByteArray("OK"));
        reply->deleteLater();

        int workerStatus = 0;
        QThread* worker = QThread::create([&]() {
            auto* workerManager = QgsNetworkAccessManager::instance();
            workerManager->setFallbackProxyAndExcludes(brokenProxy, {}, {});
            QNetworkReply* workerReply = workerManager->get(QNetworkRequest(backendUrl));
            QEventLoop loop;
            QObject::connect(workerReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);
            loop.exec();
            workerStatus = workerReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            delete workerReply;
        });
        QSignalSpy workerFinished(worker, &QThread::finished);
        worker->start();
        const bool completed = workerFinished.wait(7000);
        worker->wait();
        delete worker;
        QVERIFY(completed);
        QCOMPARE(workerStatus, 200);
        QVERIFY(!proxy.hasPendingConnections());

        reply = manager->get(QNetworkRequest(QUrl("http://unrelated.example.invalid/wfs")));
        QSignalSpy otherFinished(reply, &QNetworkReply::finished);
        QVERIFY(otherFinished.wait(5000));
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 502);
        reply->deleteLater();
    }

    void liveWfsLayer()
    {
        const QUrl backendUrl(qEnvironmentVariable("TNP_LIVE_BACKEND"));
        const QString typeName = qEnvironmentVariable("TNP_LIVE_TYPENAME");
        if (backendUrl.isEmpty() || typeName.isEmpty()) {
            QSKIP("Set TNP_LIVE_BACKEND and TNP_LIVE_TYPENAME to check the actual WFS provider.");
        }
        m_preprocessor = QgisBackendProxy::install(backendUrl);
        const QString uri = QStringLiteral("url='%1/wfs' typename='%2' version='2.0.0' srsname='EPSG:4326'")
            .arg(backendUrl.toString(QUrl::StripTrailingSlash), typeName);
        QgsVectorLayer layer(uri, QStringLiteral("WFS integration check"), QStringLiteral("WFS"));
        QVERIFY2(layer.isValid(), qPrintable(layer.error().message()));
        QVERIFY(layer.fields().count() > 0);
        QgsFeature feature;
        const bool hasFeature = layer.getFeatures(QgsFeatureRequest().setLimit(1)).nextFeature(feature);
        QVERIFY(hasFeature);
        qInfo() << "Actual WFS layer valid; fields:" << layer.fields().count() << "; first feature read:" << hasFeature;
    }
};

int main(int argc, char** argv)
{
    QgsApplication app(argc, argv, false);
    const QString prefix = qEnvironmentVariable("TNP_QGIS_PREFIX");
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + "/plugins");
    QgsApplication::setPkgDataPath(prefix);
    QgsApplication::initQgis();
    QgisProxyTests test;
    const int result = QTest::qExec(&test, argc, argv);
    QgsProject::instance()->removeAllMapLayers();
    QgsApplication::exitQgis();
    return result;
}
#include "qgis_proxy_tests.moc"
