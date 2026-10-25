#pragma once

#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QUrl>
#include <functional>
#include <utility>

// Backend requests use a direct route; other destinations retain their proxy policy.
class BackendProxyFactory final : public QNetworkProxyFactory {
public:
    using ProxyResolver = std::function<QList<QNetworkProxy>(const QNetworkProxyQuery&)>;

    explicit BackendProxyFactory(
        const QUrl& backendUrl,
        ProxyResolver fallback = QNetworkProxyFactory::systemProxyForQuery)
        : m_backendHost(backendUrl.host())
        , m_fallback(std::move(fallback))
    {
    }

    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery& query) override
    {
        const QUrl url = query.url();
        const bool isHttp = url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
            || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
        if (isHttp && !m_backendHost.isEmpty()
            && url.host().compare(m_backendHost, Qt::CaseInsensitive) == 0) {
            return {QNetworkProxy(QNetworkProxy::NoProxy)};
        }
        return m_fallback(query);
    }

private:
    QString m_backendHost;
    ProxyResolver m_fallback;
};
