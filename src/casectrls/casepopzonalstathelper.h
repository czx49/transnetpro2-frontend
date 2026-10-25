/*****************************************************************//**
 * \file   casepopzonalstathelper.h
 * \brief  基础数据人口栅格分区统计（简化版 QGIS 分区统计）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEPOPZONALSTATHELPER_H
#define CASEPOPZONALSTATHELPER_H

#include <QList>
#include <QString>
#include <QVariant>

class QgsVectorLayer;

struct CasePopZonalStatRecord {
    QVariant idValue;
    double populationSum = 0.0;
};

struct CasePopZonalStatResult {
    QString idFieldName;
    QString outputPrefix;
    QString rasterPath;
    QString layerTypeName;
    QList<CasePopZonalStatRecord> records;
    int skippedCount = 0;
    QString errorMessage;
};

/**
 * \brief  人口栅格分区统计辅助
 * \details 按小区多边形对单波段栅格求和，关联字段固定为 area_id，输出字段 pop_sum
 * \author zhangmg
 * \date   June 2026
 */
class CasePopZonalStatHelper {
public:
    static QString popIdFieldName();
    static QString popOutputPrefix();
    static QString popSumFieldName();

    static QString defaultPopRasterFileName();
    static QString defaultPopRasterPath();
    static QString defaultPopRasterDisplayLabel();

    static bool hasPopIdField(QgsVectorLayer* layer);

    static QString formatRasterBandDisplayText(const QString& rasterPath, int bandIndex = 1);

    static CasePopZonalStatResult run(QgsVectorLayer* communityLayer, const QString& rasterPath);

    static QVariantList buildPopDataRequestArray(const QList<CasePopZonalStatRecord>& records);
};

#endif // CASEPOPZONALSTATHELPER_H
