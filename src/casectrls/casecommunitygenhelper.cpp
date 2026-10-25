/*****************************************************************//**
 * \file   casecommunitygenhelper.cpp
 * \brief  生成交通小区本地处理
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casecommunitygenhelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include <qgsabstractgeometry.h>
#include <qgsfield.h>
#include <qgsfeatureiterator.h>
#include <qgscoordinatetransform.h>
#include <qgsgeometry.h>
#include <qgsgeometryengine.h>
#include <qgsjsonutils.h>
#include <qgsmultipolygon.h>
#include <qgspolygon.h>
#include <qgsproject.h>
#include <qgsrasterdataprovider.h>
#include <qgsrasterlayer.h>
#include <qgsspatialindex.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgswkbtypes.h>
#include <qgszonalstatistics.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <queue>
#include <vector>
#include <limits>
#include <memory>

namespace {

const int kBufferSegments = 8;
const double kMinClipAreaRatio = 0.05;
const int kMaxGridWarningCount = 50000;

// 小路网保护：最少生成的交通小区数（内部固定值，不对外配置）。
const int kMinTazCount = 2;

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

void reportProgress(const CaseCommunityGenProgressCallback& progress, CaseCommunityGenStep step,
                    const QString& message, int percent)
{
    if (progress) {
        progress(step, message, percent);
    }
}

int mapProgressRange(int startPercent, int endPercent, double ratio)
{
    if (ratio <= 0.0) {
        return startPercent;
    }
    if (ratio >= 1.0) {
        return endPercent;
    }
    return startPercent + static_cast<int>(std::lround((endPercent - startPercent) * ratio));
}

QString clipMaskProgressMessage(const char* stageSuffixUtf8)
{
    return QString::fromUtf8(u8"路网缓冲与填洞…（") + QString::fromUtf8(stageSuffixUtf8)
        + QString::fromUtf8(u8"）");
}

QgsCoordinateReferenceSystem projectedCrsForCenterLon(double centerLon)
{
    const int zone = static_cast<int>(std::floor((centerLon + 1.5) / 3.0));
    const double centralMeridian = zone * 3.0;
    const int epsgId = 4544 + (zone - 25);
    if (epsgId >= 4544 && epsgId <= 4554) {
        QgsCoordinateReferenceSystem crs = QgsCoordinateReferenceSystem::fromEpsgId(epsgId);
        if (crs.isValid()) {
            return crs;
        }
    }

    const QString proj = QStringLiteral("+proj=tmerc +lat_0=0 +lon_0=%1 +k=1 +x_0=500000 +y_0=0 +ellps=GRS80 +units=m +no_defs")
                             .arg(centralMeridian, 0, 'f', 1);
    return QgsCoordinateReferenceSystem::fromProj(proj);
}

QgsGeometry transformGeometry(const QgsGeometry& geometry, const QgsCoordinateTransform& transform,
                              bool* okOut = nullptr)
{
    if (geometry.isNull() || geometry.isEmpty()) {
        if (okOut) {
            *okOut = false;
        }
        return QgsGeometry();
    }

    QgsGeometry copy(geometry);
    try {
        copy.transform(transform);
        if (okOut) {
            *okOut = true;
        }
        return copy;
    } catch (const QgsCsException&) {
        if (okOut) {
            *okOut = false;
        }
        return QgsGeometry();
    }
}

#ifdef TNP_COMMUNITY_GEN_FILL_HOLES
QgsGeometry deleteHoles(const QgsGeometry& geometry, const CaseCommunityGenProgressCallback& progress)
{
    if (geometry.isNull() || geometry.isEmpty()) {
        return geometry;
    }

    std::unique_ptr<QgsAbstractGeometry> abstract(geometry.constGet()->clone());
    if (!abstract) {
        return geometry;
    }

    const QString fillMessage = clipMaskProgressMessage(u8"填洞");
    int lastReported = -1;
    auto reportFill = [&](int percent) {
        if (percent == lastReported) {
            return;
        }
        lastReported = percent;
        reportProgress(progress, CaseCommunityGenStep::ClipMask, fillMessage, percent);
    };

    reportFill(65);

    if (QgsPolygon* poly = qgsgeometry_cast<QgsPolygon*>(abstract.get())) {
        const int ringCount = poly->numInteriorRings();
        int removed = 0;
        while (poly->numInteriorRings() > 0) {
            poly->removeInteriorRing(0);
            ++removed;
            if (ringCount > 0) {
                reportFill(mapProgressRange(65, 80, static_cast<double>(removed) / ringCount));
            }
        }
    } else if (QgsMultiPolygon* multiPoly = qgsgeometry_cast<QgsMultiPolygon*>(abstract.get())) {
        const int partCount = multiPoly->numGeometries();
        for (int i = 0; i < partCount; ++i) {
            if (QgsPolygon* part = qgsgeometry_cast<QgsPolygon*>(multiPoly->geometryN(i))) {
                while (part->numInteriorRings() > 0) {
                    part->removeInteriorRing(0);
                }
            }
            if (partCount > 0) {
                reportFill(mapProgressRange(65, 80, static_cast<double>(i + 1) / partCount));
            }
        }
    }

    reportFill(80);
    return QgsGeometry(abstract.release());
}
#endif // TNP_COMMUNITY_GEN_FILL_HOLES

QgsGeometry collectRoadGeometries(QgsVectorLayer* roadWayLayer, const QgsCoordinateTransform& toProjected,
                                  QString* errorOut, const CaseCommunityGenProgressCallback& progress)
{
    QgsFeatureIterator iterator = roadWayLayer->getFeatures();
    QgsFeature feature;
    const qint64 featureCount = roadWayLayer->featureCount();
    const int total = featureCount > 0 ? static_cast<int>(featureCount) : 0;
    QVector<QgsGeometry> parts;
    parts.reserve(total > 0 ? total : 64);

    const QString collectMessage = clipMaskProgressMessage(u8"采集");
    int lastReported = -1;
    int processed = 0;
    reportProgress(progress, CaseCommunityGenStep::ClipMask, collectMessage, 0);

    while (iterator.nextFeature(feature)) {
        ++processed;
        QgsGeometry geometry = feature.geometry();
        if (geometry.isNull() || geometry.isEmpty()) {
            // still advance progress for scanned features
        } else {
            bool ok = false;
            geometry = transformGeometry(geometry, toProjected, &ok);
            if (ok) {
                parts.append(geometry);
            }
        }

        if (total > 0) {
            const int percent =
                mapProgressRange(0, 40, static_cast<double>(processed) / static_cast<double>(total));
            if (percent != lastReported) {
                lastReported = percent;
                reportProgress(progress, CaseCommunityGenStep::ClipMask, collectMessage, percent);
            }
        } else if (processed % 64 == 0) {
            // featureCount unknown: keep under collect range end until loop finishes
            const int percent = std::min(39, lastReported < 0 ? 1 : lastReported + 1);
            if (percent != lastReported) {
                lastReported = percent;
                reportProgress(progress, CaseCommunityGenStep::ClipMask, collectMessage, percent);
            }
        }
    }

    reportProgress(progress, CaseCommunityGenStep::ClipMask, collectMessage, 40);

    if (parts.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"road_way 图层无有效线要素。");
        }
        return QgsGeometry();
    }

    if (parts.size() == 1) {
        reportProgress(progress, CaseCommunityGenStep::ClipMask, clipMaskProgressMessage(u8"合并"), 50);
        return parts.first();
    }

    const QString mergeMessage = clipMaskProgressMessage(u8"合并");
    reportProgress(progress, CaseCommunityGenStep::ClipMask, mergeMessage, 40);
    QgsGeometry merged = QgsGeometry::unaryUnion(parts);
    if (merged.isNull() || merged.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"road_way 线几何合并失败。");
        }
        return QgsGeometry();
    }
    reportProgress(progress, CaseCommunityGenStep::ClipMask, mergeMessage, 50);
    return merged;
}

QgsGeometry buildClipMask(QgsVectorLayer* roadWayLayer, double bufferHalfWidthM,
                          const QgsCoordinateReferenceSystem& projectedCrs, QString* errorOut,
                          const CaseCommunityGenProgressCallback& progress)
{
    const QgsCoordinateReferenceSystem sourceCrs = roadWayLayer->crs();
    QgsCoordinateTransform toProjected(sourceCrs, projectedCrs, QgsProject::instance());
    toProjected.setBallparkTransformsAreAppropriate(true);

    const QgsGeometry mergedLines = collectRoadGeometries(roadWayLayer, toProjected, errorOut, progress);
    if (mergedLines.isNull() || mergedLines.isEmpty()) {
        return QgsGeometry();
    }

    const QString bufferMessage = clipMaskProgressMessage(u8"缓冲");
    reportProgress(progress, CaseCommunityGenStep::ClipMask, bufferMessage, 50);
    QgsGeometry buffered = mergedLines.buffer(bufferHalfWidthM, kBufferSegments);
    if (buffered.isNull() || buffered.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"路网缓冲失败。");
        }
        return QgsGeometry();
    }
    reportProgress(progress, CaseCommunityGenStep::ClipMask, bufferMessage, 65);

#ifdef TNP_COMMUNITY_GEN_FILL_HOLES
    buffered = deleteHoles(buffered, progress);
    if (buffered.isNull() || buffered.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"缓冲面填洞后为空。");
        }
        return QgsGeometry();
    }
#endif // TNP_COMMUNITY_GEN_FILL_HOLES
    return buffered;
}

struct GridCell {
    long long gridOid = 0;
    double popSum = 0.0;
    QgsGeometry geometry;
};

std::unique_ptr<QgsVectorLayer> createMemoryPolygonLayer(const QgsCoordinateReferenceSystem& crs,
                                                         const QString& layerName)
{
    const QString uri = QStringLiteral("Polygon?crs=%1").arg(crs.authid());
    auto layer = std::make_unique<QgsVectorLayer>(uri, layerName, QStringLiteral("memory"));
    if (!layer->isValid()) {
        return nullptr;
    }
    return layer;
}

bool writeDebugLayer(QgsVectorLayer* layer, const QString& fileName, const QgsCoordinateReferenceSystem& crs)
{
#ifdef TNP_COMMUNITY_GEN_DEBUG_SHP
    if (!layer || !layer->isValid()) {
        return false;
    }

    const QString outDir = QDir(applicationBaseDir()).filePath(QStringLiteral("data"));
    QDir().mkpath(outDir);
    const QString outPath = QDir(outDir).filePath(fileName);

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("ESRI Shapefile");
    options.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;

    QString errorMessage;
    const QgsVectorFileWriter::WriterError err = QgsVectorFileWriter::writeAsVectorFormatV3(
        layer,
        outPath,
        QgsProject::instance()->transformContext(),
        options,
        &errorMessage,
        nullptr,
        nullptr);
    return err == QgsVectorFileWriter::WriterError::NoError;
#else
    Q_UNUSED(layer)
    Q_UNUSED(fileName)
    Q_UNUSED(crs)
    return true;
#endif
}

bool writeDebugGeometryOnly(const QgsGeometry& geometry, const QString& fileName,
                            const QgsCoordinateReferenceSystem& crs)
{
#ifdef TNP_COMMUNITY_GEN_DEBUG_SHP
    auto layer = createMemoryPolygonLayer(crs, fileName);
    if (!layer) {
        return false;
    }

    QgsFeatureList features;
    QgsFeature feature;
    feature.setGeometry(geometry);
    features.append(feature);
    layer->dataProvider()->addFeatures(features);
    return writeDebugLayer(layer.get(), fileName, crs);
#else
    Q_UNUSED(geometry)
    Q_UNUSED(fileName)
    Q_UNUSED(crs)
    return true;
#endif
}

bool writeDebugGridCells(const QList<GridCell>& cells, const QString& fileName,
                         const QgsCoordinateReferenceSystem& crs)
{
#ifdef TNP_COMMUNITY_GEN_DEBUG_SHP
    auto layer = createMemoryPolygonLayer(crs, fileName);
    if (!layer) {
        return false;
    }

    QList<QgsField> attrFields;
    attrFields.append(QgsField(QStringLiteral("grid_oid"), QMetaType::LongLong));
    attrFields.append(QgsField(QStringLiteral("pop_sum"), QMetaType::Double));
    layer->dataProvider()->addAttributes(attrFields);
    layer->updateFields();

    const int gridOidIdx = layer->fields().indexFromName(QStringLiteral("grid_oid"));
    const int popSumIdx = layer->fields().indexFromName(QStringLiteral("pop_sum"));

    QgsFeatureList features;
    features.reserve(cells.size());
    for (const GridCell& cell : cells) {
        if (cell.geometry.isNull() || cell.geometry.isEmpty()) {
            continue;
        }
        QgsFeature feature(layer->fields());
        feature.setGeometry(cell.geometry);
        feature.setAttribute(gridOidIdx, QVariant::fromValue(cell.gridOid));
        feature.setAttribute(popSumIdx, cell.popSum);
        features.append(feature);
    }
    layer->dataProvider()->addFeatures(features);
    return writeDebugLayer(layer.get(), fileName, crs);
#else
    Q_UNUSED(cells)
    Q_UNUSED(fileName)
    Q_UNUSED(crs)
    return true;
#endif
}

bool writeDebugTazRecords(const QList<CaseCommunityGenRecord>& records, const QString& fileName,
                          const QgsCoordinateReferenceSystem& projectedCrs,
                          const QgsCoordinateReferenceSystem& wgs84Crs)
{
#ifdef TNP_COMMUNITY_GEN_DEBUG_SHP
    auto layer = createMemoryPolygonLayer(projectedCrs, fileName);
    if (!layer) {
        return false;
    }

    QList<QgsField> attrFields;
    attrFields.append(QgsField(QStringLiteral("area_id"), QMetaType::Int));
    attrFields.append(QgsField(QStringLiteral("pop_sum"), QMetaType::Double));
    attrFields.append(QgsField(QStringLiteral("num_grids"), QMetaType::Int));
    attrFields.append(QgsField(QStringLiteral("seed_oid"), QMetaType::LongLong));
    layer->dataProvider()->addAttributes(attrFields);
    layer->updateFields();

    const int areaIdIdx = layer->fields().indexFromName(QStringLiteral("area_id"));
    const int popSumIdx = layer->fields().indexFromName(QStringLiteral("pop_sum"));
    const int numGridsIdx = layer->fields().indexFromName(QStringLiteral("num_grids"));
    const int seedOidIdx = layer->fields().indexFromName(QStringLiteral("seed_oid"));

    QgsCoordinateTransform toProjected(wgs84Crs, projectedCrs, QgsProject::instance());
    toProjected.setBallparkTransformsAreAppropriate(true);

    QgsFeatureList features;
    features.reserve(records.size());
    for (const CaseCommunityGenRecord& record : records) {
        QgsGeometry geometry =
            QgsJsonUtils::geometryFromGeoJson(QString::fromUtf8(record.geometryGeoJsonUtf8));
        if (geometry.isNull() || geometry.isEmpty()) {
            continue;
        }
        bool ok = false;
        geometry = transformGeometry(geometry, toProjected, &ok);
        if (!ok || geometry.isNull() || geometry.isEmpty()) {
            continue;
        }

        QgsFeature feature(layer->fields());
        feature.setGeometry(geometry);
        feature.setAttribute(areaIdIdx, record.areaId);
        feature.setAttribute(popSumIdx, record.popSum);
        feature.setAttribute(numGridsIdx, record.numGrids);
        feature.setAttribute(seedOidIdx, QVariant::fromValue(record.seedOid));
        features.append(feature);
    }
    layer->dataProvider()->addFeatures(features);
    return writeDebugLayer(layer.get(), fileName, projectedCrs);
#else
    Q_UNUSED(records)
    Q_UNUSED(fileName)
    Q_UNUSED(projectedCrs)
    Q_UNUSED(wgs84Crs)
    return true;
#endif
}

QList<GridCell> buildFishnet(const QgsGeometry& clipMask, double cellSizeM, QString* errorOut)
{
    QList<GridCell> cells;
    if (clipMask.isNull() || clipMask.isEmpty() || cellSizeM <= 0.0) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"格网参数无效。");
        }
        return cells;
    }

    const QgsRectangle extent = clipMask.boundingBox();
    if (extent.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"缓冲面范围无效。");
        }
        return cells;
    }

    const int cols = static_cast<int>(std::ceil(extent.width() / cellSizeM));
    const int rows = static_cast<int>(std::ceil(extent.height() / cellSizeM));
    if (cols <= 0 || rows <= 0) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"格网行列数无效。");
        }
        return cells;
    }

    const qint64 estimatedCount = static_cast<qint64>(cols) * static_cast<qint64>(rows);
    if (estimatedCount > kMaxGridWarningCount) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"预计格网数 %1 过大，请增大格网边长或缩小路网范围。")
                             .arg(estimatedCount);
        }
        return cells;
    }

    cells.reserve(static_cast<int>(estimatedCount));
    long long oid = 1;
    const double xmin = extent.xMinimum();
    const double ymin = extent.yMinimum();

    for (int row = 0; row < rows; ++row) {
        const double y1 = ymin + row * cellSizeM;
        const double y2 = y1 + cellSizeM;
        for (int col = 0; col < cols; ++col) {
            const double x1 = xmin + col * cellSizeM;
            const double x2 = x1 + cellSizeM;
            GridCell cell;
            cell.gridOid = oid++;
            cell.geometry = QgsGeometry::fromRect(QgsRectangle(x1, y1, x2, y2));
            cells.append(cell);
        }
    }
    return cells;
}

void assignPopulationToGrid(QList<GridCell>& cells, const QString& rasterPath,
                            const QgsCoordinateReferenceSystem& projectedCrs, QString* errorOut)
{
    const QFileInfo rasterInfo(rasterPath);
    QgsRasterLayer rasterLayer(rasterPath, rasterInfo.completeBaseName(), QStringLiteral("gdal"));
    if (!rasterLayer.isValid()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"无法打开人口栅格。");
        }
        return;
    }

    QgsRasterDataProvider* provider = rasterLayer.dataProvider();
    if (!provider) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"栅格数据提供者无效。");
        }
        return;
    }

    const double cellSizeX = rasterLayer.rasterUnitsPerPixelX();
    const double cellSizeY = rasterLayer.rasterUnitsPerPixelY();
    const QgsCoordinateReferenceSystem rasterCrs = rasterLayer.crs();
    QgsCoordinateTransform toRasterTransform(projectedCrs, rasterCrs, QgsProject::instance());
    toRasterTransform.setBallparkTransformsAreAppropriate(true);

    for (GridCell& cell : cells) {
        QgsGeometry geometry = cell.geometry;
        if (geometry.isNull() || geometry.isEmpty()) {
            continue;
        }
        try {
            geometry.transform(toRasterTransform);
        } catch (const QgsCsException&) {
            continue;
        }

        const QMap<Qgis::ZonalStatistic, QVariant> stats = QgsZonalStatistics::calculateStatistics(
            provider, geometry, cellSizeX, cellSizeY, 1, Qgis::ZonalStatistic::Sum);
        const QVariant sumValue = stats.value(Qgis::ZonalStatistic::Sum);
        cell.popSum = sumValue.isValid() ? sumValue.toDouble() : 0.0;
    }
}

QList<GridCell> clipGridByMask(const QList<GridCell>& fishnet, const QgsGeometry& clipMask, QString* errorOut)
{
    QList<GridCell> clipped;
    if (clipMask.isNull() || clipMask.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"裁剪掩膜无效。");
        }
        return clipped;
    }

    std::unique_ptr<QgsGeometryEngine> engine(QgsGeometry::createGeometryEngine(clipMask.constGet()));
    if (!engine) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"裁剪掩膜引擎创建失败。");
        }
        return clipped;
    }
    engine->prepareGeometry();

    clipped.reserve(fishnet.size());
    for (const GridCell& cell : fishnet) {
        if (cell.geometry.isNull() || cell.geometry.isEmpty()) {
            continue;
        }
        if (!engine->intersects(cell.geometry.constGet())) {
            continue;
        }

        QgsGeometry intersection = clipMask.intersection(cell.geometry);
        if (intersection.isNull() || intersection.isEmpty()) {
            continue;
        }

        const double originalArea = cell.geometry.area();
        const double clippedArea = intersection.area();
        if (originalArea > 0.0 && clippedArea / originalArea < kMinClipAreaRatio) {
            continue;
        }

        GridCell out = cell;
        out.geometry = intersection;
        clipped.append(out);
    }

    if (clipped.isEmpty() && errorOut) {
        *errorOut = QString::fromUtf8(u8"裁剪后无有效格网。");
    }
    return clipped;
}

bool geometriesTouch(const QgsGeometry& a, const QgsGeometry& b)
{
    if (a.isNull() || b.isNull() || a.isEmpty() || b.isEmpty()) {
        return false;
    }
    return a.touches(b) || a.intersects(b);
}

struct RegionGroup {
    QList<long long> gridOids;
    long long seedOid = 0;
    double popSum = 0.0;
};

QList<RegionGroup> regionGrowClusters(const QList<GridCell>& gridCells, double targetPopulation,
                                      double populationCapFactor)
{
    const int cellCount = static_cast<int>(gridCells.size());
    if (cellCount == 0) {
        return {};
    }

    QHash<long long, GridCell> cellByOid;
    cellByOid.reserve(cellCount);
    for (const GridCell& cell : gridCells) {
        cellByOid.insert(cell.gridOid, cell);
    }

    // 等距水滴扩张：质心一次性预计算（格网几何已处于米制投影坐标系）。
    QHash<long long, QPair<double, double>> centroidByOid;
    centroidByOid.reserve(cellCount);
    for (const GridCell& cell : gridCells) {
        const QgsGeometry centroidGeometry = cell.geometry.centroid();
        const QgsPointXY centroid = (centroidGeometry.isNull() || centroidGeometry.isEmpty())
                                        ? cell.geometry.boundingBox().center()
                                        : centroidGeometry.asPoint();
        centroidByOid.insert(cell.gridOid, qMakePair(centroid.x(), centroid.y()));
    }

    QgsSpatialIndex spatialIndex;
    QHash<QgsFeatureId, long long> featureIdToOid;
    featureIdToOid.reserve(cellCount);
    long long featureCounter = 1;
    for (const GridCell& cell : gridCells) {
        QgsFeature feature(featureCounter);
        feature.setGeometry(cell.geometry);
        spatialIndex.addFeature(feature);
        featureIdToOid.insert(featureCounter, cell.gridOid);
        ++featureCounter;
    }

    // 预计算邻接拓扑：每对格只判定一次几何相接并双向写入，避免生长过程中重复 GEOS 调用。
    QHash<long long, QVector<long long>> adjacency;
    adjacency.reserve(cellCount);
    for (const GridCell& cell : gridCells) {
        adjacency.insert(cell.gridOid, {});
    }
    for (const GridCell& cell : gridCells) {
        QgsRectangle bbox = cell.geometry.boundingBox();
        bbox.grow(1e-6);
        const QList<QgsFeatureId> candidateIds = spatialIndex.intersects(bbox);
        for (QgsFeatureId fid : candidateIds) {
            const long long neighborOid = featureIdToOid.value(fid);
            if (neighborOid <= 0 || neighborOid <= cell.gridOid) {
                continue;
            }
            if (!geometriesTouch(cell.geometry, cellByOid.value(neighborOid).geometry)) {
                continue;
            }
            adjacency[cell.gridOid].append(neighborOid);
            adjacency[neighborOid].append(cell.gridOid);
        }
    }

    QList<long long> seedOids;
    seedOids.reserve(cellCount);
    for (const GridCell& cell : gridCells) {
        seedOids.append(cell.gridOid);
    }
    std::shuffle(seedOids.begin(), seedOids.end(), *QRandomGenerator::global());

    QSet<long long> assigned;
    QList<RegionGroup> groups;
    const double maxPop = targetPopulation * populationCapFactor;
    QRandomGenerator* random = QRandomGenerator::global();

    using DistanceItem = QPair<double, long long>;
    auto distanceGreater = [](const DistanceItem& lhs, const DistanceItem& rhs) {
        return lhs.first > rhs.first;
    };

    std::deque<long long> seedQueue(seedOids.begin(), seedOids.end());
    while (!seedQueue.empty()) {
        const long long seed = seedQueue.front();
        seedQueue.pop_front();
        if (assigned.contains(seed)) {
            continue;
        }

        RegionGroup group;
        group.seedOid = seed;
        double groupPop = 0.0;
        const QPair<double, double> seedXY = centroidByOid.value(seed);

        // 按候选格质心到种子质心的平方距离×jitter²扩张：形状趋近圆形并打破各向异性。
        std::priority_queue<DistanceItem, std::vector<DistanceItem>, decltype(distanceGreater)> queue(
            distanceGreater);
        QSet<long long> inQueue;
        queue.emplace(0.0, seed);
        inQueue.insert(seed);

        while (!queue.empty()) {
            const long long oid = queue.top().second;
            queue.pop();
            inQueue.remove(oid);
            if (assigned.contains(oid)) {
                continue;
            }

            const GridCell cell = cellByOid.value(oid);
            const double pop = cell.popSum;
            if (!group.gridOids.isEmpty() && groupPop + pop > maxPop) {
                seedQueue.push_front(oid);
                break;
            }

            // 小路网保护：为尚未创建的小区各预留至少一个网格，避免首个种子吞并全部格网。
            const int groupCount = static_cast<int>(groups.size());
            const int reservedForFutureTaz = qMax(0, kMinTazCount - (groupCount + 1));
            const int remainingUnassigned = cellCount - static_cast<int>(assigned.size());
            if (!group.gridOids.isEmpty() && remainingUnassigned <= reservedForFutureTaz) {
                seedQueue.push_front(oid);
                break;
            }

            assigned.insert(oid);
            group.gridOids.append(oid);
            groupPop += pop;

            for (long long neighborOid : adjacency.value(oid)) {
                if (assigned.contains(neighborOid) || inQueue.contains(neighborOid)) {
                    continue;
                }
                const QPair<double, double> neighborXY = centroidByOid.value(neighborOid);
                const double dx = neighborXY.first - seedXY.first;
                const double dy = neighborXY.second - seedXY.second;
                const double jitter = 0.3 + 1.4 * random->generateDouble();
                queue.emplace((dx * dx + dy * dy) * jitter * jitter, neighborOid);
                inQueue.insert(neighborOid);
            }
        }

        if (!group.gridOids.isEmpty()) {
            group.popSum = groupPop;
            groups.append(group);
        }
    }

    for (const GridCell& cell : gridCells) {
        if (assigned.contains(cell.gridOid)) {
            continue;
        }
        RegionGroup group;
        group.seedOid = cell.gridOid;
        group.gridOids.append(cell.gridOid);
        group.popSum = cell.popSum;
        groups.append(group);
    }

    return groups;
}
QList<CaseCommunityGenRecord> mergeGroupsToTaz(const QList<RegionGroup>& groups,
                                               const QHash<long long, GridCell>& cellByOid,
                                               const QgsCoordinateReferenceSystem& projectedCrs,
                                               const QgsCoordinateReferenceSystem& wgs84Crs,
                                               QString* errorOut)
{
    QList<CaseCommunityGenRecord> records;
    records.reserve(groups.size());

    QgsCoordinateTransform toWgs84(projectedCrs, wgs84Crs, QgsProject::instance());
    toWgs84.setBallparkTransformsAreAppropriate(true);

    int areaId = 1;
    for (const RegionGroup& group : groups) {
        if (group.gridOids.isEmpty()) {
            continue;
        }

        QVector<QgsGeometry> groupGeometries;
        groupGeometries.reserve(group.gridOids.size());
        for (long long oid : group.gridOids) {
            const GridCell cell = cellByOid.value(oid);
            if (cell.geometry.isNull() || cell.geometry.isEmpty()) {
                continue;
            }
            groupGeometries.append(cell.geometry);
        }

        if (groupGeometries.isEmpty()) {
            continue;
        }

        QgsGeometry merged = groupGeometries.size() == 1
                                 ? groupGeometries.first()
                                 : QgsGeometry::unaryUnion(groupGeometries);
        if (merged.isNull() || merged.isEmpty()) {
            continue;
        }

        bool ok = false;
        merged = transformGeometry(merged, toWgs84, &ok);
        if (!ok || merged.isNull() || merged.isEmpty()) {
            continue;
        }

        CaseCommunityGenRecord record;
        record.areaId = areaId++;
        record.popSum = group.popSum;
        record.numGrids = group.gridOids.size();
        record.seedOid = group.seedOid;
        record.geometryGeoJsonUtf8 = merged.asJson().toUtf8();
        records.append(record);
    }

    if (records.isEmpty() && errorOut) {
        *errorOut = QString::fromUtf8(u8"合并后无有效交通小区。");
    }
    return records;
}

} // namespace

QString CaseCommunityGenHelper::stepDisplayMessage(CaseCommunityGenStep step)
{
    switch (step) {
    case CaseCommunityGenStep::Validate:
        return QString::fromUtf8(u8"校验参数…");
    case CaseCommunityGenStep::ClipMask:
        return QString::fromUtf8(u8"路网缓冲与填洞…");
    case CaseCommunityGenStep::Fishnet:
        return QString::fromUtf8(u8"生成格网…");
    case CaseCommunityGenStep::PopGrid:
        return QString::fromUtf8(u8"人口分区统计…");
    case CaseCommunityGenStep::GridClip:
        return QString::fromUtf8(u8"裁剪格网…");
    case CaseCommunityGenStep::RegionGrow:
        return QString::fromUtf8(u8"区域生长聚类…");
    case CaseCommunityGenStep::Merge:
        return QString::fromUtf8(u8"合并交通小区…");
    case CaseCommunityGenStep::Submit:
        return QString::fromUtf8(u8"提交服务端…");
    }
    return QString();
}

int CaseCommunityGenHelper::stepStartPercent(CaseCommunityGenStep step)
{
    switch (step) {
    case CaseCommunityGenStep::Validate:
        return 0;
    case CaseCommunityGenStep::ClipMask:
        return 0;
    case CaseCommunityGenStep::Fishnet:
        return 80;
    case CaseCommunityGenStep::PopGrid:
        return 83;
    case CaseCommunityGenStep::GridClip:
        return 87;
    case CaseCommunityGenStep::RegionGrow:
        return 91;
    case CaseCommunityGenStep::Merge:
        return 95;
    case CaseCommunityGenStep::Submit:
        return 98;
    }
    return 0;
}

QString CaseCommunityGenHelper::validateInputs(QgsVectorLayer* roadWayLayer, const QString& rasterPath,
                                               const CaseCommunityGenParams& params)
{
    if (!roadWayLayer || !roadWayLayer->isValid()) {
        return QString::fromUtf8(u8"road_way 图层无效。");
    }
    if (roadWayLayer->geometryType() != Qgis::GeometryType::Line) {
        return QString::fromUtf8(u8"road_way 必须为线图层。");
    }
    if (params.gridCellSizeM <= 0.0) {
        return QString::fromUtf8(u8"格网边长必须大于 0。");
    }
    if (params.bufferHalfWidthM <= 0.0) {
        return QString::fromUtf8(u8"路网缓冲半宽必须大于 0。");
    }
    if (params.targetPopulation <= 0.0) {
        return QString::fromUtf8(u8"目标人口必须大于 0。");
    }
    if (params.populationCapFactor < 1.0) {
        return QString::fromUtf8(u8"人口上限系数必须 ≥ 1。");
    }

    const QFileInfo rasterInfo(rasterPath);
    if (!rasterInfo.exists()) {
        return QString::fromUtf8(u8"未找到人口栅格文件：%1").arg(rasterPath);
    }

    QgsRasterLayer rasterLayer(rasterPath, rasterInfo.completeBaseName(), QStringLiteral("gdal"));
    if (!rasterLayer.isValid()) {
        return QString::fromUtf8(u8"无法打开人口栅格。");
    }
    if (rasterLayer.bandCount() != 1) {
        return QString::fromUtf8(u8"人口栅格必须仅包含 1 个波段。");
    }

    const qint64 featureCount = roadWayLayer->featureCount();
    if (featureCount == 0) {
        return QString::fromUtf8(u8"road_way 图层无要素，请确认 WFS 已加载。");
    }

    return QString();
}

CaseCommunityGenResult CaseCommunityGenHelper::run(QgsVectorLayer* roadWayLayer, const QString& rasterPath,
                                                   const CaseCommunityGenParams& params,
                                                   const CaseCommunityGenProgressCallback& progress)
{
    CaseCommunityGenResult result;

    reportProgress(progress, CaseCommunityGenStep::Validate, stepDisplayMessage(CaseCommunityGenStep::Validate),
                   stepStartPercent(CaseCommunityGenStep::Validate));
    const QString validationError = validateInputs(roadWayLayer, rasterPath, params);
    if (!validationError.isEmpty()) {
        result.errorMessage = validationError;
        result.failedStep = CaseCommunityGenStep::Validate;
        return result;
    }

    const QgsCoordinateReferenceSystem wgs84Crs = QgsCoordinateReferenceSystem::fromEpsgId(4326);
    const QgsRectangle wgsExtent = roadWayLayer->extent();
    const double centerLon = (wgsExtent.xMinimum() + wgsExtent.xMaximum()) * 0.5;
    const QgsCoordinateReferenceSystem projectedCrs = projectedCrsForCenterLon(centerLon);
    if (!projectedCrs.isValid()) {
        result.errorMessage = QString::fromUtf8(u8"无法确定米制投影坐标系。");
        result.failedStep = CaseCommunityGenStep::Validate;
        return result;
    }

    QString stepError;
    const QgsGeometry clipMask =
        buildClipMask(roadWayLayer, params.bufferHalfWidthM, projectedCrs, &stepError, progress);
    if (clipMask.isNull() || clipMask.isEmpty()) {
        result.errorMessage = stepError.isEmpty() ? QString::fromUtf8(u8"生成缓冲掩膜失败。") : stepError;
        result.failedStep = CaseCommunityGenStep::ClipMask;
        return result;
    }
    writeDebugGeometryOnly(clipMask, QStringLiteral("community_gen_clip_mask.shp"), projectedCrs);

    reportProgress(progress, CaseCommunityGenStep::Fishnet, stepDisplayMessage(CaseCommunityGenStep::Fishnet),
                   stepStartPercent(CaseCommunityGenStep::Fishnet));
    QList<GridCell> fishnet = buildFishnet(clipMask, params.gridCellSizeM, &stepError);
    if (fishnet.isEmpty()) {
        result.errorMessage = stepError.isEmpty() ? QString::fromUtf8(u8"格网生成失败。") : stepError;
        result.failedStep = CaseCommunityGenStep::Fishnet;
        return result;
    }

    reportProgress(progress, CaseCommunityGenStep::PopGrid, stepDisplayMessage(CaseCommunityGenStep::PopGrid),
                   stepStartPercent(CaseCommunityGenStep::PopGrid));
    assignPopulationToGrid(fishnet, rasterPath, projectedCrs, &stepError);
    if (!stepError.isEmpty()) {
        result.errorMessage = stepError;
        result.failedStep = CaseCommunityGenStep::PopGrid;
        return result;
    }
    writeDebugGridCells(fishnet, QStringLiteral("community_gen_fishnet.shp"), projectedCrs);

    reportProgress(progress, CaseCommunityGenStep::GridClip, stepDisplayMessage(CaseCommunityGenStep::GridClip),
                   stepStartPercent(CaseCommunityGenStep::GridClip));
    QList<GridCell> clippedGrid = clipGridByMask(fishnet, clipMask, &stepError);
    if (clippedGrid.isEmpty()) {
        result.errorMessage = stepError.isEmpty() ? QString::fromUtf8(u8"格网裁剪失败。") : stepError;
        result.failedStep = CaseCommunityGenStep::GridClip;
        return result;
    }
    writeDebugGridCells(clippedGrid, QStringLiteral("community_gen_grid_clip.shp"), projectedCrs);
    result.gridCount = clippedGrid.size();

    reportProgress(progress, CaseCommunityGenStep::RegionGrow, stepDisplayMessage(CaseCommunityGenStep::RegionGrow),
                   stepStartPercent(CaseCommunityGenStep::RegionGrow));
    const QList<RegionGroup> groups =
        regionGrowClusters(clippedGrid, params.targetPopulation, params.populationCapFactor);

    reportProgress(progress, CaseCommunityGenStep::Merge, stepDisplayMessage(CaseCommunityGenStep::Merge),
                   stepStartPercent(CaseCommunityGenStep::Merge));
    QHash<long long, GridCell> cellByOid;
    cellByOid.reserve(clippedGrid.size());
    for (const GridCell& cell : clippedGrid) {
        cellByOid.insert(cell.gridOid, cell);
    }

    result.records = mergeGroupsToTaz(groups, cellByOid, projectedCrs, wgs84Crs, &stepError);
    if (result.records.isEmpty()) {
        result.errorMessage = stepError.isEmpty() ? QString::fromUtf8(u8"合并交通小区失败。") : stepError;
        result.failedStep = CaseCommunityGenStep::Merge;
        return result;
    }

    result.tazCount = result.records.size();
    for (const CaseCommunityGenRecord& record : result.records) {
        result.totalPopulation += record.popSum;
    }

    writeDebugTazRecords(result.records, QStringLiteral("community_gen_taz.shp"), projectedCrs, wgs84Crs);

    return result;
}

QVariantList CaseCommunityGenHelper::buildGenCommunityRequestArray(const QList<CaseCommunityGenRecord>& records)
{
    QVariantList dataList;
    dataList.reserve(records.size());
    for (const CaseCommunityGenRecord& record : records) {
        QVariantMap item;
        item.insert(QStringLiteral("area_id"), record.areaId);
        item.insert(QStringLiteral("pop_sum"), record.popSum);
        item.insert(QStringLiteral("num_grids"), record.numGrids);
        item.insert(QStringLiteral("seed_oid"), record.seedOid);

        const QJsonDocument doc = QJsonDocument::fromJson(record.geometryGeoJsonUtf8);
        if (doc.isObject()) {
            item.insert(QStringLiteral("geometry"), doc.object().toVariantMap());
        }
        dataList.append(item);
    }
    return dataList;
}
