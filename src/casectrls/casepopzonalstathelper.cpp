/*****************************************************************//**
 * \file   casepopzonalstathelper.cpp
 * \brief  基础数据人口栅格分区统计
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casepopzonalstathelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <qgsfeatureiterator.h>
#include <qgscoordinatetransform.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrasterdataprovider.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>
#include <qgszonalstatistics.h>

namespace {

const QString kPopIdField = QStringLiteral("area_id");
const QString kPopOutputPrefix = QStringLiteral("pop");
const QString kPopSumField = QStringLiteral("pop_sum");

QString applicationBaseDir()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    if (dir.dirName() == QStringLiteral("depends")) {
        dir.cdUp();
        appDir = dir.absolutePath();
    }
    return appDir;
}

bool fieldValueIsEmpty(const QVariant& value)
{
    return !value.isValid() || value.isNull()
        || (value.type() == QVariant::String && value.toString().trimmed().isEmpty());
}

QString colorInterpretationLabel(Qgis::RasterColorInterpretation interpretation)
{
    switch (interpretation) {
    case Qgis::RasterColorInterpretation::GrayIndex:
        return QString::fromUtf8(u8"灰度");
    case Qgis::RasterColorInterpretation::RedBand:
        return QString::fromUtf8(u8"红");
    case Qgis::RasterColorInterpretation::GreenBand:
        return QString::fromUtf8(u8"绿");
    case Qgis::RasterColorInterpretation::BlueBand:
        return QString::fromUtf8(u8"蓝");
    case Qgis::RasterColorInterpretation::AlphaBand:
        return QString::fromUtf8(u8"Alpha");
    case Qgis::RasterColorInterpretation::PaletteIndex:
        return QString::fromUtf8(u8"调色板");
    case Qgis::RasterColorInterpretation::ContinuousPalette:
        return QString::fromUtf8(u8"连续调色板");
    default:
        return QString::fromUtf8(u8"灰度");
    }
}

} // namespace

QString CasePopZonalStatHelper::popIdFieldName()
{
    return kPopIdField;
}

QString CasePopZonalStatHelper::popOutputPrefix()
{
    return kPopOutputPrefix;
}

QString CasePopZonalStatHelper::popSumFieldName()
{
    return kPopSumField;
}

QString CasePopZonalStatHelper::defaultPopRasterFileName()
{
    return QStringLiteral("chn_pop_2024_CN_100m_R2025A_v1.tif");
}

QString CasePopZonalStatHelper::defaultPopRasterPath()
{
    return QDir(applicationBaseDir()).filePath(QStringLiteral("data/%1").arg(defaultPopRasterFileName()));
}

QString CasePopZonalStatHelper::defaultPopRasterDisplayLabel()
{
    return QString::fromUtf8(u8"2024人口数据");
}

bool CasePopZonalStatHelper::hasPopIdField(QgsVectorLayer* layer)
{
    return layer && layer->isValid() && layer->fields().indexFromName(kPopIdField) >= 0;
}

QString CasePopZonalStatHelper::formatRasterBandDisplayText(const QString& rasterPath, int bandIndex)
{
    const QFileInfo fileInfo(rasterPath);
    if (!fileInfo.exists()) {
        return QString::fromUtf8(u8"栅格文件不存在：%1").arg(rasterPath);
    }

    QgsRasterLayer rasterLayer(rasterPath, fileInfo.completeBaseName(), QStringLiteral("gdal"));
    if (!rasterLayer.isValid()) {
        return QString::fromUtf8(u8"无法打开栅格：%1").arg(rasterPath);
    }
    if (rasterLayer.bandCount() < bandIndex) {
        return QString::fromUtf8(u8"栅格波段无效");
    }

    const QString rasterName = rasterLayer.name().trimmed().isEmpty() ? fileInfo.completeBaseName()
                                                                       : rasterLayer.name().trimmed();
    Qgis::RasterColorInterpretation interpretation = Qgis::RasterColorInterpretation::GrayIndex;
    if (QgsRasterDataProvider* provider = rasterLayer.dataProvider()) {
        interpretation = provider->colorInterpretation(bandIndex);
    }

    return QString::fromUtf8(u8"波段 %1: %2 (%3)")
        .arg(bandIndex)
        .arg(rasterName)
        .arg(colorInterpretationLabel(interpretation));
}

CasePopZonalStatResult CasePopZonalStatHelper::run(QgsVectorLayer* communityLayer, const QString& rasterPath)
{
    CasePopZonalStatResult result;
    result.rasterPath = rasterPath;
    result.idFieldName = kPopIdField;
    result.outputPrefix = kPopOutputPrefix;

    if (!communityLayer || !communityLayer->isValid()) {
        result.errorMessage = QString::fromUtf8(u8"小区图层无效。");
        return result;
    }

    result.layerTypeName = communityLayer->name();

    const QFileInfo rasterInfo(rasterPath);
    if (!rasterInfo.exists()) {
        result.errorMessage = QString::fromUtf8(u8"人口栅格文件不存在：%1").arg(rasterPath);
        return result;
    }

    QgsRasterLayer rasterLayer(rasterPath, rasterInfo.completeBaseName(), QStringLiteral("gdal"));
    if (!rasterLayer.isValid()) {
        result.errorMessage = QString::fromUtf8(u8"无法打开人口栅格。");
        return result;
    }
    if (rasterLayer.bandCount() != 1) {
        result.errorMessage = QString::fromUtf8(u8"人口栅格必须仅包含 1 个波段。");
        return result;
    }

    const int idFieldIndex = communityLayer->fields().indexFromName(kPopIdField);
    if (idFieldIndex < 0) {
        result.errorMessage = QString::fromUtf8(u8"小区图层缺少 area_id 字段。");
        return result;
    }

    QgsRasterDataProvider* rasterProvider = rasterLayer.dataProvider();
    if (!rasterProvider) {
        result.errorMessage = QString::fromUtf8(u8"栅格数据提供者无效。");
        return result;
    }

    const double cellSizeX = rasterLayer.rasterUnitsPerPixelX();
    const double cellSizeY = rasterLayer.rasterUnitsPerPixelY();
    const QgsCoordinateReferenceSystem rasterCrs = rasterLayer.crs();
    const QgsCoordinateReferenceSystem layerCrs = communityLayer->crs();

    QgsCoordinateTransform toRasterTransform(layerCrs, rasterCrs, QgsProject::instance());
    toRasterTransform.setBallparkTransformsAreAppropriate(true);

    QgsFeatureIterator iterator = communityLayer->getFeatures();
    QgsFeature feature;
    while (iterator.nextFeature(feature)) {
        const QVariant idValue = feature.attribute(idFieldIndex);
        if (fieldValueIsEmpty(idValue)) {
            ++result.skippedCount;
            continue;
        }

        QgsGeometry geometry = feature.geometry();
        if (geometry.isNull() || geometry.isEmpty()) {
            ++result.skippedCount;
            continue;
        }

        try {
            geometry = QgsGeometry(geometry);
            geometry.transform(toRasterTransform);
        } catch (const QgsCsException&) {
            ++result.skippedCount;
            continue;
        }

        const QMap<Qgis::ZonalStatistic, QVariant> stats = QgsZonalStatistics::calculateStatistics(
            rasterProvider,
            geometry,
            cellSizeX,
            cellSizeY,
            1,
            Qgis::ZonalStatistic::Sum);

        const QVariant sumValue = stats.value(Qgis::ZonalStatistic::Sum);
        CasePopZonalStatRecord record;
        record.idValue = idValue;
        record.populationSum = sumValue.isValid() ? sumValue.toDouble() : 0.0;
        result.records.append(record);
    }

    if (result.records.isEmpty()) {
        result.errorMessage = QString::fromUtf8(u8"未计算出有效人口数据，请确认小区图层已加载要素。");
    }

    return result;
}

QVariantList CasePopZonalStatHelper::buildPopDataRequestArray(const QList<CasePopZonalStatRecord>& records)
{
    QVariantList dataList;
    dataList.reserve(records.size());
    for (const CasePopZonalStatRecord& record : records) {
        QVariantMap item;
        item.insert(kPopIdField, record.idValue.toLongLong());
        item.insert(kPopSumField, record.populationSum);
        dataList.append(item);
    }
    return dataList;
}
