/*****************************************************************//**
 * \file   casemapcanvasexporter.h
 * \brief  地图画布离屏渲染导出
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEMAPCANVASEXPORTER_H
#define CASEMAPCANVASEXPORTER_H

#include "casemapexportdialog.h"

#include <QSize>
#include <QString>

class TNMapWidget;
class QgsMapSettings;
class QgsRectangle;

class CaseMapCanvasExporter
{
public:
    /** @brief 计算导出范围 */
    static QgsRectangle computeExtent(TNMapWidget* mapWidget, const CaseMapExportOptions& options);

    /** @brief 计算输出像素尺寸 */
    static QSize computeOutputSize(TNMapWidget* mapWidget, int dpi);

    /** @brief 构建完整导出 MapSettings（含 extent / size / dpi） */
    static QgsMapSettings buildExportSettings(TNMapWidget* mapWidget, const CaseMapExportOptions& options);

    /** @brief 渲染并保存文件；成功返回空字符串，失败返回错误信息 */
    static QString exportToFile(TNMapWidget* mapWidget, const CaseMapExportOptions& options);

    /** @brief 仅渲染（供子线程调用，mapWidget 仅用于图层栈，不访问 canvas 控件） */
    static QString renderAndSave(const QgsMapSettings& settings, const CaseMapExportOptions& options);
};

#endif // CASEMAPCANVASEXPORTER_H
