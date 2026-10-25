/*****************************************************************//**
 * \file   tnlayerfilterapply.cpp
 * \brief  定义查询应用到矢量图层
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "tnlayerfilterapply.h"

#include "tnlayerclientfilter.h"
#include "tnlayerclientfilter.h"
#include "tnlayerfiltersqlbuilder.h"
#include "tnmapwidget.h"

#include <QDebug>

#include <qgsexpression.h>
#include <qgsexpressioncontextutils.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsmapcanvas.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace TNLayerFilter {

namespace {

bool isWfsProvider(const QgsVectorLayer* layer)
{
    return SqlBuilder::layerUsesQgsExpressionSubset(layer);
}

void reloadVectorLayerData(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    if (QgsVectorDataProvider* provider = layer->dataProvider()) {
        provider->reloadData();
    }

    layer->reload();
    layer->updateExtents();
    layer->triggerRepaint();
    layer->emitStyleChanged();
}

QString checkFeatureCountUnchanged(qlonglong countBefore, qlonglong countAfter, const QString& subset)
{
    if (subset.trimmed().isEmpty()) {
        return {};
    }
    if (countBefore < 0 || countAfter < 0) {
        return {};
    }
    if (countBefore != countAfter) {
        return {};
    }
    return QString::fromUtf8(u8"服务端可能未应用过滤，要素数量未变化（约 %1 条）。请检查字段名、运算符或 WFS 服务能力。")
        .arg(countAfter);
}

bool applyGenericDefinitionQuery(QgsVectorLayer* layer, const QString& whereClause, QString* errorOut, QString* warningOut)
{
    Q_UNUSED(warningOut);

    if (!layer) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return false;
    }

    const QString trimmed = whereClause.trimmed();
    const qlonglong countBefore = layer->featureCount();

    if (!layer->setSubsetString(trimmed)) {
        if (errorOut) {
            const QString err = layer->lastError().trimmed();
            *errorOut = err.isEmpty() ? QString::fromUtf8(u8"无法应用过滤条件。") : err;
        }
        return false;
    }

    reloadVectorLayerData(layer);

    const QString countWarning = checkFeatureCountUnchanged(countBefore, layer->featureCount(), trimmed);
    if (!countWarning.isEmpty() && warningOut) {
        *warningOut = countWarning;
    }

    return true;
}

} // namespace

ApplyDefinitionQueryResult applyClientLayerFilter(QgsVectorLayer* layer, const QString& expression, QString* errorOut)
{
    ApplyDefinitionQueryResult result;

    if (!layer || !layer->isValid()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return result;
    }

    const QString trimmed = expression.trimmed();
    if (trimmed.isEmpty()) {
        return clearClientLayerFilter(layer, errorOut);
    }

    if (!SqlBuilder::validateSubsetForLayer(layer, trimmed, errorOut)) {
        return result;
    }

    ensureWfsUriWithoutServerFilter(layer);

    if (!wrapRendererWithDisplayFilter(layer, trimmed, errorOut)) {
        return result;
    }

    result.success = true;
    qDebug() << "TNLayerFilter::applyClientLayerFilter applied to" << layer->name()
             << "expression:" << trimmed;
    return result;
}

ApplyDefinitionQueryResult clearClientLayerFilter(QgsVectorLayer* layer, QString* errorOut)
{
    ApplyDefinitionQueryResult result;

    if (!layer) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return result;
    }

    ensureWfsUriWithoutServerFilter(layer);
    unwrapRenderer(layer);
    result.success = true;
    return result;
}

QString displayFilterExpression(const QgsVectorLayer* layer)
{
    if (!layer) {
        return {};
    }

    if (isWfsProvider(layer)) {
        return layer->customProperty(kDisplayFilterPropertyKey).toString().trimmed();
    }

    return layer->subsetString().trimmed();
}

bool layerUsesClientDisplayFilter(const QgsVectorLayer* layer)
{
    return isWfsProvider(layer) && !displayFilterExpression(layer).isEmpty();
}

qlonglong countFeaturesMatchingExpression(QgsVectorLayer* layer, const QString& expression, QString* errorOut)
{
    if (!layer || !layer->isValid()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return -1;
    }

    const QString trimmed = expression.trimmed();
    if (trimmed.isEmpty()) {
        return layer->featureCount();
    }

    QgsExpression expr(trimmed);
    if (expr.hasParserError()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"表达式语法错误：%1").arg(expr.parserErrorString());
        }
        return -1;
    }

    QgsExpressionContext context;
    context.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(layer));
    if (!expr.prepare(&context)) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"表达式无法在当前图层上解析。");
        }
        return -1;
    }

    QgsFeatureRequest request;
    request.setFilterExpression(trimmed);
    request.setExpressionContext(context);

    qlonglong count = 0;
    QgsFeature feature;
    QgsFeatureIterator iterator = layer->getFeatures(request);
    while (iterator.nextFeature(feature)) {
        ++count;
    }
    return count;
}

bool hasFeaturesMatchingExpression(QgsVectorLayer* layer, const QString& expression, QString* errorOut)
{
    if (!layer || !layer->isValid()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return false;
    }

    const QString trimmed = expression.trimmed();

    QgsFeatureRequest request;
    if (!trimmed.isEmpty()) {
        QgsExpression expr(trimmed);
        if (expr.hasParserError()) {
            if (errorOut) {
                *errorOut = QString::fromUtf8(u8"表达式语法错误：%1").arg(expr.parserErrorString());
            }
            return false;
        }

        QgsExpressionContext context;
        context.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(layer));
        if (!expr.prepare(&context)) {
            if (errorOut) {
                *errorOut = QString::fromUtf8(u8"表达式无法在当前图层上解析。");
            }
            return false;
        }

        request.setFilterExpression(trimmed);
        request.setExpressionContext(context);
    }

    request.setLimit(1);
    request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);

    QgsFeature feature;
    QgsFeatureIterator iterator = layer->getFeatures(request);
    return iterator.nextFeature(feature);
}

ApplyDefinitionQueryResult applyDefinitionQuery(QgsVectorLayer* layer, const QString& subsetClause, QString* errorOut)
{
    ApplyDefinitionQueryResult result;

    if (!layer || !layer->isValid()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return result;
    }

    if (isWfsProvider(layer)) {
        if (subsetClause.trimmed().isEmpty()) {
            return clearClientLayerFilter(layer, errorOut);
        }
        return applyClientLayerFilter(layer, subsetClause, errorOut);
    }

    bool ok = applyGenericDefinitionQuery(layer, subsetClause, errorOut, &result.warning);
    if (!ok) {
        return result;
    }

    result.success = true;

    qDebug() << "TNLayerFilter::applyDefinitionQuery applied to" << layer->name()
             << "provider:" << layer->providerType() << "subset:" << layer->subsetString()
             << "featureCount:" << layer->featureCount()
             << "warning:" << result.warning;

    return result;
}

void refreshLayerAfterDefinitionQuery(QgsVectorLayer* layer, TNMapWidget* mapWidget)
{
    if (!mapWidget || !layer) {
        return;
    }

    mapWidget->setCurrentLayer(layer);

    if (QgsMapCanvas* canvas = mapWidget->canvas()) {
        canvas->refreshAllLayers();
    }
}

} // namespace TNLayerFilter
