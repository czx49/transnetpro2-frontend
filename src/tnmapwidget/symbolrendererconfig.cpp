#include "symbolrendererconfig.h"
#include <QVariant>
#include <QColor>
#include <QVector>
#include "qgssinglesymbolrenderer.h"
#include "qgscategorizedsymbolrenderer.h"
#include "qgsgraduatedsymbolrenderer.h"
#include "qgsrulebasedrenderer.h"
#include "qgsheatmaprenderer.h"
#include "qgspointclusterrenderer.h"
#include "qgsinvertedpolygonrenderer.h"
#include "qgsnullsymbolrenderer.h"
#include "qgsfeaturerequest.h"
#include "qgssymbol.h"
#include "qgslinesymbol.h"
#include "qgsfillsymbol.h"
#include "qgsmarkersymbol.h"
#include "qgscolorramp.h"
#include "qgscolorrampimpl.h"

// 解析渲染器类型字符串为枚举
static SymbolRendererConfig::RendererType parseType(const QVariantMap& spec) {
    QString t = spec.value("type").toString();
    if (t == "single") return SymbolRendererConfig::Single;
    if (t == "categorized") return SymbolRendererConfig::Categorized;
    if (t == "graduated") return SymbolRendererConfig::Graduated;
    if (t == "rule") return SymbolRendererConfig::RuleBased;
    if (t == "heatmap") return SymbolRendererConfig::Heatmap;
    if (t == "cluster") return SymbolRendererConfig::PointCluster;
    if (t == "inverted_polygon") return SymbolRendererConfig::InvertedPolygon;
    if (t == "null") return SymbolRendererConfig::NullRenderer;
    return SymbolRendererConfig::Single;
}

// 应用通用符号属性（颜色、透明度、大小/线宽等）
static void applyCommonSymbolProps(QgsSymbol* s, const QVariantMap& props) {
    if (!s) return;
    if (props.contains("color")) s->setColor(QColor(props.value("color").toString()));
    if (props.contains("opacity")) s->setOpacity(qBound(0.0, props.value("opacity").toDouble(), 1.0));
    if (auto m = dynamic_cast<QgsMarkerSymbol*>(s))
    {
        if (props.contains("size"))
        {
            m->setSize(props.value("size").toDouble());
        }
        else
        {
            m->setSize(1.8);
        }
    }
    if (auto l = dynamic_cast<QgsLineSymbol*>(s))
    {
        if (props.contains("width"))
        {
            l->setWidth(props.value("width").toDouble());
        }
        else
        {
            l->setWidth(0.6);
        }
    }
}

// 根据配置为矢量图层构建对应的渲染器
QgsFeatureRenderer* SymbolRendererConfig::buildRenderer(QgsVectorLayer* layer, QVariantMap spec) {
    if (!layer) return nullptr;
    auto type = parseType(spec);
    switch (type) {
    case Single: {
        QgsSymbol* sym = QgsSymbol::defaultSymbol(layer->geometryType());
        applyCommonSymbolProps(sym, spec.value("symbol").toMap());
        return new QgsSingleSymbolRenderer(sym);
    }
    case Categorized: {
        // 分类渲染：按字段值精确匹配
        QString field = spec.value("field").toString();
        QgsCategoryList cats;
        for (const auto& v : spec.value("categories").toList()) {
            QVariantMap c = v.toMap();
            QgsSymbol* s = QgsSymbol::defaultSymbol(layer->geometryType());
            applyCommonSymbolProps(s, c.value("symbol").toMap());
            cats.append(QgsRendererCategory(c.value("value"), s, c.value("label").toString()));
        }
        auto* renderer = new QgsCategorizedSymbolRenderer(field, cats);
        if (spec.value("orderByEnabled").toBool()) {
            QgsFeatureRequest::OrderBy orderBy;
            orderBy.append(QgsFeatureRequest::OrderByClause(
                spec.value("orderByField", field).toString(),
                spec.value("orderByAscending", true).toBool()));
            renderer->setOrderBy(orderBy);
            renderer->setOrderByEnabled(true);
        }
        return renderer;
    }
    case Graduated: {
        // 分级渲染：按数值范围分段着色
        QString field = spec.value("field").toString();
        QList<QgsRendererRange> ranges;
        for (const auto& v : spec.value("ranges").toList()) {
            QVariantMap r = v.toMap();
            QgsSymbol* s = QgsSymbol::defaultSymbol(layer->geometryType());
            applyCommonSymbolProps(s, r.value("symbol").toMap());
            ranges.append(QgsRendererRange(r.value("min").toDouble(), r.value("max").toDouble(), s, r.value("label").toString()));
        }
        auto* renderer = new QgsGraduatedSymbolRenderer(field, ranges);
        return renderer;
    }
    case RuleBased: {
        // 规则渲染：表达式+比例尺控制
        QgsRuleBasedRenderer::Rule* root = new QgsRuleBasedRenderer::Rule(nullptr);
        for (const auto& v : spec.value("rules").toList()) {
            QVariantMap r = v.toMap();
            QgsSymbol* s = QgsSymbol::defaultSymbol(layer->geometryType());
            applyCommonSymbolProps(s, r.value("symbol").toMap());
            int scaleMin = r.value("scale_min").toInt();
            int scaleMax = r.value("scale_max").toInt();
            QString expr = r.value("expression").toString();
            QString label = r.value("label").toString();
            QString elseExp = QString();
            bool enabled = true;
            QgsRuleBasedRenderer::Rule* rr = new QgsRuleBasedRenderer::Rule(s, scaleMin, scaleMax, expr, label, elseExp, enabled);
            root->appendChild(rr);
        }
        return new QgsRuleBasedRenderer(root);
    }
    case Heatmap: {
        // 热力图渲染：点图层权重与半径
        if (layer->geometryType() != Qgis::GeometryType::Point) return nullptr;
        QString field = spec.value("field").toString();
        double radius = spec.value("radius").toDouble();
        auto* r = new QgsHeatmapRenderer();
        r->setWeightExpression(field);
        r->setRadius(radius);
        r->setColorRamp(new QgsGradientColorRamp(QColor(spec.value("color1").toString()), QColor(spec.value("color2").toString())));
        return r;
    }
    case PointCluster: {
        // 点聚类渲染
        if (layer->geometryType() != Qgis::GeometryType::Point) return nullptr;
        auto* r = new QgsPointClusterRenderer();
        return r;
    }
    case InvertedPolygon: {
        // 反向面渲染：在面之外填充
        if (layer->geometryType() != Qgis::GeometryType::Polygon) return nullptr;
        QgsSymbol* s = QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon);
        applyCommonSymbolProps(s, spec.value("symbol").toMap());
        auto* inner = new QgsSingleSymbolRenderer(s);
        return new QgsInvertedPolygonRenderer(inner);
    }
    case NullRenderer:
        return new QgsNullSymbolRenderer();
    }
    return nullptr;
}

std::shared_ptr<QgsGradientColorRamp> SymbolRendererConfig::createColorRamp(const QString& colorRampName)
{
    if ("RdYlGn" == colorRampName) {
        std::shared_ptr<QgsGradientColorRamp> ramp(new QgsGradientColorRamp(QColor(215, 25, 28), QColor(26, 150, 65)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(253, 174, 97))
            << QgsGradientStop(0.5, QColor(255, 255, 192))
            << QgsGradientStop(0.75, QColor(166, 217, 106));
        ramp->setStops(stops);
        return ramp;
    } else if ("Reds" == colorRampName) {
        std::shared_ptr<QgsGradientColorRamp> ramp(new QgsGradientColorRamp(QColor(254, 229, 217), QColor(165, 15, 21)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(252, 174, 145))
              << QgsGradientStop(0.5, QColor(251, 106, 74))
              << QgsGradientStop(0.75, QColor(222, 45, 38));
        ramp->setStops(stops);
        return ramp;
    } else if ("Spectral" == colorRampName) {
        // ColorBrewer Spectral：紫蓝 → 青绿 → 黄 → 橙 → 红
        std::shared_ptr<QgsGradientColorRamp> ramp(
            new QgsGradientColorRamp(QColor(94, 79, 162), QColor(158, 1, 66)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(102, 194, 165))
              << QgsGradientStop(0.5, QColor(255, 255, 191))
              << QgsGradientStop(0.75, QColor(253, 174, 97));
        ramp->setStops(stops);
        return ramp;
    } else if ("RdYlBu" == colorRampName) {
        // ColorBrewer RdYlBu：红 → 黄 → 蓝
        std::shared_ptr<QgsGradientColorRamp> ramp(
            new QgsGradientColorRamp(QColor(215, 25, 28), QColor(44, 123, 182)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(253, 174, 97))
              << QgsGradientStop(0.5, QColor(255, 255, 191))
              << QgsGradientStop(0.75, QColor(171, 217, 233));
        ramp->setStops(stops);
        return ramp;
    } else if ("Blues" == colorRampName) {
        std::shared_ptr<QgsGradientColorRamp> ramp(
            new QgsGradientColorRamp(QColor(239, 243, 255), QColor(8, 81, 156)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(189, 215, 231))
              << QgsGradientStop(0.5, QColor(107, 174, 214))
              << QgsGradientStop(0.75, QColor(49, 130, 189));
        ramp->setStops(stops);
        return ramp;
    } else if ("YlOrRd" == colorRampName) {
        std::shared_ptr<QgsGradientColorRamp> ramp(
            new QgsGradientColorRamp(QColor(255, 255, 204), QColor(189, 0, 38)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(255, 237, 160))
              << QgsGradientStop(0.5, QColor(254, 178, 76))
              << QgsGradientStop(0.75, QColor(253, 141, 60));
        ramp->setStops(stops);
        return ramp;
    } else if ("YlGnBu" == colorRampName) {
        std::shared_ptr<QgsGradientColorRamp> ramp(
            new QgsGradientColorRamp(QColor(255, 255, 217), QColor(8, 29, 88)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(237, 248, 177))
              << QgsGradientStop(0.5, QColor(127, 205, 187))
              << QgsGradientStop(0.75, QColor(65, 182, 196));
        ramp->setStops(stops);
        return ramp;
    } else if ("PuBuGn" == colorRampName) {
        std::shared_ptr<QgsGradientColorRamp> ramp(
            new QgsGradientColorRamp(QColor(246, 239, 247), QColor(1, 70, 54)));
        QList<QgsGradientStop> stops;
        stops << QgsGradientStop(0.25, QColor(208, 209, 230))
              << QgsGradientStop(0.5, QColor(166, 189, 219))
              << QgsGradientStop(0.75, QColor(103, 169, 207));
        ramp->setStops(stops);
        return ramp;
    } else {
        return std::make_shared<QgsGradientColorRamp>(QColor(255, 0, 0), QColor(0, 128, 0));
    }
}

QStringList SymbolRendererConfig::availableColorRampNames()
{
    return QStringList{
        QStringLiteral("RdYlGn"),
        QStringLiteral("Reds"),
        QStringLiteral("Spectral"),
        QStringLiteral("RdYlBu"),
        QStringLiteral("Blues"),
        QStringLiteral("YlOrRd"),
        QStringLiteral("YlGnBu"),
        QStringLiteral("PuBuGn"),
    };
}

QString SymbolRendererConfig::colorRampDisplayName(const QString& colorRampName)
{
    if (colorRampName == QLatin1String("RdYlGn")) {
        return QString::fromUtf8(u8"红黄绿 (RdYlGn)");
    }
    if (colorRampName == QLatin1String("Reds")) {
        return QString::fromUtf8(u8"红色系 (Reds)");
    }
    if (colorRampName == QLatin1String("Spectral")) {
        return QString::fromUtf8(u8"光谱 (Spectral)");
    }
    if (colorRampName == QLatin1String("RdYlBu")) {
        return QString::fromUtf8(u8"红黄蓝 (RdYlBu)");
    }
    if (colorRampName == QLatin1String("Blues")) {
        return QString::fromUtf8(u8"蓝色系 (Blues)");
    }
    if (colorRampName == QLatin1String("YlOrRd")) {
        return QString::fromUtf8(u8"黄橙红 (YlOrRd)");
    }
    if (colorRampName == QLatin1String("YlGnBu")) {
        return QString::fromUtf8(u8"黄绿蓝 (YlGnBu)");
    }
    if (colorRampName == QLatin1String("PuBuGn")) {
        return QString::fromUtf8(u8"紫蓝绿 (PuBuGn)");
    }
    return colorRampName;
}

// 将渲染器应用到图层并触发重绘
bool SymbolRendererConfig::applyToLayer(QgsMapLayer* layer, QVariantMap spec) {
    if (!layer) return false;
    if (layer->type() != Qgis::LayerType::Vector) return false;
    auto* vl = qobject_cast<QgsVectorLayer*>(layer);
    auto* r = buildRenderer(vl, spec);
    if (!r) return false;
    vl->setRenderer(r);
    vl->triggerRepaint();
    return true;
}
