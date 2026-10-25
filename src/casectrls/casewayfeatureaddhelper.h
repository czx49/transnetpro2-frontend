/*****************************************************************//**
 * \file   casewayfeatureaddhelper.h
 * \brief  路段图层添加要素时的属性预填（节点、link_id、长度等）
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#ifndef CASEWAYFEATUREADDHELPER_H
#define CASEWAYFEATUREADDHELPER_H

#include <functional>

#include <QString>
#include <QStringList>

class BaseDataTableView;
class CaseTreeView;
class TNMapWidget;
class QgsFeature;
class QgsVectorLayer;

namespace CaseWayFeatureAdd {

/** @brief 激活添加要素工具前：为 road_way / slow_road_way 自动勾选对应点图层 */
void prepareNodeLayerForWayAdd(TNMapWidget* map,
                               CaseTreeView* schemeTree,
                               BaseDataTableView* baseDataView,
                               int caseId);

/** @brief 绘制完成后、属性弹窗前预填路段字段 */
void prefillWayAttributes(TNMapWidget* map,
                          QgsVectorLayer* wayLayer,
                          QgsFeature& feature,
                          QString* warning = nullptr,
                          QStringList* lockedFieldNames = nullptr);

using PrefillHandler = std::function<void(QgsVectorLayer* layer, QgsFeature& feature, QStringList* readOnlyFieldNames)>;

PrefillHandler makePrefillHandler(TNMapWidget* map, QString* warningOut = nullptr);

/** @brief 清除节点空间索引静态缓存（地图/项目关闭时调用） */
void clearNodeIndexCache();

} // namespace CaseWayFeatureAdd

#endif // CASEWAYFEATUREADDHELPER_H
