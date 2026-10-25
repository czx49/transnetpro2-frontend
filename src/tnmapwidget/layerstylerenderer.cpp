/*****************************************************************//**
 * \file   layerstylerenderer.cpp
 * \brief  字段符号化规则到 QGIS 渲染器的转换
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#include "layerstylerenderer.h"

#include <QColor>
#include <QDebug>
#include <QtGlobal>

#include <memory>

#include <qgscategorizedsymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgsgraduatedsymbolrenderer.h>
#include <qgslinesymbol.h>
#include <qgsmarkersymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgslinesymbollayer.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>

namespace {

void setError(const QString& error, QString* errorMessage)
{
    if (errorMessage) {
        *errorMessage = error;
    }
    if (!error.trimmed().isEmpty()) {
        qWarning() << error;
    }
}

bool isDoubleEqual(double a, double b)
{
    return qAbs(a - b) <= 1e-9;
}

} // namespace

namespace LayerStyleRenderer {

bool isNumericFieldType(int variantType)
{
    const QVariant::Type type = static_cast<QVariant::Type>(variantType);
    switch (type) {
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
    case QVariant::Double:
        return true;
    default:
        return false;
    }
}

void configureSymbol(QgsSymbol* symbol, double opacity, double lineWidth)
{
    if (!symbol) {
        return;
    }

    symbol->setOpacity(qBound(0.0, opacity / 100.0, 1.0));
    const double boundedLineWidth = qMax(0.0, lineWidth);

    switch (symbol->type()) {
    case Qgis::SymbolType::Line: {
        auto* lineSymbol = dynamic_cast<QgsLineSymbol*>(symbol);
        if (lineSymbol) {
            lineSymbol->setWidth(boundedLineWidth);
        }
        break;
    }
    case Qgis::SymbolType::Fill: {
        auto* fillSymbol = dynamic_cast<QgsFillSymbol*>(symbol);
        if (!fillSymbol) {
            break;
        }
        for (int i = 0; i < fillSymbol->symbolLayerCount(); ++i) {
            QgsSymbolLayer* symbolLayer = fillSymbol->symbolLayer(i);
            auto* simpleFill = dynamic_cast<QgsSimpleFillSymbolLayer*>(symbolLayer);
            if (simpleFill) {
                simpleFill->setStrokeWidth(boundedLineWidth);
                break;
            }
            auto* simpleLine = dynamic_cast<QgsSimpleLineSymbolLayer*>(symbolLayer);
            if (simpleLine) {
                simpleLine->setWidth(boundedLineWidth);
                break;
            }
        }
        break;
    }
    case Qgis::SymbolType::Marker: {
        auto* markerSymbol = dynamic_cast<QgsMarkerSymbol*>(symbol);
        if (!markerSymbol || markerSymbol->symbolLayerCount() <= 0) {
            break;
        }
        auto* markerLayer = dynamic_cast<QgsSimpleMarkerSymbolLayer*>(markerSymbol->symbolLayer(0));
        if (markerLayer) {
            markerLayer->setStrokeWidth(boundedLineWidth);
        }
        break;
    }
    case Qgis::SymbolType::Hybrid:
        break;
    }
}

bool applyFieldColorRule(QgsVectorLayer* layer,
                         const QString& fieldName,
                         const FieldColorRule& rule,
                         QString* errorMessage)
{
    if (!layer || !layer->isValid()) {
        setError(QString::fromUtf8(u8"图层无效，无法应用字段样式。"), errorMessage);
        return false;
    }
    if (fieldName.trimmed().isEmpty() || !rule.isValid()) {
        setError(QString::fromUtf8(u8"字段或样式规则无效，无法应用字段样式。"), errorMessage);
        return false;
    }

    const int fieldIndex = layer->fields().indexFromName(fieldName);
    if (fieldIndex < 0) {
        setError(QString::fromUtf8(u8"图层中不存在字段：%1").arg(fieldName), errorMessage);
        return false;
    }

    QgsFeatureRenderer* renderer = nullptr;
    const Qgis::GeometryType geomType = layer->geometryType();

    if (rule.category == FieldStyleCategory::Number) {
        QgsRangeList ranges;
        for (const FieldColorRuleItem& item : rule.colors) {
            std::unique_ptr<QgsSymbol> symbol(QgsSymbol::defaultSymbol(geomType));
            if (!symbol) {
                continue;
            }

            const QColor color(item.color);
            if (!color.isValid()) {
                continue;
            }
            symbol->setColor(color);
            configureSymbol(symbol.get(), rule.opacity, rule.lineWidth);

            const QString label = isDoubleEqual(item.min, item.max)
                ? QString::number(item.min, 'f', 6)
                : QStringLiteral("%1 - %2").arg(item.min).arg(item.max);
            ranges.append(QgsRendererRange(item.min, item.max, symbol.release(), label));
        }

        if (!ranges.isEmpty()) {
            auto* graduatedRenderer = new QgsGraduatedSymbolRenderer(fieldName, ranges);
            graduatedRenderer->setGraduatedMethod(Qgis::GraduatedMethod::Color);
            renderer = graduatedRenderer;
        }
    } else if (rule.category == FieldStyleCategory::Bool || rule.category == FieldStyleCategory::String) {
        QgsCategoryList categories;
        for (const FieldColorRuleItem& item : rule.colors) {
            std::unique_ptr<QgsSymbol> symbol(QgsSymbol::defaultSymbol(geomType));
            if (!symbol) {
                continue;
            }

            const QColor color(item.color);
            if (!color.isValid()) {
                continue;
            }
            symbol->setColor(color);
            configureSymbol(symbol.get(), rule.opacity, rule.lineWidth);

            const QVariant value = item.value(rule.category);
            categories.append(QgsRendererCategory(value, symbol.release(), item.label(rule.category)));
        }

        if (!categories.isEmpty()) {
            renderer = new QgsCategorizedSymbolRenderer(fieldName, categories);
        }
    }

    if (!renderer) {
        setError(QString::fromUtf8(u8"字段样式没有可用的颜色分级。"), errorMessage);
        return false;
    }

    layer->setRenderer(renderer);
    layer->triggerRepaint();
    layer->emitStyleChanged();
    return true;
}

} // namespace LayerStyleRenderer
