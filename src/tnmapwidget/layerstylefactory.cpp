#include "layerstylefactory.h"
#include "symbolrendererconfig.h"
#include "tnmapwidget.h"

#include <QColor>
#include <QDebug>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>

#include "qgscategorizedsymbolrenderer.h"
#include "qgsdatasourceuri.h"
#include "qgsfeaturerequest.h"
#include "qgsfields.h"
#include "qgsgraduatedsymbolrenderer.h"
#include "qgslinesymbol.h"
#include "qgsmapcanvas.h"
#include "qgsmarkersymbol.h"
#include "qgssinglesymbolrenderer.h"
#include "qgssymbol.h"

namespace {

const QString kOtherOdVal = QStringLiteral("other_od");
const QString kOtherOdDemandStyleKey = QStringLiteral("other_od_demand");
const QString kDemandField = QStringLiteral("demand");
const QString kVolumeField = QStringLiteral("volume");
const QString kSaturationEditField = QStringLiteral("saturation_edit");
const QString kTrafficFlowStyleKey = QStringLiteral("traffic_flow");
const QString kTrafficSaturationStyleKey = QStringLiteral("traffic_saturation");
const QString kPathTraceStyleKey = QStringLiteral("path_trace");
const QString kPathFlowField = QStringLiteral("path_flow");
const QString kRedsColorRampName = QStringLiteral("Reds");
const QString kSlowRoadWayVal = QStringLiteral("slow_road_way");
const QString kBusWayVal = QStringLiteral("bus_way");
const QString kBusPointVal = QStringLiteral("bus_point");
const QString kLayerValProp = QStringLiteral("tnp_layer_val");
const double kDefaultLineWidth = 0.6;
const int kGraduatedClassCount = 5;

bool variantDemandLess(const QVariant& left, const QVariant& right)
{
    bool leftOk = false;
    bool rightOk = false;
    const double leftValue = left.toDouble(&leftOk);
    const double rightValue = right.toDouble(&rightOk);

    if (leftOk && rightOk) {
        return leftValue < rightValue;
    }

    if (leftOk != rightOk) {
        return leftOk;
    }

    return left.toString().localeAwareCompare(right.toString()) < 0;
}

QList<QVariant> sortedDemandValues(QgsVectorLayer* layer)
{
    if (!layer) {
        return {};
    }

    const int fieldIndex = layer->fields().indexOf(kDemandField);
    if (fieldIndex < 0) {
        qWarning() << "LayerStyleFactory: field not found" << kDemandField << "on layer" << layer->name();
        return {};
    }

    QList<QVariant> values = layer->uniqueValues(fieldIndex).values();
    values.erase(std::remove_if(values.begin(), values.end(), [](const QVariant& value) {
        return !value.isValid() || value.isNull() || value.toString().trimmed().isEmpty();
    }), values.end());
    std::sort(values.begin(), values.end(), variantDemandLess);
    return values;
}

void applyDefaultLineSymbol(QgsSymbol* symbol)
{
    if (!symbol) {
        return;
    }
    if (auto* lineSymbol = dynamic_cast<QgsLineSymbol*>(symbol)) {
        lineSymbol->setWidth(kDefaultLineWidth);
    }
}

QString layerValFromTypeName(const QString& typeName)
{
    const QString lower = typeName.trimmed().toLower();
    if (lower.isEmpty()) {
        return {};
    }
    const QStringList knownVals = {
        kSlowRoadWayVal,
        QStringLiteral("slow_road_point"),
        kBusWayVal,
        kBusPointVal,
        QStringLiteral("road_way"),
        QStringLiteral("road_point"),
        kOtherOdVal,
    };
    for (const QString& val : knownVals) {
        if (lower == val || lower.endsWith(QLatin1Char('_') + val)) {
            return val;
        }
    }
    return {};
}

QList<double> collectNumericFieldValues(QgsVectorLayer* layer, const QString& field)
{
    QList<double> values;
    if (!layer) {
        return values;
    }
    const int fieldIndex = layer->fields().indexOf(field);
    if (fieldIndex < 0) {
        return values;
    }

    QgsFeatureRequest request;
    request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
    request.setSubsetOfAttributes(QgsAttributeList() << fieldIndex);
    QgsFeatureIterator it = layer->getFeatures(request);
    QgsFeature feature;
    while (it.nextFeature(feature)) {
        const QVariant attr = feature.attribute(fieldIndex);
        if (!attr.isValid() || attr.isNull()) {
            continue;
        }
        bool ok = false;
        const double value = attr.toDouble(&ok);
        if (ok && std::isfinite(value)) {
            values.append(value);
        }
    }
    return values;
}

QList<QPair<double, double>> buildEqualCountRanges(QList<double> values, int classCount)
{
    QList<QPair<double, double>> ranges;
    if (values.isEmpty() || classCount <= 0) {
        return ranges;
    }
    std::sort(values.begin(), values.end());
    const int n = values.size();
    const int classes = qMin(classCount, n);

    QVector<double> breaks(classes + 1);
    breaks[0] = values.first();
    breaks[classes] = values.last();
    for (int i = 1; i < classes; ++i) {
        const int idx = (i * n) / classes;
        breaks[i] = values.at(qBound(0, idx, n - 1));
    }

    ranges.reserve(classes);
    for (int i = 0; i < classes; ++i) {
        ranges.append(qMakePair(breaks[i], breaks[i + 1]));
    }
    return ranges;
}

bool applyTrafficFlowStyleNow(QgsVectorLayer* layer, const QString& colorRampName, bool useEqualCount)
{
    if (!layer || !layer->isValid()) {
        return false;
    }
    if (layer->geometryType() == Qgis::GeometryType::Null) {
        return false;
    }

    const int fieldIndex = layer->fields().indexOf(kVolumeField);
    if (fieldIndex < 0) {
        return false;
    }

    const QString rampName = colorRampName.isEmpty() ? QStringLiteral("RdYlGn") : colorRampName;
    auto ramp = SymbolRendererConfig::createColorRamp(rampName);
    ramp->invert();

    QList<QPair<double, double>> rangeBounds;
    if (useEqualCount) {
        QList<double> values = collectNumericFieldValues(layer, kVolumeField);
        if (values.isEmpty()) {
            return false;
        }
        rangeBounds = buildEqualCountRanges(values, kGraduatedClassCount);
    } else {
        double min = 0.0;
        double max = 0.0;
        if (!LayerStyleFactory::fieldMinMax(layer, kVolumeField, min, max)) {
            return false;
        }
        const double step = (max - min) / static_cast<double>(kGraduatedClassCount);
        rangeBounds.reserve(kGraduatedClassCount);
        for (int i = 0; i < kGraduatedClassCount; ++i) {
            const double lo = min + step * i;
            const double hi = (i == kGraduatedClassCount - 1) ? max : min + step * (i + 1);
            rangeBounds.append(qMakePair(lo, hi));
        }
    }

    if (rangeBounds.isEmpty()) {
        return false;
    }

    const int classCount = rangeBounds.size();
    QList<QgsRendererRange> ranges;
    ranges.reserve(classCount);
    for (int i = 0; i < classCount; ++i) {
        const double lo = rangeBounds.at(i).first;
        const double hi = rangeBounds.at(i).second;
        std::unique_ptr<QgsSymbol> symbol(QgsSymbol::defaultSymbol(layer->geometryType()));
        applyDefaultLineSymbol(symbol.get());
        const double t = (classCount <= 1) ? 0.0
                                           : static_cast<double>(i) / static_cast<double>(classCount - 1);
        symbol->setColor(ramp->color(t));
        ranges.append(QgsRendererRange(lo, hi, symbol.release(), QString()));
    }

    auto* renderer = new QgsGraduatedSymbolRenderer(kVolumeField, ranges);
    renderer->setGraduatedMethod(Qgis::GraduatedMethod::Color);
    layer->setRenderer(renderer);
    layer->triggerRepaint();
    return true;
}

bool applyPathTraceStyleNow(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid()) {
        return false;
    }
    if (layer->geometryType() == Qgis::GeometryType::Null) {
        return false;
    }

    const int fieldIndex = layer->fields().indexOf(kPathFlowField);
    if (fieldIndex < 0) {
        return false;
    }

    double min = 0.0;
    double max = 0.0;
    if (!LayerStyleFactory::fieldMinMax(layer, kPathFlowField, min, max)) {
        return false;
    }

    auto ramp = SymbolRendererConfig::createColorRamp();
    ramp->invert();

    const double step = (max - min) / static_cast<double>(kGraduatedClassCount);
    QList<QgsRendererRange> ranges;
    ranges.reserve(kGraduatedClassCount);
    for (int i = 0; i < kGraduatedClassCount; ++i) {
        const double lo = min + step * i;
        const double hi = (i == kGraduatedClassCount - 1) ? max : min + step * (i + 1);
        std::unique_ptr<QgsSymbol> symbol(QgsSymbol::defaultSymbol(layer->geometryType()));
        applyDefaultLineSymbol(symbol.get());
        symbol->setColor(ramp->color(static_cast<double>(i) / static_cast<double>(kGraduatedClassCount - 1)));
        ranges.append(QgsRendererRange(lo, hi, symbol.release(), QString()));
    }

    auto* renderer = new QgsGraduatedSymbolRenderer(kPathFlowField, ranges);
    renderer->setGraduatedMethod(Qgis::GraduatedMethod::Color);
    layer->setRenderer(renderer);
    layer->triggerRepaint();
    return true;
}

bool applyTrafficSaturationStyleNow(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid()) {
        return false;
    }
    if (layer->geometryType() == Qgis::GeometryType::Null) {
        return false;
    }

    if (layer->fields().indexOf(kSaturationEditField) < 0) {
        return false;
    }

    auto ramp = SymbolRendererConfig::createColorRamp();
    ramp->invert();

    const double bounds[6] = {0.0, 0.2, 0.4, 0.6, 0.8, std::numeric_limits<double>::max()};
    const char* labels[5] = {"0-0.2", "0.2-0.4", "0.4-0.6", "0.6-0.8", "0.8-max"};

    QList<QgsRendererRange> ranges;
    ranges.reserve(kGraduatedClassCount);
    for (int i = 0; i < kGraduatedClassCount; ++i) {
        std::unique_ptr<QgsSymbol> symbol(QgsSymbol::defaultSymbol(layer->geometryType()));
        applyDefaultLineSymbol(symbol.get());
        symbol->setColor(ramp->color(static_cast<double>(i) / static_cast<double>(kGraduatedClassCount - 1)));
        ranges.append(QgsRendererRange(bounds[i], bounds[i + 1], symbol.release(),
                                     QString::fromUtf8(labels[i])));
    }

    auto* renderer = new QgsGraduatedSymbolRenderer(kSaturationEditField, ranges);
    renderer->setGraduatedMethod(Qgis::GraduatedMethod::Color);
    layer->setRenderer(renderer);
    layer->triggerRepaint();
    return true;
}

template <typename ApplyFn>
bool deferApplyWayStyle(QgsVectorLayer* layer, ApplyFn applyFn)
{
    if (!layer) {
        return false;
    }

    QPointer<QgsVectorLayer> layerGuard(layer);
    QTimer::singleShot(0, layer, [layerGuard, applyFn]() {
        if (!layerGuard) {
            return;
        }
        if (applyFn(layerGuard.data())) {
            return;
        }
        QTimer::singleShot(200, layerGuard.data(), [layerGuard, applyFn]() {
            if (layerGuard) {
                applyFn(layerGuard.data());
            }
        });
    });
    return true;
}

bool applyOtherOdDemandStyleNow(QgsVectorLayer* layer, QgsMapCanvas* canvas);

QHash<QString, quint64> gOtherOdStyleGenerations;

void refreshLayerAfterOdStyle(QgsVectorLayer* layer, QgsMapCanvas* canvas)
{
    if (!layer) {
        return;
    }
    if (canvas) {
        canvas->refresh();
    } else {
        layer->triggerRepaint();
    }
}

QString otherOdStyleLayerKey(QgsVectorLayer* layer)
{
    if (!layer) {
        return {};
    }
    if (layer->dataProvider()) {
        const QString uri = layer->dataProvider()->dataSourceUri().trimmed();
        if (!uri.isEmpty()) {
            return uri;
        }
    }
    return layer->id();
}

void scheduleOtherOdDemandStyleApply(QgsVectorLayer* layer,
                                     quint64 generation,
                                     const std::function<void(bool)>& done,
                                     QgsMapCanvas* canvas,
                                     bool afterReload,
                                     int delayMs,
                                     int attemptIndex)
{
    static constexpr int kRetryDelays[] = {0, 150, 400, 800, 1500};
    static constexpr int kRetryDelaysAfterReload[] = {0, 100, 300, 600};
    const int maxAttempts = afterReload
        ? static_cast<int>(sizeof(kRetryDelaysAfterReload) / sizeof(kRetryDelaysAfterReload[0]))
        : static_cast<int>(sizeof(kRetryDelays) / sizeof(kRetryDelays[0]));

    QPointer<QgsVectorLayer> layerGuard(layer);
    QPointer<QgsMapCanvas> canvasGuard(canvas);
    const QString layerKey = otherOdStyleLayerKey(layer);

    QTimer::singleShot(delayMs, layerGuard.data(), [layerGuard, canvasGuard, generation, layerKey, done, afterReload, attemptIndex, maxAttempts]() {
        if (gOtherOdStyleGenerations.value(layerKey) != generation) {
            return;
        }
        if (!layerGuard) {
            if (done) {
                done(false);
            }
            return;
        }

        if (applyOtherOdDemandStyleNow(layerGuard.data(), canvasGuard.data())) {
            if (done) {
                done(true);
            }
            return;
        }

        const int nextAttempt = attemptIndex + 1;
        if (nextAttempt >= maxAttempts) {
            qWarning() << "LayerStyleFactory::applyOtherOdDemandStyle: deferred apply failed for"
                       << layerGuard->name() << ", fallback to default renderer";
            if (canvasGuard) {
                canvasGuard->stopRendering();
                canvasGuard->waitWhileRendering();
            }
            TNMapWidget::setDefaultRendererForVectorLayer(layerGuard.data());
            refreshLayerAfterOdStyle(layerGuard.data(), canvasGuard.data());
            if (done) {
                done(false);
            }
            return;
        }

        const int nextDelay = afterReload ? kRetryDelaysAfterReload[nextAttempt] : kRetryDelays[nextAttempt];
        scheduleOtherOdDemandStyleApply(layerGuard.data(),
                                        generation,
                                        done,
                                        canvasGuard.data(),
                                        afterReload,
                                        nextDelay,
                                        nextAttempt);
    });
}

bool applyOtherOdDemandStyleNow(QgsVectorLayer* layer, QgsMapCanvas* canvas)
{
    if (!layer || !layer->isValid()) {
        return false;
    }

    if (layer->geometryType() == Qgis::GeometryType::Null) {
        return false;
    }

    if (canvas) {
        canvas->stopRendering();
        canvas->waitWhileRendering();
        canvas->clearCache();
    }

    const QList<QVariant> values = sortedDemandValues(layer);
    if (values.isEmpty()) {
        return false;
    }

    auto ramp = SymbolRendererConfig::createColorRamp(kRedsColorRampName);

    QgsCategoryList categories;
    categories.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        const double colorPosition = values.size() <= 1
            ? 1.0
            : static_cast<double>(i) / static_cast<double>(values.size() - 1);

        QgsSymbol* symbol = QgsSymbol::defaultSymbol(layer->geometryType());
        if (!symbol) {
            continue;
        }
        symbol->setColor(ramp->color(colorPosition));
        if (auto* lineSymbol = dynamic_cast<QgsLineSymbol*>(symbol)) {
            lineSymbol->setWidth(kDefaultLineWidth);
        }
        categories.append(QgsRendererCategory(values.at(i), symbol, values.at(i).toString()));
    }

    if (categories.isEmpty()) {
        return false;
    }

    QgsFeatureRequest::OrderBy orderBy;
    orderBy.append(QgsFeatureRequest::OrderByClause(kDemandField, true));

    if (auto* existingCat = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer())) {
        if (existingCat->classAttribute().compare(kDemandField, Qt::CaseInsensitive) == 0) {
            existingCat->deleteAllCategories();
            for (const QgsRendererCategory& category : categories) {
                existingCat->addCategory(category);
            }
            existingCat->setOrderBy(orderBy);
            existingCat->setOrderByEnabled(true);
            refreshLayerAfterOdStyle(layer, canvas);
            return true;
        }
    }

    auto* renderer = new QgsCategorizedSymbolRenderer(kDemandField, categories);
    renderer->setOrderBy(orderBy);
    renderer->setOrderByEnabled(true);

    layer->setRenderer(renderer);
    refreshLayerAfterOdStyle(layer, canvas);
    return true;
}

bool tryApplyOtherOdDemandStyleImpl(QgsVectorLayer* layer, QgsMapCanvas* canvas)
{
    return applyOtherOdDemandStyleNow(layer, canvas);
}

} // namespace

bool LayerStyleFactory::isOtherOdLayer(const QString& layerVal, const QString& typeName)
{
    if (layerVal.compare(kOtherOdVal, Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString trimmedTypeName = typeName.trimmed();
    return !trimmedTypeName.isEmpty()
           && trimmedTypeName.endsWith(kOtherOdVal, Qt::CaseInsensitive);
}

bool LayerStyleFactory::fieldMinMax(QgsVectorLayer* layer, const QString& field, double& min, double& max)
{
    if (!layer || field.isEmpty()) {
        return false;
    }

    const int idx = layer->fields().indexOf(field);
    if (idx < 0) {
        qWarning() << "LayerStyleFactory::fieldMinMax: field not found" << field << "on layer" << layer->name();
        return false;
    }

    QVariant vmin;
    QVariant vmax;
    layer->minimumAndMaximumValue(idx, vmin, vmax);
    min = vmin.isValid() ? vmin.toDouble() : 0.0;
    max = vmax.isValid() ? vmax.toDouble() : (min + 1.0);
    if (max < min) {
        std::swap(min, max);
    }
    if (qFuzzyCompare(min, max)) {
        max = min + 1.0;
    }
    return true;
}

QVariantMap LayerStyleFactory::createGraduatedEqualIntervalSpec(
    const QString& field,
    double min,
    double max,
    int classCount,
    std::shared_ptr<QgsGradientColorRamp> ramp,
    bool scaleLineWidthByClass)
{
    QVariantMap spec;
    if (field.isEmpty() || classCount <= 0 || !ramp) {
        return spec;
    }

    const double step = (max - min) / static_cast<double>(classCount);
    QVariantList ranges;
    for (int i = 0; i < classCount; ++i) {
        const double rangeMin = min + step * i;
        const double rangeMax = min + step * (i + 1);
        const double colorPos = 0.2 * i;

        QVariantMap sym;
        sym.insert(QStringLiteral("color"), ramp->color(colorPos).name(QColor::HexRgb));
        if (scaleLineWidthByClass) {
            sym.insert(QStringLiteral("width"), 0.2 * (i + 1));
        }

        QVariantMap range;
        range.insert(QStringLiteral("min"), rangeMin);
        range.insert(QStringLiteral("max"), rangeMax);
        range.insert(QStringLiteral("label"), QString());
        range.insert(QStringLiteral("symbol"), sym);
        ranges.append(range);
    }

    spec.insert(QStringLiteral("type"), QStringLiteral("graduated"));
    spec.insert(QStringLiteral("field"), field);
    spec.insert(QStringLiteral("ranges"), ranges);
    return spec;
}

QVariantMap LayerStyleFactory::createOtherOdDemandSpec(QgsVectorLayer* layer)
{
    if (!layer) {
        return {};
    }

    const QList<QVariant> values = sortedDemandValues(layer);
    if (values.isEmpty()) {
        return {};
    }

    auto ramp = SymbolRendererConfig::createColorRamp(kRedsColorRampName);
    QVariantList categories;
    categories.reserve(values.size());

    for (int i = 0; i < values.size(); ++i) {
        const double colorPosition = values.size() <= 1
            ? 1.0
            : static_cast<double>(i) / static_cast<double>(values.size() - 1);

        QVariantMap symbol;
        symbol.insert(QStringLiteral("color"), ramp->color(colorPosition).name(QColor::HexRgb));

        QVariantMap category;
        category.insert(QStringLiteral("value"), values.at(i));
        category.insert(QStringLiteral("label"), values.at(i).toString());
        category.insert(QStringLiteral("symbol"), symbol);
        categories.append(category);
    }

    QVariantMap spec;
    spec.insert(QStringLiteral("type"), QStringLiteral("categorized"));
    spec.insert(QStringLiteral("field"), kDemandField);
    spec.insert(QStringLiteral("categories"), categories);
    spec.insert(QStringLiteral("orderByEnabled"), true);
    spec.insert(QStringLiteral("orderByField"), kDemandField);
    spec.insert(QStringLiteral("orderByAscending"), true);
    return spec;
}

bool LayerStyleFactory::applyOtherOdDemandStyle(QgsVectorLayer* layer, std::function<void(bool applied)> done)
{
    return applyOtherOdDemandStyle(layer, nullptr, std::move(done), false);
}

bool LayerStyleFactory::tryApplyOtherOdDemandStyle(QgsVectorLayer* layer, QgsMapCanvas* canvas)
{
    return tryApplyOtherOdDemandStyleImpl(layer, canvas);
}

bool LayerStyleFactory::applyOtherOdDemandStyle(QgsVectorLayer* layer,
                                                QgsMapCanvas* canvas,
                                                std::function<void(bool applied)> done,
                                                bool skipZeroDelayFirstAttempt)
{
    if (!layer) {
        if (done) {
            done(false);
        }
        return false;
    }

    const QString layerKey = otherOdStyleLayerKey(layer);
    const quint64 generation = ++gOtherOdStyleGenerations[layerKey];
    const bool afterReload = skipZeroDelayFirstAttempt;
    const int initialDelay = 0;
    scheduleOtherOdDemandStyleApply(layer, generation, std::move(done), canvas, afterReload, initialDelay, 0);
    return true;
}

bool LayerStyleFactory::applyOtherOdDemandStyle(QgsVectorLayer* layer)
{
    return applyOtherOdDemandStyle(layer, nullptr, nullptr, false);
}

bool LayerStyleFactory::applyTrafficFlowStyle(QgsVectorLayer* layer, const QString& colorRampName,
                                              bool useEqualCount)
{
    const QString rampName = colorRampName.isEmpty() ? QStringLiteral("RdYlGn") : colorRampName;
    return deferApplyWayStyle(layer, [rampName, useEqualCount](QgsVectorLayer* vl) {
        return applyTrafficFlowStyleNow(vl, rampName, useEqualCount);
    });
}

bool LayerStyleFactory::applyTrafficSaturationStyle(QgsVectorLayer* layer)
{
    return deferApplyWayStyle(layer, applyTrafficSaturationStyleNow);
}

bool LayerStyleFactory::applyPathTraceStyle(QgsVectorLayer* layer)
{
    return deferApplyWayStyle(layer, applyPathTraceStyleNow);
}

bool LayerStyleFactory::applyStyle(QgsVectorLayer* layer, const QString& styleKey)
{
    if (!layer || styleKey.isEmpty()) {
        return false;
    }

    if (styleKey == kOtherOdDemandStyleKey || styleKey == kOtherOdVal) {
        return applyOtherOdDemandStyle(layer);
    }
    if (styleKey == kTrafficFlowStyleKey || styleKey == QStringLiteral("road_way_volume")) {
        return applyTrafficFlowStyle(layer);
    }
    if (styleKey == kTrafficSaturationStyleKey || styleKey == QStringLiteral("road_way_saturation")) {
        return applyTrafficSaturationStyle(layer);
    }
    if (styleKey == kPathTraceStyleKey || styleKey == QStringLiteral("trace_tmp")) {
        return applyPathTraceStyle(layer);
    }

    return false;
}

QString LayerStyleFactory::layerValPropertyKey()
{
    return kLayerValProp;
}

QString LayerStyleFactory::resolveLayerVal(QgsVectorLayer* layer, const QString& layerVal,
                                           const QString& typeName)
{
    const QString fromArg = layerVal.trimmed().toLower();
    if (!fromArg.isEmpty()) {
        return fromArg;
    }

    if (layer) {
        const QString fromProp = layer->customProperty(kLayerValProp).toString().trimmed().toLower();
        if (!fromProp.isEmpty()) {
            return fromProp;
        }
    }

    const QString fromType = layerValFromTypeName(typeName);
    if (!fromType.isEmpty()) {
        return fromType;
    }

    if (layer && layer->dataProvider()) {
        QgsDataSourceUri uri(layer->dataProvider()->dataSourceUri());
        QString table = uri.table();
        if (table.isEmpty()) {
            table = uri.param(QStringLiteral("typename"));
        }
        return layerValFromTypeName(table);
    }
    return {};
}

bool LayerStyleFactory::applyDefaultStyle(QgsVectorLayer* layer, const QString& layerVal, const QString& typeName)
{
    if (!layer || !layer->isValid()) {
        return false;
    }

    const QString val = resolveLayerVal(layer, layerVal, typeName);
    if (!val.isEmpty()) {
        layer->setCustomProperty(kLayerValProp, val);
    }

    const QString typeLower = typeName.trimmed().toLower();
    if (val.contains(QStringLiteral("trace_tmp")) || typeLower.contains(QStringLiteral("trace_tmp"))) {
        return applyPathTraceStyle(layer);
    }
    return false;
}

bool LayerStyleFactory::applyDefaultStyle(TNMapWidget* map, const QString& dataSourceUri, const QString& layerVal,
                                          const QString& typeName)
{
    if (!map || dataSourceUri.isEmpty()) {
        return false;
    }

    auto* layer = qobject_cast<QgsVectorLayer*>(map->getLayer(dataSourceUri));
    if (!layer) {
        return false;
    }
    return applyDefaultStyle(layer, layerVal, typeName);
}
