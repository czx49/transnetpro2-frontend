/*****************************************************************//**
 * \file   casedrcsvhelper.cpp
 * \brief  `/api/files/drcsv` 与 `/api/project/export` 的 table 参数
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casedrcsvhelper.h"

#include <QStringList>

namespace CaseDrcsvHelper {

namespace {

const QStringList& knownImportTables()
{
    static const QStringList tables = {
        QStringLiteral("other_trip_distribution_hist"),
        QStringLiteral("other_trip_production_attraction"),
        QStringLiteral("other_taz_socioeconomic"),
        QStringLiteral("other_bus_card_records"),
        QStringLiteral("other_mobile_cdr"),
        QStringLiteral("other_observation"),
        QStringLiteral("other_population"),
        QStringLiteral("other_taxi_trip"),
        QStringLiteral("other_toll_gate"),
        QStringLiteral("other_od"),
        QStringLiteral("road_community"),
        QStringLiteral("bus_community"),
        QStringLiteral("road_way"),
        QStringLiteral("bus_way"),
        QStringLiteral("slow_road_way"),
        QStringLiteral("slow_road_point"),
        QStringLiteral("slow_road_community"),
        QStringLiteral("road_point"),
        QStringLiteral("bus_point"),
    };
    return tables;
}

} // namespace

QString tableNameFromLayerVal(const QString& valRaw)
{
    const QString val = valRaw.trimmed().toLower();
    if (val.isEmpty()) {// || !knownImportTables().contains(val)) {
        return {};
    }
    return val;
}

} // namespace CaseDrcsvHelper
