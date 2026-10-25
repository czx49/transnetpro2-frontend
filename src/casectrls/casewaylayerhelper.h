/*****************************************************************//**

 * \file   casewaylayerhelper.h

 * \brief  基础方案路段图层（road_way / bus_way / slow_road_way）过滤与表名解析

 *

 * \author zhangmg

 * \date   June 2026

 *********************************************************************/

#ifndef CASEWAYLAYERHELPER_H

#define CASEWAYLAYERHELPER_H



#include "caseitem.h"



#include <QString>



class QgsVectorLayer;
class TNMapWidget;



namespace CaseWayLayer {



bool isWayLayerVal(const QString& layerVal);

/** @brief 按 WFS typeName 识别路段层（val 缺失时的兜底） */

bool isWayLayerTypeName(const QString& typeName);

/** @brief 基础方案路段层是否应默认应用 type 过滤 */

bool shouldApplyDefaultTypeFilter(bool isBaseCase, const QString& layerVal, const QString& typeName);

QString defaultTypeSubsetExpression();

QString defaultTypeRowFilterExpression();



bool layerHasTypeField(QgsVectorLayer* layer);

/** @brief 默认 type 过滤包装结果（仅渲染器包装，不含要素存在性校验） */
enum class TypeFilterApplyResult
{
    NotReady = 0,   ///< 图层无效 / 字段或几何未就绪 / 换源中 / 挂起中，调用方按需重试
    Skipped,        ///< 已有分析样式或已 fail-open 回退，不应包装
    AlreadyApplied, ///< 已包装且表达式一致，无需重绘
    Applied         ///< 本次完成包装，调用方可 triggerRepaint
};

/**
 * @brief 立即把 "type" < 10 包装到渲染器（纯内存操作，无网络 I/O）
 * @note fail-open 存在性校验由 scheduleDefaultTypeFilter 在首帧后异步执行
 */
TypeFilterApplyResult applyDefaultTypeFilterNow(QgsVectorLayer* layer);

/** @brief applyDefaultTypeFilterNow 的兼容包装，忽略返回值 */
void applyDefaultTypeFilter(QgsVectorLayer* layer);

/**
 * @brief 清除路段默认 type 过滤，并剥掉 WFS URI 上的服务端 filter
 * @param map 所属地图；换 Provider 时用于冻结画布。为空则不 setDataSource
 * @param deferProviderReload true 时把换 provider 推迟到下一轮事件循环
 */
void clearTypeFilter(QgsVectorLayer* layer, TNMapWidget* map = nullptr, bool deferProviderReload = false);

/**
 * @brief 下一轮事件循环起：状态核对 / 未就绪退避重试 / 一次性异步 fail-open 校验
 * @note 仅在本次真正完成包装时 triggerRepaint，已应用不重绘
 */
void scheduleDefaultTypeFilter(QgsVectorLayer* layer);

/** @brief 分析上图/换源期间挂起默认 type 过滤，避免与分级样式抢渲染器 */
void setDefaultTypeFilterSuspended(bool suspended);
bool isDefaultTypeFilterSuspended();

class DefaultTypeFilterSuspendGuard
{
public:
    DefaultTypeFilterSuspendGuard();
    ~DefaultTypeFilterSuspendGuard();
    DefaultTypeFilterSuspendGuard(const DefaultTypeFilterSuspendGuard&) = delete;
    DefaultTypeFilterSuspendGuard& operator=(const DefaultTypeFilterSuspendGuard&) = delete;
};



QString findLayerTypeName(const CaseItem& caseItem, const QString& layerVal);

QString pgTableNameForLayer(const CaseItem& caseItem, const QString& layerVal);

/** @brief 由道路层 API value 推导 trace_tmp 的 WFS typename */
QString deriveTraceTmpTypeName(const QString& sourceWayTypeName);

/** @brief 添加要素时是否应自动预填路段属性（road_way / bus_way / slow_road_way） */
bool shouldAutoFillWayAttributes(const QString& layerVal);

/** @brief 路段层 val 推导对应点图层 val */
QString pointLayerValForWayLayerVal(const QString& wayLayerVal);

/** @brief 由路段 WFS typename 推导点图层 typename */
QString derivePointTypeName(const QString& wayTypeName);

/** @brief 从 WFS typename 反推路段层 val */
QString wayLayerValFromTypeName(const QString& typeName);

} // namespace CaseWayLayer



#endif // CASEWAYLAYERHELPER_H

