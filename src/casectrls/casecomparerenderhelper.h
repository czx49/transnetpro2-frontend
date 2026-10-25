/*****************************************************************//**
 * \file   casecomparerenderhelper.h
 * \brief  双屏对比模式下路段渲染样式（与机动车工作台分析样式一致）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASECOMPARERENDERHELPER_H
#define CASECOMPARERENDERHELPER_H

#include "caseitem.h"

#include <QList>
#include <functional>

class CaseTreeModel;
class QgsVectorLayer;
class TNMapWidget;

enum class CompareRenderStyle
{
    None = 0,
    Saturation,
    TrafficFlow
};

namespace CaseCompareRender
{

CompareRenderStyle styleFromComboIndex(int index);
bool ensureRoadWayLayer(TNMapWidget* map, int caseId, CaseTreeModel* model, QString* wfsUriOut,
                        QString* errorOut);
bool findRoadWayLayerUri(int caseId, CaseTreeModel* model, QString* wfsUriOut);
void applyToMap(TNMapWidget* map, int caseId, CaseTreeModel* model, const QList<CaseItem>& caseList,
                CompareRenderStyle style, const std::function<void(QgsVectorLayer*, bool applied)>& onDone);

} // namespace CaseCompareRender

#endif // CASECOMPARERENDERHELPER_H
