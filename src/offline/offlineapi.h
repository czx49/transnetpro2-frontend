#pragma once
#include "offlinestore.h"
#include <functional>

// Adapter for the original HTTP contract. Offline requests never reach the network.
class OfflineApi {
public:
    using Callback = std::function<void(bool, const QString&, const QByteArray&)>;
    static bool enabled();
    static bool initialize(QString& error);
    static void shutdown();
    static void dispatch(const QString& method, const QString& url, const QVariantMap& params,
                         const Callback& callback, const QString& file = QString());
    static QJsonObject execute(const QString& method, const QString& url,
                               const QVariantMap& params = {}, const QString& file = QString());
    static QStringList sourceLayers(const QString& file);
    static QJsonObject layerConnection(const QString& table, QString* error = nullptr);
};
