/*****************************************************************//**
 * \file   casemaputils.h
 * \brief  方案地图通用初始化工具
 * \details 统一接入 TNMapWidget 的场景样式（工具栏/状态栏）；底图来自服务端配置
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef CASEMAPUTILS_H
#define CASEMAPUTILS_H

class TNMapWidget;

namespace CaseMapUtils {
void setupCaseSceneMap(TNMapWidget* mapWidget);
} // namespace CaseMapUtils

#endif // CASEMAPUTILS_H
