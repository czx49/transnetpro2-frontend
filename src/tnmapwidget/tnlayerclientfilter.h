/*****************************************************************//**
 * \file   tnlayerclientfilter.h
 * \brief  WFS 客户端图层显示过滤（渲染器规则包装）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef TNLAYERCLIENTFILTER_H
#define TNLAYERCLIENTFILTER_H

#include <QString>

class QgsVectorLayer;
class TNMapWidget;

namespace TNLayerFilter {

/**
 * @brief 在已去掉 tnp_wfs_type_server_filter 时，从 WFS URI 剥离 filter/cql_filter 并清除 subsetString
 * @note 不向 URI 回写客户端过滤；换 Provider 仅在 map 非空时经冻结画布执行
 */
void ensureWfsUriWithoutServerFilter(QgsVectorLayer* layer, TNMapWidget* map = nullptr);

/** @brief 用 displayFilter 包装当前渲染器为 RuleBased 规则过滤 */
bool wrapRendererWithDisplayFilter(QgsVectorLayer* layer, const QString& displayFilter, QString* errorOut);

/** @brief 从备份恢复渲染器并清除备份 */
void unwrapRenderer(QgsVectorLayer* layer);

/** @brief 清除客户端过滤属性并从 XML 备份恢复基础渲染器（移除/切换样式前调用） */
void restoreBaseRendererAfterClientFilter(QgsVectorLayer* layer);

} // namespace TNLayerFilter

#endif // TNLAYERCLIENTFILTER_H
