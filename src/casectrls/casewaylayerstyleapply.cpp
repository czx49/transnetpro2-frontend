/*****************************************************************//**
 * \file   casewaylayerstyleapply.cpp
 * \brief  路段图层分析样式（流量分布 / 交通饱和度）共用应用与恢复
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casewaylayerstyleapply.h"

#include "casewaylayerhelper.h"
#include "casewfsmapcontroller.h"
#include "../tnmapwidget/layerstylefactory.h"
#include "../tnmapwidget/tnlayerfiltertypes.h"
#include "../tnmapwidget/tnmapwidget.h"

#include <qgsvectorlayer.h>

namespace CaseWayLayerStyle
{

WayLayerAnalysisStyle fromResultKey(const QString& resultKey)
{
    if (resultKey == QStringLiteral("traffic_flow") || resultKey == QStringLiteral("traffic_flow_diag")) {
        return WayLayerAnalysisStyle::TrafficFlow;
    }
    if (resultKey == QStringLiteral("saturation") || resultKey == QStringLiteral("saturation_diag")) {
        return WayLayerAnalysisStyle::TrafficSaturation;
    }
    return WayLayerAnalysisStyle::None;
}

QString graduatedFieldName(WayLayerAnalysisStyle style)
{
    switch (style) {
        case WayLayerAnalysisStyle::TrafficFlow:
            return QStringLiteral("volume");
        case WayLayerAnalysisStyle::TrafficSaturation:
            return QStringLiteral("saturation_edit");
        case WayLayerAnalysisStyle::None:
        default:
            return QString();
    }
}

bool applyAnalysisStyle(QgsVectorLayer* layer, WayLayerAnalysisStyle style,
                        const QString& trafficFlowRampName, bool trafficFlowEqualCount,
                        TNMapWidget* map)
{
    if (!layer || !layer->isValid() || style == WayLayerAnalysisStyle::None) {
        return false;
    }

    CaseWayLayer::DefaultTypeFilterSuspendGuard typeFilterGuard;
    CaseWayLayer::clearTypeFilter(layer, map, false);
    bool applied = false;
    switch (style) {
        case WayLayerAnalysisStyle::TrafficFlow:
            applied = LayerStyleFactory::applyTrafficFlowStyle(layer, trafficFlowRampName, trafficFlowEqualCount);
            break;
        case WayLayerAnalysisStyle::TrafficSaturation:
            applied = LayerStyleFactory::applyTrafficSaturationStyle(layer);
            break;
        case WayLayerAnalysisStyle::None:
        default:
            return false;
    }
    if (applied) {
        layer->setCustomProperty(TNLayerFilter::kWayAnalysisStylePropertyKey, graduatedFieldName(style));
    }
    return applied;
}

void restoreDefaultStyle(TNMapWidget* map, QgsVectorLayer* layer, bool isBaseCase)
{
    if (!map || !layer) {
        return;
    }

    layer->removeCustomProperty(TNLayerFilter::kWayAnalysisStylePropertyKey);
    CaseWfsMap::resetPanelWfsDefaultSymbols(map);
    if (isBaseCase) {
        CaseWayLayer::applyDefaultTypeFilter(layer);
    }
    if (map->canvas()) {
        map->canvas()->refresh();
    }
}

} // namespace CaseWayLayerStyle
