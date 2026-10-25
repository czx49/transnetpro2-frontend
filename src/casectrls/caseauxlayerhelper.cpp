/*****************************************************************//**
 * \file   caseauxlayerhelper.cpp
 * \brief  期望线/道路溯源辅助图层 typename 推导
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "caseauxlayerhelper.h"
#include "casewaylayerhelper.h"

namespace {

const QString kCenteroidLinesSuffix = QStringLiteral("_centeroid_lines");
const QString kCenteroidToLineSuffix = QStringLiteral("_centeroid_to_line");

QString trimmedTableName(const QString& tableFullName)
{
    return tableFullName.trimmed();
}

QString deriveCenteroidLinesTypeName(const QString& communityTableFullName)
{
    const QString community = trimmedTableName(communityTableFullName);
    if (community.isEmpty()) {
        return {};
    }
    return community + kCenteroidLinesSuffix;
}

QString deriveCenteroidToLineTypeName(const QString& communityTableFullName)
{
    const QString community = trimmedTableName(communityTableFullName);
    if (community.isEmpty()) {
        return {};
    }
    return community + kCenteroidToLineSuffix;
}

CaseAuxLayer::AuxLayerTarget makeTarget(const QString& typeName, const QString& displayName, int level)
{
    CaseAuxLayer::AuxLayerTarget target;
    target.typeName = typeName;
    target.displayName = displayName;
    target.level = level;
    return target;
}

} // namespace

namespace CaseAuxLayer {

QList<AuxLayerTarget> targetsForDesireline(const QString& communityTableFullName, const QString& apiCreatedTable)
{
    QList<AuxLayerTarget> targets;
    const QString community = trimmedTableName(communityTableFullName);
    if (community.isEmpty()) {
        return targets;
    }

    QString linesType = trimmedTableName(apiCreatedTable);
    if (linesType.isEmpty()) {
        linesType = deriveCenteroidLinesTypeName(community);
    }

    const QString toLineType = deriveCenteroidToLineTypeName(community);
    if (!linesType.isEmpty()) {
        targets.append(makeTarget(linesType, QString::fromUtf8(u8"道路图层_期望线"), 2));
    }
    if (!toLineType.isEmpty()) {
        targets.append(makeTarget(toLineType, QString::fromUtf8(u8"道路图层_质心节点"), 2));
    }
    return targets;
}

QList<AuxLayerTarget> targetsForPathTrace(const QString& roadTableFullName)
{
    QList<AuxLayerTarget> targets;
    const QString traceType = CaseWayLayer::deriveTraceTmpTypeName(roadTableFullName);
    if (traceType.isEmpty()) {
        return targets;
    }

    targets.append(makeTarget(traceType, QString::fromUtf8(u8"道路溯源"), 2));
    return targets;
}

} // namespace CaseAuxLayer
