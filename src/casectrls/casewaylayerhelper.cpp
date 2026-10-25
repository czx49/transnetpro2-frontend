#include "casewaylayerhelper.h"

#include "../tnmapwidget/tnlayerclientfilter.h"
#include "../tnmapwidget/tnlayerfilterapply.h"
#include "../tnmapwidget/tnlayerfiltertypes.h"
#include "../tnmapwidget/tnmapwidget.h"
#include "../uis/uiglobalstate.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QPair>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QtConcurrent>

#include <memory>

#include <qgsexpressioncontext.h>
#include <qgsexpressioncontextutils.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsrulebasedrenderer.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerfeatureiterator.h>

namespace CaseWayLayer {

namespace {

const QString kRoadWay = QStringLiteral("road_way");
const QString kBusWay = QStringLiteral("bus_way");
const QString kSlowRoadWay = QStringLiteral("slow_road_way");
const QString kRoadPoint = QStringLiteral("road_point");
const QString kBusPoint = QStringLiteral("bus_point");
const QString kSlowRoadPoint = QStringLiteral("slow_road_point");
const QString kTypeField = QStringLiteral("type");
int g_defaultTypeFilterSuspendDepth = 0;
/** 正在进行异步 fail-open 校验的图层（仅用于去重，回调开头无条件移除） */
QSet<const QgsVectorLayer*> g_failOpenChecking;

/** 工作线程探测结果：first = 有匹配 type 过滤的要素，second = 图层有任意要素 */
using FailOpenProbeResult = QPair<bool, bool>;

bool isWfsLayer(QgsVectorLayer* layer)
{
    return layer && layer->providerType().compare(QStringLiteral("WFS"), Qt::CaseInsensitive) == 0;
}

bool layerReadyForTypeFilter(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid()) {
        return false;
    }
    if (layer->geometryType() == Qgis::GeometryType::Null) {
        return false;
    }
    return layerHasTypeField(layer);
}

bool applyTypeFilterExpression(QgsVectorLayer* layer, const QString& expression)
{
    if (!layer || !layer->isValid()) {
        return false;
    }

    QString error;
    const TNLayerFilter::ApplyDefinitionQueryResult result =
        TNLayerFilter::applyDefinitionQuery(layer, expression, &error);
    if (!result.success) {
        if (!error.isEmpty()) {
            qWarning() << "CaseWayLayer: apply type filter failed on" << layer->name() << error;
        }
        return false;
    }
    if (!result.warning.isEmpty()) {
        qDebug() << "CaseWayLayer: type filter warning on" << layer->name() << result.warning;
    }
    return true;
}

bool rendererHasTypeFilter(QgsVectorLayer* layer, const QString& expected)
{
    if (!layer || expected.trimmed().isEmpty()) {
        return false;
    }

    auto* ruleBased = dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer());
    if (!ruleBased || !ruleBased->rootRule()) {
        return false;
    }

    const QString rootFilter = ruleBased->rootRule()->filterExpression().trimmed();
    return rootFilter.contains(expected, Qt::CaseInsensitive);
}

/** 纯状态判断：显示过滤标记一致且渲染器根规则已含 type 过滤（无要素探测） */
bool typeFilterWrapped(QgsVectorLayer* layer, const QString& expected)
{
    if (!layer) {
        return false;
    }
    if (TNLayerFilter::displayFilterExpression(layer).compare(expected, Qt::CaseInsensitive) != 0) {
        return false;
    }
    return rendererHasTypeFilter(layer, expected);
}

bool hasAnalysisStyle(const QgsVectorLayer* layer)
{
    return layer
           && !layer->customProperty(TNLayerFilter::kWayAnalysisStylePropertyKey).toString().trimmed().isEmpty();
}

bool typeFilterVerified(const QgsVectorLayer* layer, const QString& expected)
{
    return layer
           && layer->customProperty(TNLayerFilter::kTypeFilterVerifiedPropertyKey)
                      .toString()
                      .trimmed()
                      .compare(expected, Qt::CaseInsensitive)
                  == 0;
}

/** 已被 fail-open 回退：校验标记存在、但显示过滤与服务端标记均已清除 */
bool typeFilterRevertedByFailOpen(QgsVectorLayer* layer, const QString& expected)
{
    if (!typeFilterVerified(layer, expected)) {
        return false;
    }
    if (!TNLayerFilter::displayFilterExpression(layer).isEmpty()) {
        return false;
    }
    return layer->customProperty(TNLayerFilter::kTypeServerFilterPropertyKey).toString().trimmed().isEmpty();
}

/** @param defer 为 true 时推迟到下一轮事件循环再换 provider，避开当前调用栈上的渲染 */
void stripTypeServerFilterUri(QgsVectorLayer* layer, TNMapWidget* map, bool defer)
{
    if (!layer) {
        return;
    }
    if (!defer) {
        TNLayerFilter::ensureWfsUriWithoutServerFilter(layer, map);
        return;
    }

    QPointer<QgsVectorLayer> layerGuard(layer);
    QPointer<TNMapWidget> mapGuard(map);
    QObject* context = map ? static_cast<QObject*>(map) : static_cast<QObject*>(layer);
    QTimer::singleShot(0, context, [layerGuard, mapGuard]() {
        if (layerGuard) {
            TNLayerFilter::ensureWfsUriWithoutServerFilter(layerGuard.data(), mapGuard.data());
        }
    });
}

void revertTypeFilterFailOpen(QgsVectorLayer* layer, const QString& expected)
{
    qWarning() << "CaseWayLayer: type filter excluded all features on" << layer->name()
               << "- reverting filter" << expected;
    if (isWfsLayer(layer)) {
        layer->removeCustomProperty(TNLayerFilter::kTypeServerFilterPropertyKey);
        stripTypeServerFilterUri(layer, nullptr, true);
    }
    TNLayerFilter::restoreBaseRendererAfterClientFilter(layer);
    layer->triggerRepaint();
    layer->emitStyleChanged();
}

TypeFilterApplyResult applyDefaultTypeFilterNowInternal(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid()) {
        return TypeFilterApplyResult::NotReady;
    }
    if (TNMapWidget::isVectorLayerReloadInProgress(layer) || g_defaultTypeFilterSuspendDepth > 0) {
        return TypeFilterApplyResult::NotReady;
    }

    if (hasAnalysisStyle(layer)) {
        return TypeFilterApplyResult::Skipped;
    }

    if (!isWfsLayer(layer) && !layerHasTypeField(layer)) {
        return TypeFilterApplyResult::NotReady;
    }

    if (!layerReadyForTypeFilter(layer)) {
        return TypeFilterApplyResult::NotReady;
    }

    const QString expected = defaultTypeSubsetExpression();
    if (typeFilterRevertedByFailOpen(layer, expected)) {
        // 该层已被校验为“过滤后无要素”并回退，不再反复包装
        return TypeFilterApplyResult::Skipped;
    }
    if (typeFilterWrapped(layer, expected)) {
        return TypeFilterApplyResult::AlreadyApplied;
    }

    if (!layer->renderer() || dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer())) {
        TNMapWidget::setDefaultRendererForVectorLayer(layer);
    }

    if (TNLayerFilter::displayFilterExpression(layer).compare(expected, Qt::CaseInsensitive) == 0) {
        TNLayerFilter::restoreBaseRendererAfterClientFilter(layer);
        TNMapWidget::setDefaultRendererForVectorLayer(layer);
    }

    if (!applyTypeFilterExpression(layer, expected)) {
        return TypeFilterApplyResult::NotReady;
    }

    return typeFilterWrapped(layer, expected) ? TypeFilterApplyResult::Applied : TypeFilterApplyResult::NotReady;
}

/** 主线程：消费工作线程探测结果，仅在状态未被期间改动时生效 */
void onFailOpenCheckFinished(QPointer<QgsVectorLayer> layerGuard, const QgsVectorLayer* rawLayer,
                             const QString& expected, const FailOpenProbeResult& probe, qint64 elapsedMs)
{
    g_failOpenChecking.remove(rawLayer);
    if (!layerGuard) {
        return;
    }
    QgsVectorLayer* layer = layerGuard.data();
    if (!layer->isValid()) {
        return;
    }
    if (TNMapWidget::isVectorLayerReloadInProgress(layer) || g_defaultTypeFilterSuspendDepth > 0) {
        return;
    }
    if (hasAnalysisStyle(layer)) {
        return;
    }
    if (!typeFilterWrapped(layer, expected)) {
        // 期间已被 clearTypeFilter / 分析样式等清除，本次结果作废
        return;
    }

    const bool hasMatch = probe.first;
    const bool hasAny = probe.second;
    qInfo() << "CaseWayLayer: fail-open check" << layer->name() << "hasMatch" << hasMatch << "hasAny" << hasAny
            << "elapsedMs" << elapsedMs;

    layer->setCustomProperty(TNLayerFilter::kTypeFilterVerifiedPropertyKey, expected);
    if (!hasMatch && hasAny) {
        revertTypeFilterFailOpen(layer, expected);
    }
}

/** 首帧后一次性异步校验：过滤后是否仍有要素；无要素但图层有数据则 fail-open 回退 */
void scheduleFailOpenCheck(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid() || !qApp) {
        return;
    }
    const QString expected = defaultTypeSubsetExpression();
    if (typeFilterVerified(layer, expected)) {
        return;
    }
    if (g_failOpenChecking.contains(layer)) {
        return;
    }
    if (!typeFilterWrapped(layer, expected)) {
        return;
    }

    if (!isWfsLayer(layer)) {
        // 本地数据源：同步探测足够快
        const bool hasMatch = TNLayerFilter::hasFeaturesMatchingExpression(layer, expected, nullptr);
        const bool hasAny = hasMatch || TNLayerFilter::hasFeaturesMatchingExpression(layer, QString(), nullptr);
        onFailOpenCheckFinished(QPointer<QgsVectorLayer>(layer), layer, expected, qMakePair(hasMatch, hasAny), 0);
        return;
    }

    QgsExpression expression(expected);
    if (expression.hasParserError()) {
        return;
    }
    QgsExpressionContext context;
    context.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(layer));
    if (!expression.prepare(&context)) {
        return;
    }

    QgsFeatureRequest matchRequest;
    matchRequest.setFilterExpression(expected);
    matchRequest.setExpressionContext(context);
    matchRequest.setLimit(1);
    matchRequest.setFlags(Qgis::FeatureRequestFlag::NoGeometry);

    QgsFeatureRequest anyRequest;
    anyRequest.setLimit(1);
    anyRequest.setFlags(Qgis::FeatureRequestFlag::NoGeometry);

    // 快照持有 WFS 共享数据，图层被删除后仍可安全迭代 / 析构；工作线程不触碰 QgsVectorLayer
    auto source = std::make_shared<QgsVectorLayerFeatureSource>(layer);

    g_failOpenChecking.insert(layer);
    const QgsVectorLayer* rawLayer = layer;
    QPointer<QgsVectorLayer> layerGuard(layer);
    auto* watcher = new QFutureWatcher<FailOpenProbeResult>(qApp);
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    QObject::connect(watcher, &QFutureWatcher<FailOpenProbeResult>::finished, qApp,
                     [watcher, layerGuard, rawLayer, expected, timer]() {
                         FailOpenProbeResult probe(true, true);
                         if (!watcher->isCanceled()) {
                             probe = watcher->result();
                         }
                         onFailOpenCheckFinished(layerGuard, rawLayer, expected, probe, timer->elapsed());
                         watcher->deleteLater();
                     });
    watcher->setFuture(QtConcurrent::run([source, matchRequest, anyRequest]() -> FailOpenProbeResult {
        QgsFeature feature;
        const bool hasMatch = source->getFeatures(matchRequest).nextFeature(feature);
        const bool hasAny = hasMatch || source->getFeatures(anyRequest).nextFeature(feature);
        return qMakePair(hasMatch, hasAny);
    }));
}

void scheduleRetry(QPointer<QgsVectorLayer> layerGuard, int attempt)
{
    if (!layerGuard) {
        return;
    }

    if (TNMapWidget::isVectorLayerReloadInProgress(layerGuard.data()) || g_defaultTypeFilterSuspendDepth > 0) {
        QTimer::singleShot(50, layerGuard.data(), [layerGuard, attempt]() {
            scheduleRetry(layerGuard, attempt);
        });
        return;
    }

    switch (applyDefaultTypeFilterNowInternal(layerGuard.data())) {
    case TypeFilterApplyResult::Applied:
        layerGuard->triggerRepaint();
        scheduleFailOpenCheck(layerGuard.data());
        return;
    case TypeFilterApplyResult::AlreadyApplied:
        // 已包装且未变化：不重绘，只做一次性存在性校验
        scheduleFailOpenCheck(layerGuard.data());
        return;
    case TypeFilterApplyResult::Skipped:
        return;
    case TypeFilterApplyResult::NotReady:
    default:
        break;
    }

    static const int kRetryDelaysMs[] = {200, 500, 1000, 2000, 4000};
    if (attempt >= static_cast<int>(sizeof(kRetryDelaysMs) / sizeof(kRetryDelaysMs[0]))) {
        qWarning() << "CaseWayLayer: deferred type filter gave up for" << layerGuard->name()
                   << "filter:" << TNLayerFilter::displayFilterExpression(layerGuard.data());
        return;
    }

    QTimer::singleShot(kRetryDelaysMs[attempt], layerGuard.data(), [layerGuard, attempt]() {
        scheduleRetry(layerGuard, attempt + 1);
    });
}

bool typeNameEndsWithWaySuffix(const QString& typeName, const QString& suffix)
{
    const QString t = typeName.trimmed().toLower();
    const QString s = suffix.trimmed().toLower();
    return t == s || t.endsWith(QLatin1Char('_') + s);
}

} // namespace

bool isWayLayerVal(const QString& layerVal)
{
    const QString val = layerVal.trimmed().toLower();
    return val == kRoadWay || val == kBusWay || val == kSlowRoadWay;
}

bool isWayLayerTypeName(const QString& typeName)
{
    return typeNameEndsWithWaySuffix(typeName, kRoadWay) || typeNameEndsWithWaySuffix(typeName, kBusWay)
           || typeNameEndsWithWaySuffix(typeName, kSlowRoadWay);
}

bool shouldApplyDefaultTypeFilter(bool isBaseCase, const QString& layerVal, const QString& typeName)
{
    return isBaseCase && (isWayLayerVal(layerVal) || isWayLayerTypeName(typeName));
}

QString defaultTypeSubsetExpression()
{
    return QStringLiteral("\"type\" < 10");
}

QString defaultTypeRowFilterExpression()
{
    return defaultTypeSubsetExpression();
}

bool layerHasTypeField(QgsVectorLayer* layer)
{
    if (!layer) {
        return false;
    }
    return layer->fields().indexFromName(kTypeField) >= 0;
}

TypeFilterApplyResult applyDefaultTypeFilterNow(QgsVectorLayer* layer)
{
    return applyDefaultTypeFilterNowInternal(layer);
}

void applyDefaultTypeFilter(QgsVectorLayer* layer)
{
    applyDefaultTypeFilterNowInternal(layer);
}

void clearTypeFilter(QgsVectorLayer* layer, TNMapWidget* map, bool deferProviderReload)
{
    if (!layer) {
        return;
    }

    // 允许下次重新包装后再做一次 fail-open 校验
    layer->removeCustomProperty(TNLayerFilter::kTypeFilterVerifiedPropertyKey);

    if (isWfsLayer(layer)) {
        layer->removeCustomProperty(TNLayerFilter::kTypeServerFilterPropertyKey);
        TNLayerFilter::restoreBaseRendererAfterClientFilter(layer);
        stripTypeServerFilterUri(layer, map, deferProviderReload);
        layer->triggerRepaint();
        layer->emitStyleChanged();
        return;
    }

    applyTypeFilterExpression(layer, QString());
}

void scheduleDefaultTypeFilter(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    QPointer<QgsVectorLayer> layerGuard(layer);
    QTimer::singleShot(0, layer, [layerGuard]() {
        scheduleRetry(layerGuard, 0);
    });
}

void setDefaultTypeFilterSuspended(bool suspended)
{
    if (suspended) {
        ++g_defaultTypeFilterSuspendDepth;
    } else if (g_defaultTypeFilterSuspendDepth > 0) {
        --g_defaultTypeFilterSuspendDepth;
    }
}

bool isDefaultTypeFilterSuspended()
{
    return g_defaultTypeFilterSuspendDepth > 0;
}

DefaultTypeFilterSuspendGuard::DefaultTypeFilterSuspendGuard()
{
    setDefaultTypeFilterSuspended(true);
}

DefaultTypeFilterSuspendGuard::~DefaultTypeFilterSuspendGuard()
{
    setDefaultTypeFilterSuspended(false);
}

QString findLayerTypeName(const CaseItem& caseItem, const QString& layerVal)
{
    const QString target = layerVal.trimmed().toLower();
    if (target.isEmpty()) {
        return {};
    }

    for (const LayerItem& layer : caseItem.caseLayers) {
        if (layer.val.trimmed().toLower() == target) {
            return layer.wfsTypeName.trimmed();
        }
    }

    for (const LayerGroupItem& group : caseItem.caseLayerGroups) {
        for (const LayerItem& layer : group.layers) {
            if (layer.val.trimmed().toLower() == target) {
                return layer.wfsTypeName.trimmed();
            }
        }
    }
    return {};
}

QString pgTableNameForLayer(const CaseItem& caseItem, const QString& layerVal)
{
    const QString typeName = findLayerTypeName(caseItem, layerVal);
    if (typeName.isEmpty()) {
        return {};
    }

    const QSharedPointer<ProjectDetailJson> detail = UIG().currentProjectDetail();
    if (detail.isNull()) {
        return typeName;
    }

    const QString dbfix = detail->j_dbfix().trimmed();
    if (dbfix.isEmpty()) {
        return typeName;
    }

    if (typeName.contains(QLatin1Char('.'))) {
        return typeName;
    }
    return dbfix + QLatin1Char('.') + typeName;
}

QString deriveTraceTmpTypeName(const QString& sourceWayTypeName)
{
    const QString source = sourceWayTypeName.trimmed();
    if (source.isEmpty()) {
        return {};
    }

    static const QStringList kWaySuffixes = {
        QStringLiteral("slow_road_way"),
        QStringLiteral("road_way"),
        QStringLiteral("bus_way"),
    };

    for (const QString& suffix : kWaySuffixes) {
        if (source.endsWith(suffix, Qt::CaseInsensitive)) {
            return source.left(source.size() - suffix.size()) + QStringLiteral("trace_tmp");
        }
    }

    if (source.endsWith(QStringLiteral("trace_tmp"), Qt::CaseInsensitive)) {
        return source;
    }

    return source + QStringLiteral("_trace_tmp");
}

bool shouldAutoFillWayAttributes(const QString& layerVal)
{
    const QString val = layerVal.trimmed().toLower();
    return val == kRoadWay || val == kBusWay || val == kSlowRoadWay;
}

QString pointLayerValForWayLayerVal(const QString& wayLayerVal)
{
    const QString val = wayLayerVal.trimmed().toLower();
    if (val == kRoadWay) {
        return kRoadPoint;
    }
    if (val == kBusWay) {
        return kBusPoint;
    }
    if (val == kSlowRoadWay) {
        return kSlowRoadPoint;
    }
    return {};
}

QString derivePointTypeName(const QString& wayTypeName)
{
    const QString source = wayTypeName.trimmed();
    if (source.isEmpty()) {
        return {};
    }

    static const struct {
        const char* waySuffix;
        const char* pointSuffix;
    } kMappings[] = {
        {"slow_road_way", "slow_road_point"},
        {"bus_way", "bus_point"},
        {"road_way", "road_point"},
    };

    for (const auto& mapping : kMappings) {
        const QString waySuffix = QString::fromLatin1(mapping.waySuffix);
        if (source.endsWith(waySuffix, Qt::CaseInsensitive)) {
            return source.left(source.size() - waySuffix.size()) + QString::fromLatin1(mapping.pointSuffix);
        }
    }
    return {};
}

QString wayLayerValFromTypeName(const QString& typeName)
{
    if (typeNameEndsWithWaySuffix(typeName, kSlowRoadWay)) {
        return kSlowRoadWay;
    }
    if (typeNameEndsWithWaySuffix(typeName, kRoadWay)) {
        return kRoadWay;
    }
    if (typeNameEndsWithWaySuffix(typeName, kBusWay)) {
        return kBusWay;
    }
    return {};
}

} // namespace CaseWayLayer
