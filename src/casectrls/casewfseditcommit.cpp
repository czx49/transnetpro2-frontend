#include "casewfseditcommit.h"
#include "offlineapi.h"

// #include "casebuslinetypedialog.h" // 线路类型弹窗暂禁用
// #include "casewaylayerhelper.h"    // 与上同步暂不用

#include "../httpapis/httpapis.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QSet>
#include <QVariantMap>

#include <qgsdatasourceuri.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsjsonutils.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayereditbuffer.h>

namespace {

const QString kPanelWfsProp = QStringLiteral("tnp_panel_wfs");

const QStringList kPrimaryKeyFieldCandidates = {
    QStringLiteral("id"),
    QStringLiteral("fid"),
    QStringLiteral("gid"),
    QStringLiteral("objectid"),
    QStringLiteral("link_id"),
    QStringLiteral("area_id")
};

struct PrimaryKeyValue {
    QString fieldName;
    QVariant value;
};

bool typeNameEndsWithLayerSuffix(const QString& typeName, const QString& suffix)
{
    const QString normalizedTypeName = typeName.trimmed().toLower();
    const QString normalizedSuffix = suffix.trimmed().toLower();
    return normalizedTypeName == normalizedSuffix || normalizedTypeName.endsWith(QLatin1Char('_') + normalizedSuffix);
}

QString businessPrimaryKeyFieldForTypeName(const QString& typeName)
{
    static const struct {
        const char* layerSuffix;
        const char* primaryKeyField;
    } kBusinessPrimaryKeys[] = {
        {"slow_road_point", "node_id"},
        {"road_point", "node_id"},
        {"bus_point", "node_id"},
        {"slow_road_way", "link_id"},
        {"road_way", "link_id"},
        {"bus_way", "link_id"},
    };

    for (const auto& item : kBusinessPrimaryKeys) {
        if (typeNameEndsWithLayerSuffix(typeName, QString::fromLatin1(item.layerSuffix))) {
            return QString::fromLatin1(item.primaryKeyField);
        }
    }

    if (typeNameEndsWithLayerSuffix(typeName, QStringLiteral("community"))) {
        return QStringLiteral("area_id");
    }

    return {};
}

QString typeNameFromLayer(QgsVectorLayer* layer)
{
    if (!layer || !layer->dataProvider()) {
        return {};
    }
    QgsDataSourceUri uri(layer->dataProvider()->dataSourceUri());
    QString typeName = uri.param(QStringLiteral("typename"));
    if (typeName.isEmpty()) {
        typeName = uri.table();
    }
    return typeName.trimmed();
}

QVariantMap geoJsonFeatureVariant(QgsVectorLayer* layer, const QgsFeature& feature)
{
    QgsJsonExporter exporter(layer);
    const QString json = exporter.exportFeature(feature);
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    return doc.object().toVariantMap();
}

bool isUsablePropertyValue(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return false;
    }
    if (value.userType() == QMetaType::QString && value.toString().isEmpty()) {
        return false;
    }
    return true;
}

QVariantMap sanitizeProperties(const QVariantMap& properties)
{
    QVariantMap sanitized;
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        if (isUsablePropertyValue(it.value())) {
            sanitized.insert(it.key(), it.value());
        }
    }
    return sanitized;
}

/** @brief 转为服务端 REST 期望的 { geometry, properties }，而非完整 GeoJSON Feature */
QVariantMap toApiFeaturePayload(const QVariantMap& geoJsonFeature)
{
    QVariantMap payload;
    const QVariant geometry = geoJsonFeature.value(QStringLiteral("geometry"));
    if (geometry.isValid() && !geometry.isNull()) {
        payload.insert(QStringLiteral("geometry"), geometry);
    }

    const QVariantMap properties = sanitizeProperties(geoJsonFeature.value(QStringLiteral("properties")).toMap());
    if (!properties.isEmpty()) {
        payload.insert(QStringLiteral("properties"), properties);
    }
    return payload;
}

QVariantMap buildInsertFeaturePayload(QgsVectorLayer* layer, const QgsFeature& feature)
{
    return toApiFeaturePayload(geoJsonFeatureVariant(layer, feature));
}

QVariantMap buildUpdateFeaturePayload(QgsVectorLayer* layer, QgsFeatureId fid, const QgsVectorLayerEditBuffer* buffer)
{
    QVariantMap payload;

    if (buffer->isFeatureGeometryChanged(fid)) {
        const QgsGeometry geometry = buffer->changedGeometries().value(fid);
        if (!geometry.isNull()) {
            const QJsonDocument doc = QJsonDocument::fromJson(geometry.asJson().toUtf8());
            if (doc.isObject()) {
                payload.insert(QStringLiteral("geometry"), doc.object().toVariantMap());
            }
        }
    }

    if (buffer->isFeatureAttributesChanged(fid)) {
        const QgsAttributeMap attrs = buffer->changedAttributeValues().value(fid);
        QVariantMap properties;
        const QgsFields fields = layer->fields();
        for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it) {
            if (it.key() < 0 || it.key() >= fields.count()) {
                continue;
            }
            const QVariant value = it.value();
            if (!isUsablePropertyValue(value)) {
                continue;
            }
            properties.insert(fields.at(it.key()).name(), value);
        }
        if (!properties.isEmpty()) {
            payload.insert(QStringLiteral("properties"), properties);
        }
    }

    return payload;
}

bool isUsablePrimaryKeyValue(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return false;
    }
    if (value.userType() == QMetaType::QString) {
        return !value.toString().trimmed().isEmpty();
    }
    return true;
}

QgsFeature providerFeatureById(QgsVectorLayer* layer, QgsFeatureId fid)
{
    QgsFeature feature;
    if (!layer || !layer->dataProvider()) {
        return feature;
    }

    QgsFeatureRequest request(fid);
    QgsFeatureIterator it = layer->dataProvider()->getFeatures(request);
    it.nextFeature(feature);
    return feature;
}

int caseInsensitiveFieldIndex(const QgsFields& fields, const QString& fieldName)
{
    int idx = fields.indexFromName(fieldName);
    if (idx >= 0) {
        return idx;
    }

    for (int i = 0; i < fields.count(); ++i) {
        if (fields.at(i).name().compare(fieldName, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

PrimaryKeyValue primaryKeyValueFromField(QgsVectorLayer* layer, const QgsFeature& feature, const QString& fieldName)
{
    PrimaryKeyValue result;
    if (!layer || fieldName.trimmed().isEmpty()) {
        return result;
    }

    const QgsFields fields = layer->fields();
    const int idx = caseInsensitiveFieldIndex(fields, fieldName);
    if (idx < 0) {
        return result;
    }

    const QVariant value = feature.attribute(idx);
    if (isUsablePrimaryKeyValue(value)) {
        result.fieldName = fields.at(idx).name();
        result.value = value;
    }
    return result;
}

PrimaryKeyValue resolvePrimaryKeyValue(QgsVectorLayer* layer, QgsFeatureId fid, const QString& typeName)
{
    PrimaryKeyValue result;
    if (!layer) {
        return result;
    }

    QgsFeature feature = layer->getFeature(fid);
    if (!feature.isValid()) {
        feature = providerFeatureById(layer, fid);
    }
    if (!feature.isValid()) {
        return result;
    }

    const QgsFields fields = layer->fields();
    result = primaryKeyValueFromField(layer, feature, businessPrimaryKeyFieldForTypeName(typeName));
    if (!result.fieldName.isEmpty()) {
        return result;
    }

    const QgsAttributeList pkIndexes = layer->dataProvider()
                                           ? layer->dataProvider()->pkAttributeIndexes()
                                           : QgsAttributeList{};
    for (int idx : pkIndexes) {
        if (idx < 0 || idx >= fields.count()) {
            continue;
        }
        const QVariant value = feature.attribute(idx);
        if (isUsablePrimaryKeyValue(value)) {
            result.fieldName = fields.at(idx).name();
            result.value = value;
            return result;
        }
    }

    for (const QString& fieldName : kPrimaryKeyFieldCandidates) {
        result = primaryKeyValueFromField(layer, feature, fieldName);
        if (!result.fieldName.isEmpty()) {
            return result;
        }
    }

    return result;
}

bool parseTransactionResponse(const QByteArray& data, QString* errorOut)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"服务端返回非 JSON 响应。");
        }
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("success")).toBool()) {
        const QJsonObject tx = root.value(QStringLiteral("transactionResponse")).toObject();
        const QJsonArray errors = tx.value(QStringLiteral("errors")).toArray();
        if (!errors.isEmpty()) {
            QStringList parts;
            for (const QJsonValue& value : errors) {
                if (value.isObject()) {
                    const QJsonObject errObj = value.toObject();
                    const QString operation = errObj.value(QStringLiteral("operation")).toString();
                    const QString error = errObj.value(QStringLiteral("error")).toString();
                    if (!operation.isEmpty() && !error.isEmpty()) {
                        parts.append(QStringLiteral("%1: %2").arg(operation, error));
                    } else if (!error.isEmpty()) {
                        parts.append(error);
                    } else {
                        parts.append(QString::fromUtf8(QJsonDocument(errObj).toJson(QJsonDocument::Compact)));
                    }
                } else {
                    parts.append(value.toString());
                }
            }
            if (errorOut) {
                *errorOut = parts.join(QStringLiteral("; "));
            }
            return false;
        }
        return true;
    }

    if (errorOut) {
        const QString message = root.value(QStringLiteral("message")).toString();
        *errorOut = message.isEmpty() ? QString::fromUtf8(u8"WFS 事务保存失败。") : message;
    }
    return false;
}

void clearLocalEditBuffer(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }
    if (layer->isEditable()) {
        layer->rollBack();
    }
    if (layer->dataProvider()) {
        layer->dataProvider()->reloadData();
    }
    layer->triggerRepaint();
}

} // namespace

namespace CaseWfsEditCommit {

bool isPanelWfsLayer(QgsVectorLayer* layer)
{
    return layer && layer->customProperty(kPanelWfsProp).toBool()
        && (OfflineApi::enabled() || layer->providerType().compare(QStringLiteral("WFS"), Qt::CaseInsensitive) == 0);
}

bool commitLayerEdits(QgsVectorLayer* layer, const QString& authToken, QString* errorOut)
{
    if(OfflineApi::enabled() && layer && layer->providerType()==QStringLiteral("postgres")) {
        if(!layer->isEditable() || !layer->isModified())return true;
        if(layer->commitChanges())return true;
        if(errorOut)*errorOut=layer->commitErrors().join(QStringLiteral("\n"));return false;
    }
    if (!layer || !layer->isEditable() || !layer->isModified()) {
        return true;
    }

    const QString typeName = typeNameFromLayer(layer);
    if (typeName.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"无法解析 WFS 图层 typename。");
        }
        return false;
    }

    QgsVectorLayerEditBuffer* buffer = layer->editBuffer();
    if (!buffer) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层编辑缓冲不可用。");
        }
        return false;
    }

    QVariantList inserts;
    QVariantList updates;
    QVariantList deletes;

    for (QgsFeatureId fid : buffer->deletedFeatureIds()) {
        if (buffer->isFeatureAdded(fid)) {
            continue;
        }
        const PrimaryKeyValue pk = resolvePrimaryKeyValue(layer, fid, typeName);
        deletes.append(HttpApis::buildWfsBatchDeleteItem(typeName, QString::number(fid), pk.fieldName, pk.value));
    }

    const QgsFeatureMap added = buffer->addedFeatures();
    QList<QVariantMap> pendingInsertFeatures;
    for (auto it = added.constBegin(); it != added.constEnd(); ++it) {
        if (buffer->isFeatureDeleted(it.key())) {
            continue;
        }
        const QVariantMap featureJson = buildInsertFeaturePayload(layer, it.value());
        if (featureJson.isEmpty()) {
            continue;
        }
        pendingInsertFeatures.append(featureJson);
    }

    QString busType;
    // 暂跳过公交道路新增时的「线路类型」弹窗，直接走 WFS 保存（不传 busType）。
    // const bool isBusWayLayer = CaseWayLayer::wayLayerValFromTypeName(typeName)
    //                                .compare(QStringLiteral("bus_way"), Qt::CaseInsensitive)
    //                            == 0;
    // if (isBusWayLayer && !pendingInsertFeatures.isEmpty()) {
    //     if (!CaseBusLineTypeDialog::execSelect(&busType)) {
    //         if (errorOut) {
    //             *errorOut = QString::fromUtf8(u8"已取消保存：未选择新增公交线路类型。");
    //         }
    //         return false;
    //     }
    // }

    for (const QVariantMap& featureJson : pendingInsertFeatures) {
        QVariantMap item;
        item.insert(QStringLiteral("typeName"), typeName);
        if (!busType.isEmpty()) {
            item.insert(QStringLiteral("busType"), busType);
        }
        item.insert(QStringLiteral("feature"), featureJson);
        inserts.append(item);
    }

    QSet<QgsFeatureId> updateIds;
    for (auto it = buffer->changedAttributeValues().constBegin(); it != buffer->changedAttributeValues().constEnd(); ++it) {
        updateIds.insert(it.key());
    }
    for (auto it = buffer->changedGeometries().constBegin(); it != buffer->changedGeometries().constEnd(); ++it) {
        updateIds.insert(it.key());
    }

    for (QgsFeatureId fid : std::as_const(updateIds)) {
        if (buffer->isFeatureAdded(fid) || buffer->isFeatureDeleted(fid)) {
            continue;
        }
        const QVariantMap featureJson = buildUpdateFeaturePayload(layer, fid, buffer);
        if (featureJson.isEmpty()) {
            continue;
        }
        const PrimaryKeyValue pk = resolvePrimaryKeyValue(layer, fid, typeName);
        updates.append(HttpApis::buildWfsBatchUpdateItem(typeName, QString::number(fid), featureJson, pk.fieldName,
                                                         pk.value));
    }

    if (inserts.isEmpty() && updates.isEmpty() && deletes.isEmpty()) {
        clearLocalEditBuffer(layer);
        return true;
    }

    QVariantMap body;
    if (!inserts.isEmpty()) {
        body.insert(QStringLiteral("inserts"), inserts);
    }
    if (!updates.isEmpty()) {
        body.insert(QStringLiteral("updates"), updates);
    }
    if (!deletes.isEmpty()) {
        body.insert(QStringLiteral("deletes"), deletes);
    }

    bool httpOk = false;
    QByteArray responseData;
    QString transportError;
    QEventLoop loop;
    HttpApis::wfsBatchTransactionJson(body, authToken,
        [&](bool suc, const QByteArray& data, const QString& msg) {
            httpOk = suc;
            responseData = data;
            transportError = msg;
            loop.quit();
        });
    loop.exec();

    if (!httpOk) {
        if (errorOut) {
            *errorOut = transportError.isEmpty()
                            ? QString::fromUtf8(u8"WFS 事务请求失败（网络或 HTTP 错误）。")
                            : transportError;
        }
        return false;
    }

    QString businessError;
    if (!parseTransactionResponse(responseData, &businessError)) {
        if (errorOut) {
            *errorOut = businessError;
        }
        return false;
    }

    clearLocalEditBuffer(layer);
    return true;
}

} // namespace CaseWfsEditCommit
