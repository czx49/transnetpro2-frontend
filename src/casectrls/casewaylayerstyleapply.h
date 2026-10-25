/*****************************************************************//**
 * \file   casewaylayerstyleapply.h
 * \brief  路段图层分析样式（流量分布 / 交通饱和度）共用应用与恢复
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEWAYLAYERSTYLEAPPLY_H
#define CASEWAYLAYERSTYLEAPPLY_H

#include <QString>

class QgsVectorLayer;
class TNMapWidget;

enum class WayLayerAnalysisStyle
{
    None = 0,
    TrafficFlow,
    TrafficSaturation
};

namespace CaseWayLayerStyle
{

WayLayerAnalysisStyle fromResultKey(const QString& resultKey);
QString graduatedFieldName(WayLayerAnalysisStyle style);
/** @brief trafficFlowRampName / trafficFlowEqualCount 仅对 TrafficFlow 生效；map 用于安全剥离 WFS URI filter */
bool applyAnalysisStyle(QgsVectorLayer* layer, WayLayerAnalysisStyle style,
                        const QString& trafficFlowRampName = QStringLiteral("RdYlGn"),
                        bool trafficFlowEqualCount = false, TNMapWidget* map = nullptr);
void restoreDefaultStyle(TNMapWidget* map, QgsVectorLayer* layer, bool isBaseCase);

} // namespace CaseWayLayerStyle

#endif // CASEWAYLAYERSTYLEAPPLY_H
