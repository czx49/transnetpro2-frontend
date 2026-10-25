/*****************************************************************//**
 * \file   casemapexportworker.cpp
 * \brief  子线程执行地图画布导出
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casemapexportworker.h"

#include "casemapcanvasexporter.h"

CaseMapExportWorker::CaseMapExportWorker(QObject* parent)
    : QObject(parent)
{
}

void CaseMapExportWorker::exportToFile(const QgsMapSettings& settings, const CaseMapExportOptions& options)
{
    const QString error = CaseMapCanvasExporter::renderAndSave(settings, options);
    emit finished(error.isEmpty(), error);
}
