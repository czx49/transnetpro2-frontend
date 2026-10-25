/*****************************************************************//**
 * \file   casemaputils.cpp
 * \brief  方案地图通用初始化工具实现
 * \details 统一处理 TNMapWidget 显示样式；底图由 TNMapWidget 从基础配置加载
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "casemaputils.h"

#include "../tnmapwidget/tnmapwidget.h"

#include <qgsstatusbar.h>

namespace CaseMapUtils {
void setupCaseSceneMap(TNMapWidget* mapWidget)
{
    if (!mapWidget) {
        return;
    }

    mapWidget->setMapButtonVisible(false);
    const QList<QgsStatusBar*> statusBars = mapWidget->findChildren<QgsStatusBar*>();
    for (QgsStatusBar* statusBar : statusBars) {
        if (statusBar) {
            statusBar->hide();
        }
    }
}
} // namespace CaseMapUtils
