#pragma once

#include "qgsnetworkaccessmanager.h"
#include <QNetworkRequest>
#include <QUrl>

namespace QgisBackendProxy {

inline QString install(const QUrl& backendUrl)
{
    // QGIS 3.40 checks system proxies before inserted proxy factories. Its
    // noProxyList is honored by the native factory, including in worker NAMs.
    return QgsNetworkAccessManager::setRequestPreprocessor(
        [backendUrl](QNetworkRequest* request) {
            const QUrl url = request->url();
            const bool isHttp = url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
                || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
            if (!isHttp || backendUrl.host().isEmpty()
                || url.host().compare(backendUrl.host(), Qt::CaseInsensitive) != 0) {
                return;
            }
            auto* manager = QgsNetworkAccessManager::instance();
            QStringList noProxyUrls = manager->noProxyList();
            const QString origin = url.adjusted(
                QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo).toString()
                + QStringLiteral("/");
            if (!noProxyUrls.contains(origin)) {
                noProxyUrls.append(origin);
                manager->setFallbackProxyAndExcludes(
                    manager->fallbackProxy(), manager->excludeList(), noProxyUrls);
            }
        });
}

} // namespace QgisBackendProxy
