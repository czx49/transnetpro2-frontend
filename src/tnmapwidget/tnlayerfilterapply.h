/*****************************************************************//**
 * \file   tnlayerfilterapply.h
 * \brief  将定义查询应用到矢量图层（WFS 客户端过滤 / PostGIS subset）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef TNLAYERFILTERAPPLY_H
#define TNLAYERFILTERAPPLY_H

#include "tnlayerfiltertypes.h"

#include <QString>

class QgsMapCanvas;
class QgsVectorLayer;
class TNMapWidget;

namespace TNLayerFilter {

struct ApplyDefinitionQueryResult
{
    bool success = false;
    /** @brief 应用成功但要素数未变化等软性提示（非致命） */
    QString warning;
};

/** @brief 应用或清除图层定义查询（WFS：客户端渲染过滤；PostGIS：SQL subset） */
ApplyDefinitionQueryResult applyDefinitionQuery(QgsVectorLayer* layer, const QString& subsetClause,
                                                QString* errorOut = nullptr);

/** @brief WFS 客户端显示过滤（渲染器规则 + customProperty） */
ApplyDefinitionQueryResult applyClientLayerFilter(QgsVectorLayer* layer, const QString& expression,
                                                  QString* errorOut = nullptr);

/** @brief 清除 WFS 客户端显示过滤并恢复渲染器 */
ApplyDefinitionQueryResult clearClientLayerFilter(QgsVectorLayer* layer, QString* errorOut = nullptr);

/** @brief 仅清除过滤状态，不恢复渲染器（即将被新样式整体替换时使用） */
void discardClientLayerFilterState(QgsVectorLayer* layer);

/** @brief 从 XML 备份恢复基础渲染器（移除图层或清除过滤时使用，避免 RuleBased 析构崩溃） */
void restoreBaseRendererAfterClientFilter(QgsVectorLayer* layer);

/** @brief 读取当前生效的过滤表达式（WFS 读 customProperty，其它读 subsetString） */
QString displayFilterExpression(const QgsVectorLayer* layer);

/** @brief 本地迭代统计匹配表达式的要素数 */
qlonglong countFeaturesMatchingExpression(QgsVectorLayer* layer, const QString& expression, QString* errorOut = nullptr);

/** @brief 是否存在至少一条匹配表达式的要素（setLimit(1)，避免全量扫描） */
bool hasFeaturesMatchingExpression(QgsVectorLayer* layer, const QString& expression, QString* errorOut = nullptr);

/** @brief 图层是否使用 WFS 客户端显示过滤 */
bool layerUsesClientDisplayFilter(const QgsVectorLayer* layer);

/** @brief 刷新地图画布与属性表所依赖的图层数据 */
void refreshLayerAfterDefinitionQuery(QgsVectorLayer* layer, TNMapWidget* mapWidget);

} // namespace TNLayerFilter

#endif // TNLAYERFILTERAPPLY_H
