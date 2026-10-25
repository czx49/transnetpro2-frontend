/*****************************************************************//**
 * \file   caseotherodmaphelper.h
 * \brief  other_od 图层地图加载与 demand 分级渲染（中性辅助，不含业务树逻辑）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEOTHERODMAPHELPER_H
#define CASEOTHERODMAPHELPER_H

#include <functional>

#include <QString>

class TNMapWidget;

namespace CaseOtherOdMapHelper {

/**
 * \brief  在指定地图上确保 WFS 图层已加载并应用 demand 分级样式
 *
 * 图层已在 map 上则 reload 后样式化；否则通过 triggerLoad 触发 WFS 入队，
 * 队列空闲后取层并样式化。triggerLoad 由调用方负责勾选侧栏节点等加载触发。
 *
 * \param  mapWidget    目标地图
 * \param  wfsTypeName  WFS typename（完整表名）
 * \param  triggerLoad  触发图层加载，返回树中是否找到对应节点
 * \param  done         完成回调 (success, errorMessage)
 * \author zhangmg
 * \date   June 2026
 */
void loadOrReloadAndApplyDemandStyle(
    TNMapWidget* mapWidget,
    const QString& wfsTypeName,
    const std::function<bool()>& triggerLoad,
    const std::function<void(bool success, const QString& error)>& done);

} // namespace CaseOtherOdMapHelper

#endif // CASEOTHERODMAPHELPER_H
