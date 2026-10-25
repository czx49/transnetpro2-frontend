/*****************************************************************//**
 * \file   casemapexportworker.h
 * \brief  子线程执行地图画布导出
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEMAPEXPORTWORKER_H
#define CASEMAPEXPORTWORKER_H

#include "casemapexportdialog.h"

#include <QObject>
#include <QString>

#include <qgsmapsettings.h>

class CaseMapExportWorker : public QObject
{
    Q_OBJECT

public:
    explicit CaseMapExportWorker(QObject* parent = nullptr);

public slots:
    void exportToFile(const QgsMapSettings& settings, const CaseMapExportOptions& options);

signals:
    void finished(bool success, const QString& errorMessage);
};

#endif // CASEMAPEXPORTWORKER_H
