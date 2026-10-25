/*****************************************************************//**
 * \file   casewayfeatureaddhelper.cpp
 * \brief  路段图层添加要素属性预填实现
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#include "casewayfeatureaddhelper.h"

#include "basedatatableview.h"
#include "casemaplayerresolve.h"
#include "casetreemodel.h"
#include "casetreeview.h"
#include "casewaylayerhelper.h"
#include "casewfsmapcontroller.h"

#include "../tnmapwidget/tnmapwidget.h"

#include <QHash>
#include <QPointer>
#include <QtMath>

#include <qgsdistancearea.h>
#include <qgsfeature.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsspatialindex.h>
#include <qgsvectorlayer.h>
#include <qgswkbtypes.h>

namespace CaseWayFeatureAdd {

namespace {

constexpr double kNodeSearchBufferDeg = 0.0002;
constexpr double kFftSpeedMps = 33.0;

struct WayAddDefaults {
    double capacity = 1000.0;
    double b = 0.15;
    double power = 4.0;
    double toll = 0.0;
    int type = 1;
    double speedlimit = 80.0;
    double adjustNum = 1.0;
    int laneNum = 2;
};

struct NodeIndexCache {
    QPointer<QgsVectorLayer> layer;
    qlonglong featureCount = -1;
    bool valid = false;
    QgsSpatialIndex index;
    QHash<QgsFeatureId, QVariant> nodeIdByFid;
};

NodeIndexCache g_nodeIndexCache;

bool coerceVariantForField(const QgsField& field, const QVariant& value, QVariant* out)
{
    if (!out || !value.isValid()) {
        return false;
    }

    const QMetaType::Type fieldType = field.type();
    switch (fieldType) {
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong: {
        bool ok = false;
        const qlonglong v = value.toLongLong(&ok);
        if (!ok) {
            return false;
        }
        *out = v;
        return true;
    }
    case QMetaType::Double:
    case QMetaType::Float: {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (!ok) {
            return false;
        }
        *out = v;
        return true;
    }
    case QMetaType::QString:
        *out = value.toString();
        return true;
    default:
        *out = value;
        return true;
    }
}

bool setFieldIfExists(QgsFeature& feature, const QgsFields& fields, const QString& name, const QVariant& value)
{
    const int idx = fields.indexFromName(name);
    if (idx < 0) {
        return false;
    }

    QVariant coerced;
    if (!coerceVariantForField(fields.at(idx), value, &coerced)) {
        return false;
    }

    feature.setAttribute(idx, coerced);
    return true;
}

bool setFieldNullIfExists(QgsFeature& feature, const QgsFields& fields, const QString& name)
{
    const int idx = fields.indexFromName(name);
    if (idx < 0) {
        return false;
    }

    feature.setAttribute(idx, QVariant());
    return true;
}

QString resolveWayLayerVal(QgsVectorLayer* wayLayer)
{
    if (!wayLayer) {
        return {};
    }
    const QString typeName = CaseMapLayerResolve::vectorLayerWfsTypeName(wayLayer);
    return CaseWayLayer::wayLayerValFromTypeName(typeName);
}

QgsVectorLayer* resolvePointLayerOnMap(TNMapWidget* map, QgsVectorLayer* wayLayer)
{
    if (!map || !wayLayer) {
        return nullptr;
    }

    const QString wayTypeName = CaseMapLayerResolve::vectorLayerWfsTypeName(wayLayer).trimmed();
    const QString pointTypeName = CaseWayLayer::derivePointTypeName(wayTypeName);
    if (pointTypeName.isEmpty()) {
        return nullptr;
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(pointTypeName);
    if (uri.isEmpty()) {
        return nullptr;
    }

    return qobject_cast<QgsVectorLayer*>(map->getLayer(uri));
}

void ensureNodeIndex(QgsVectorLayer* pointLayer)
{
    if (!pointLayer || !pointLayer->isValid()) {
        g_nodeIndexCache = {};
        return;
    }

    const qlonglong count = pointLayer->featureCount();
    if (g_nodeIndexCache.layer.data() == pointLayer && g_nodeIndexCache.featureCount == count
        && g_nodeIndexCache.valid) {
        return;
    }

    g_nodeIndexCache.layer = pointLayer;
    g_nodeIndexCache.featureCount = count;
    g_nodeIndexCache.valid = false;
    g_nodeIndexCache.index = QgsSpatialIndex();
    g_nodeIndexCache.nodeIdByFid.clear();

    const int nodeIdIdx = pointLayer->fields().indexFromName(QStringLiteral("node_id"));
    if (nodeIdIdx < 0) {
        return;
    }

    QgsFeature feat;
    QgsFeatureIterator it = pointLayer->getFeatures();
    while (it.nextFeature(feat)) {
        if (!feat.hasGeometry()) {
            continue;
        }
        g_nodeIndexCache.index.addFeature(feat);
        g_nodeIndexCache.nodeIdByFid.insert(feat.id(), feat.attribute(nodeIdIdx));
    }
    g_nodeIndexCache.valid = !g_nodeIndexCache.nodeIdByFid.isEmpty();
}

bool findNearestNodeId(QgsVectorLayer* pointLayer, const QgsPointXY& point, QVariant* outNodeId)
{
    if (!pointLayer || !outNodeId) {
        return false;
    }

    ensureNodeIndex(pointLayer);
    if (!g_nodeIndexCache.valid) {
        return false;
    }

    const QgsGeometry searchGeom =
        QgsGeometry::fromPointXY(point).buffer(kNodeSearchBufferDeg, 5);
    if (searchGeom.isEmpty()) {
        return false;
    }

    const QgsRectangle rect = searchGeom.boundingBox();
    const QList<QgsFeatureId> candidates = g_nodeIndexCache.index.intersects(rect);
    if (candidates.isEmpty()) {
        return false;
    }

    double bestDist = std::numeric_limits<double>::max();
    QVariant bestId;

    for (QgsFeatureId fid : candidates) {
        const QVariant nodeId = g_nodeIndexCache.nodeIdByFid.value(fid);
        if (!nodeId.isValid() || nodeId.isNull()) {
            continue;
        }

        QgsFeature feat;
        if (!pointLayer->getFeatures(QgsFeatureRequest().setFilterFid(fid)).nextFeature(feat) || !feat.hasGeometry()) {
            continue;
        }

        QgsPointXY nodePt;
        if (feat.geometry().type() == Qgis::GeometryType::Point) {
            nodePt = feat.geometry().asPoint();
        } else {
            continue;
        }

        const double dist = point.sqrDist(nodePt);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = nodeId;
        }
    }

    if (!bestId.isValid()) {
        return false;
    }

    *outNodeId = bestId;
    return true;
}

bool extractLineEndpoints(const QgsGeometry& geom, QgsPointXY& start, QgsPointXY& end)
{
    if (geom.isEmpty() || geom.type() != Qgis::GeometryType::Line) {
        return false;
    }

    QgsPolylineXY polyline;
    if (QgsWkbTypes::isMultiType(geom.wkbType())) {
        const QgsMultiPolylineXY multi = geom.asMultiPolyline();
        if (multi.isEmpty() || multi.first().isEmpty()) {
            return false;
        }
        polyline = multi.first();
    } else {
        polyline = geom.asPolyline();
        if (polyline.isEmpty()) {
            return false;
        }
    }

    start = polyline.first();
    end = polyline.last();
    return true;
}

qlonglong computeNextLinkId(QgsVectorLayer* wayLayer)
{
    if (!wayLayer) {
        return 1;
    }

    const int linkIdIdx = wayLayer->fields().indexFromName(QStringLiteral("link_id"));
    if (linkIdIdx < 0) {
        return 1;
    }

    qlonglong maxId = 0;
    bool hasValid = false;

    QgsFeature feat;
    QgsFeatureIterator it = wayLayer->getFeatures();
    while (it.nextFeature(feat)) {
        const QVariant raw = feat.attribute(linkIdIdx);
        if (!raw.isValid() || raw.isNull()) {
            continue;
        }

        bool ok = false;
        const qlonglong val = raw.toLongLong(&ok);
        if (!ok) {
            const QString text = raw.toString().trimmed();
            if (text.isEmpty()) {
                continue;
            }
            const qlonglong parsed = text.toLongLong(&ok);
            if (!ok) {
                continue;
            }
            if (parsed > maxId) {
                maxId = parsed;
            }
            hasValid = true;
            continue;
        }

        if (val > maxId) {
            maxId = val;
        }
        hasValid = true;
    }

    return hasValid ? maxId + 1 : 1;
}

double measureLineLength(QgsVectorLayer* wayLayer, const QgsGeometry& geom)
{
    if (!wayLayer || geom.isEmpty()) {
        return -1.0;
    }

    QgsDistanceArea da;
    da.setSourceCrs(wayLayer->crs(), QgsProject::instance()->transformContext());
    da.setEllipsoid(QgsProject::instance()->ellipsoid());

    const double length = da.measureLength(geom);
    if (!qIsFinite(length) || length <= 0.0) {
        return -1.0;
    }
    return length;
}

void applyDefaultNumericFields(QgsFeature& feature, const QgsFields& fields, const WayAddDefaults& defaults)
{
    setFieldIfExists(feature, fields, QStringLiteral("capacity"), defaults.capacity);
    setFieldIfExists(feature, fields, QStringLiteral("b"), defaults.b);
    setFieldIfExists(feature, fields, QStringLiteral("power"), defaults.power);
    setFieldIfExists(feature, fields, QStringLiteral("toll"), defaults.toll);
    setFieldIfExists(feature, fields, QStringLiteral("type"), defaults.type);
    setFieldIfExists(feature, fields, QStringLiteral("speedlimit"), defaults.speedlimit);
    setFieldIfExists(feature, fields, QStringLiteral("adjust_num"), defaults.adjustNum);
    setFieldIfExists(feature, fields, QStringLiteral("lane_num"), defaults.laneNum);
}

void applyNullFields(QgsFeature& feature, const QgsFields& fields)
{
    static const QStringList kNullFields = {
        QStringLiteral("centroid_matched_node"),
        QStringLiteral("link_osmid"),
        QStringLiteral("volume_edit"),
        QStringLiteral("saturation_edit"),
        QStringLiteral("link_flow_diff"),
        QStringLiteral("saturation_diff"),
        QStringLiteral("volume"),
        QStringLiteral("v_c"),
        QStringLiteral("bottleneck"),
    };

    for (const QString& name : kNullFields) {
        setFieldNullIfExists(feature, fields, name);
    }
}

void ensurePointLayerChecked(const QString& pointVal,
                             CaseTreeView* schemeTree,
                             BaseDataTableView* baseDataView,
                             int caseId)
{
    if (pointVal.isEmpty()) {
        return;
    }

    if (baseDataView) {
        baseDataView->setLayerCheckedByVal(pointVal, true, nullptr);
        return;
    }

    if (schemeTree && schemeTree->caseModel() && caseId > 0) {
        schemeTree->caseModel()->setLayerCheckedByVal(caseId, pointVal, true, nullptr);
    }
}

} // namespace

void prepareNodeLayerForWayAdd(TNMapWidget* map,
                               CaseTreeView* schemeTree,
                               BaseDataTableView* baseDataView,
                               int caseId)
{
    if (!map) {
        return;
    }

    auto* wayLayer = qobject_cast<QgsVectorLayer*>(map->currentLayer());
    if (!wayLayer) {
        return;
    }

    const QString wayVal = resolveWayLayerVal(wayLayer);
    if (!CaseWayLayer::shouldAutoFillWayAttributes(wayVal)) {
        return;
    }

    const QString pointVal = CaseWayLayer::pointLayerValForWayLayerVal(wayVal);
    if (pointVal.isEmpty()) {
        return;
    }

    if (resolvePointLayerOnMap(map, wayLayer)) {
        return;
    }

    ensurePointLayerChecked(pointVal, schemeTree, baseDataView, caseId);
    map->restoreAddFeatureTargetLayer();
}

void prefillWayAttributes(TNMapWidget* map,
                          QgsVectorLayer* wayLayer,
                          QgsFeature& feature,
                          QString* warning,
                          QStringList* lockedFieldNames)
{
    if (!wayLayer || !feature.hasGeometry()) {
        return;
    }

    const QString wayVal = resolveWayLayerVal(wayLayer);
    if (!CaseWayLayer::shouldAutoFillWayAttributes(wayVal)) {
        return;
    }

    const QgsFields fields = wayLayer->fields();
    const WayAddDefaults defaults;

    if (fields.indexFromName(QStringLiteral("link_id")) >= 0) {
        setFieldIfExists(feature, fields, QStringLiteral("link_id"), computeNextLinkId(wayLayer));
    }

    const bool needsInitNode = fields.indexFromName(QStringLiteral("init_node")) >= 0;
    const bool needsTermNode = fields.indexFromName(QStringLiteral("term_node")) >= 0;
    if (needsInitNode || needsTermNode) {
        QgsPointXY startPt;
        QgsPointXY endPt;
        if (extractLineEndpoints(feature.geometry(), startPt, endPt)) {
            QgsVectorLayer* pointLayer = resolvePointLayerOnMap(map, wayLayer);
            if (!pointLayer) {
                if (warning) {
                    if (!warning->isEmpty()) {
                        *warning += QChar('\n');
                    }
                    *warning += QString::fromUtf8(u8"点图层尚未加载，init_node/term_node 未能自动填充。");
                }
            } else {
                if (needsInitNode) {
                    QVariant initNode;
                    if (findNearestNodeId(pointLayer, startPt, &initNode)) {
                        if (setFieldIfExists(feature, fields, QStringLiteral("init_node"), initNode) && lockedFieldNames) {
                            lockedFieldNames->append(QStringLiteral("init_node"));
                        }
                    } else {
                        setFieldNullIfExists(feature, fields, QStringLiteral("init_node"));
                    }
                }
                if (needsTermNode) {
                    QVariant termNode;
                    if (findNearestNodeId(pointLayer, endPt, &termNode)) {
                        if (setFieldIfExists(feature, fields, QStringLiteral("term_node"), termNode) && lockedFieldNames) {
                            lockedFieldNames->append(QStringLiteral("term_node"));
                        }
                    } else {
                        setFieldNullIfExists(feature, fields, QStringLiteral("term_node"));
                    }
                }
            }
        }
    }

    const double lengthVal = measureLineLength(wayLayer, feature.geometry());
    if (lengthVal > 0.0) {
        setFieldIfExists(feature, fields, QStringLiteral("length"), lengthVal);
        if (fields.indexFromName(QStringLiteral("fft")) >= 0) {
            setFieldIfExists(feature, fields, QStringLiteral("fft"), lengthVal / kFftSpeedMps);
        }
    }

    applyDefaultNumericFields(feature, fields, defaults);
    applyNullFields(feature, fields);
}

PrefillHandler makePrefillHandler(TNMapWidget* map, QString* warningOut)
{
    return [map, warningOut](QgsVectorLayer* layer, QgsFeature& feature, QStringList* readOnlyFieldNames) {
        QString localWarning;
        prefillWayAttributes(map, layer, feature, warningOut ? warningOut : &localWarning, readOnlyFieldNames);
    };
}

void clearNodeIndexCache()
{
    g_nodeIndexCache = {};
}

} // namespace CaseWayFeatureAdd
