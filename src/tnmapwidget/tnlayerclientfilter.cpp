/*****************************************************************//**
 * \file   tnlayerclientfilter.cpp
 * \brief  WFS 客户端图层显示过滤（渲染器规则包装）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "tnlayerclientfilter.h"

#include "tnlayerfiltertypes.h"
#include "tnmapwidget.h"

#include <QDomDocument>
#include <memory>

#include <qgsdatasourceuri.h>
#include <qgsexpression.h>
#include <qgsexpressioncontextutils.h>
#include <qgsreadwritecontext.h>
#include <qgsrenderer.h>
#include <qgsrulebasedrenderer.h>
#include <qgssymbol.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace TNLayerFilter {

namespace {

QString combineFilters(const QString& displayFilter, const QString& classFilter)
{
    const QString display = displayFilter.trimmed();
    const QString cls = classFilter.trimmed();
    if (display.isEmpty()) {
        return cls;
    }
    if (cls.isEmpty()) {
        return display;
    }
    return QStringLiteral("(%1) AND (%2)").arg(display, cls);
}

void backupRendererIfNeeded(QgsVectorLayer* layer)
{
    if (!layer || layer->customProperty(kRendererBackupPropertyKey).isValid()) {
        return;
    }

    QgsFeatureRenderer* renderer = layer->renderer();
    if (!renderer || dynamic_cast<QgsRuleBasedRenderer*>(renderer)) {
        return;
    }

    std::unique_ptr<QgsFeatureRenderer> clone(renderer->clone());
    if (!clone) {
        return;
    }

    QDomDocument doc(QStringLiteral("renderer"));
    QgsReadWriteContext context;
    const QDomElement element = clone->save(doc, context);
    if (element.isNull()) {
        return;
    }
    doc.appendChild(element);
    layer->setCustomProperty(kRendererBackupPropertyKey, doc.toString());
}

void restoreRendererFromBackup(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    const QString xml = layer->customProperty(kRendererBackupPropertyKey).toString();
    layer->removeCustomProperty(kRendererBackupPropertyKey);

    if (xml.trimmed().isEmpty()) {
        if (!dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer())) {
            TNMapWidget::setDefaultRendererForVectorLayer(layer);
        }
        return;
    }

    QDomDocument doc;
    QString parseError;
    if (!doc.setContent(xml, &parseError)) {
        if (!dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer())) {
            TNMapWidget::setDefaultRendererForVectorLayer(layer);
        }
        return;
    }

    QgsReadWriteContext context;
    QDomElement rootElement = doc.documentElement();
    std::unique_ptr<QgsFeatureRenderer> restored(QgsFeatureRenderer::load(rootElement, context));
    if (restored) {
        layer->setRenderer(restored.release());
    } else if (!dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer())) {
        TNMapWidget::setDefaultRendererForVectorLayer(layer);
    }
}

QgsFeatureRenderer* buildFilteredRenderer(QgsFeatureRenderer* sourceRenderer, const QString& displayFilter,
                                         QgsVectorLayer* layer, QString* errorOut)
{
    if (!sourceRenderer || !layer) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层渲染器无效。");
        }
        return nullptr;
    }

    const QString trimmedFilter = displayFilter.trimmed();
    if (trimmedFilter.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"过滤表达式为空。");
        }
        return nullptr;
    }

    std::unique_ptr<QgsRuleBasedRenderer> ruleBased(
        QgsRuleBasedRenderer::convertFromRenderer(sourceRenderer, layer));
    if (!ruleBased || !ruleBased->rootRule()) {
        QgsSymbol* symbol = QgsSymbol::defaultSymbol(layer->geometryType());
        if (!symbol) {
            if (errorOut) {
                *errorOut = QString::fromUtf8(u8"无法创建默认符号。");
            }
            return nullptr;
        }
        ruleBased.reset(new QgsRuleBasedRenderer(symbol));
    }

    QgsRuleBasedRenderer::Rule* root = ruleBased->rootRule();
    root->setFilterExpression(combineFilters(trimmedFilter, root->filterExpression()).trimmed());

    return ruleBased.release();
}

bool isValidFilteredRenderer(QgsFeatureRenderer* renderer)
{
    auto* ruleBased = dynamic_cast<QgsRuleBasedRenderer*>(renderer);
    if (!ruleBased || !ruleBased->rootRule()) {
        return false;
    }

    QgsRuleBasedRenderer::Rule* root = ruleBased->rootRule();
    if (!root->children().isEmpty()) {
        return true;
    }

    return root->symbol() != nullptr;
}

void resetToBaseRendererBeforeWrap(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    const bool hasClientFilter = !layer->customProperty(kDisplayFilterPropertyKey).toString().trimmed().isEmpty();
    const bool hasRuleBasedRenderer = dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer()) != nullptr;
    if (!hasClientFilter && !hasRuleBasedRenderer) {
        return;
    }

    layer->removeCustomProperty(kDisplayFilterPropertyKey);
    restoreRendererFromBackup(layer);
    ensureWfsUriWithoutServerFilter(layer);
}

} // namespace

void ensureWfsUriWithoutServerFilter(QgsVectorLayer* layer, TNMapWidget* map)
{
    if (!layer || !layer->isValid() || !layer->dataProvider()) {
        return;
    }
    if (TNMapWidget::isVectorLayerReloadInProgress(layer)) {
        return;
    }

    const QString preservedTypeFilter =
        layer->customProperty(kTypeServerFilterPropertyKey).toString().trimmed();

    QgsDataSourceUri uri(layer->dataProvider()->dataSourceUri(false));
    const bool hasAnyServerFilter =
        !uri.param(QStringLiteral("filter")).trimmed().isEmpty()
        || !uri.param(QStringLiteral("cql_filter")).trimmed().isEmpty()
        || !uri.sql().trimmed().isEmpty();

    if (preservedTypeFilter.isEmpty() && hasAnyServerFilter) {
        uri.removeParam(QStringLiteral("filter"));
        uri.removeParam(QStringLiteral("cql_filter"));
        uri.setSql(QString());
        if (map) {
            map->reloadVectorLayerDataSourceSafely(layer, uri.uri(false));
        }
    }

    if (!layer->subsetString().trimmed().isEmpty()) {
        layer->setSubsetString(QString());
    }
}

void discardClientLayerFilterState(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    layer->removeCustomProperty(kDisplayFilterPropertyKey);
    layer->removeCustomProperty(kRendererBackupPropertyKey);
    layer->removeCustomProperty(kTypeServerFilterPropertyKey);
    ensureWfsUriWithoutServerFilter(layer);
}

void restoreBaseRendererAfterClientFilter(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    layer->removeCustomProperty(kDisplayFilterPropertyKey);
    restoreRendererFromBackup(layer);
}

bool wrapRendererWithDisplayFilter(QgsVectorLayer* layer, const QString& displayFilter, QString* errorOut)
{
    if (!layer || !layer->isValid()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return false;
    }

    const QString trimmed = displayFilter.trimmed();
    if (trimmed.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"过滤表达式为空。");
        }
        return false;
    }

    QgsExpression expression(trimmed);
    if (expression.hasParserError()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"表达式语法错误：%1").arg(expression.parserErrorString());
        }
        return false;
    }

    QgsExpressionContext context;
    context.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(layer));
    if (!expression.prepare(&context)) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"表达式无法在当前图层上解析。");
        }
        return false;
    }

    if (!layer->customProperty(kDisplayFilterPropertyKey).toString().trimmed().isEmpty()
        || dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer())) {
        resetToBaseRendererBeforeWrap(layer);
    }

    backupRendererIfNeeded(layer);

    QgsFeatureRenderer* sourceRenderer = layer->renderer();
    if (!sourceRenderer || dynamic_cast<QgsRuleBasedRenderer*>(sourceRenderer)) {
        TNMapWidget::setDefaultRendererForVectorLayer(layer);
        sourceRenderer = layer->renderer();
    }

    std::unique_ptr<QgsFeatureRenderer> filteredRenderer(buildFilteredRenderer(sourceRenderer, trimmed, layer, errorOut));
    if (!filteredRenderer || !isValidFilteredRenderer(filteredRenderer.get())) {
        if (errorOut && errorOut->isEmpty()) {
            *errorOut = QString::fromUtf8(u8"无法构建有效的过滤渲染器。");
        }
        return false;
    }

    layer->setRenderer(filteredRenderer.release());
    layer->setCustomProperty(kDisplayFilterPropertyKey, trimmed);
    layer->triggerRepaint();
    layer->emitStyleChanged();
    return true;
}

void unwrapRenderer(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    if (layer->customProperty(kDisplayFilterPropertyKey).toString().trimmed().isEmpty()) {
        layer->removeCustomProperty(kRendererBackupPropertyKey);
        return;
    }

    restoreBaseRendererAfterClientFilter(layer);
    ensureWfsUriWithoutServerFilter(layer);
    layer->triggerRepaint();
    layer->emitStyleChanged();
}

} // namespace TNLayerFilter
