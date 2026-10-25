/*****************************************************************//**
 * \file   casecommunitygenhelper.h
 * \brief  基础数据「生成交通小区」本地格网聚类与提交辅助
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASECOMMUNITYGENHELPER_H
#define CASECOMMUNITYGENHELPER_H

#include <QList>
#include <QString>
#include <QVariant>
#include <functional>

class QgsVectorLayer;

enum class CaseCommunityGenStep {
    Validate = 0,
    ClipMask,
    Fishnet,
    PopGrid,
    GridClip,
    RegionGrow,
    Merge,
    Submit
};

struct CaseCommunityGenParams {
    double gridCellSizeM = 500.0;
    double bufferHalfWidthM = 50.0;
    double targetPopulation = 50000.0;
    double populationCapFactor = 1.3;
    bool dryRun = false;
};

struct CaseCommunityGenRecord {
    int areaId = 0;
    double popSum = 0.0;
    int numGrids = 0;
    long long seedOid = 0;
    QByteArray geometryGeoJsonUtf8;
};

struct CaseCommunityGenResult {
    QList<CaseCommunityGenRecord> records;
    int gridCount = 0;
    int tazCount = 0;
    double totalPopulation = 0.0;
    QString errorMessage;
    CaseCommunityGenStep failedStep = CaseCommunityGenStep::Validate;
};

using CaseCommunityGenProgressCallback =
    std::function<void(CaseCommunityGenStep step, const QString& message, int percent)>;

/**
 * \brief  从 road_way 生成交通小区（格网 + 区域生长）并组装 gen_community 请求
 * \author zhangmg
 * \date   June 2026
 */
class CaseCommunityGenHelper {
public:
    static QString validateInputs(QgsVectorLayer* roadWayLayer, const QString& rasterPath,
                                  const CaseCommunityGenParams& params);

    static CaseCommunityGenResult run(QgsVectorLayer* roadWayLayer, const QString& rasterPath,
                                      const CaseCommunityGenParams& params,
                                      const CaseCommunityGenProgressCallback& progress = {});

    static QVariantList buildGenCommunityRequestArray(const QList<CaseCommunityGenRecord>& records);

    static QString stepDisplayMessage(CaseCommunityGenStep step);

    /** @brief 大步骤进度起点（0–100）；ClipMask 内部另有细化百分比 */
    static int stepStartPercent(CaseCommunityGenStep step);
};

#endif // CASECOMMUNITYGENHELPER_H
