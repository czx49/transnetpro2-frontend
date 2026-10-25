/*****************************************************************//**
 * \file   caseauxlayerhelper.h
 * \brief  期望线/道路溯源辅助图层 typename 推导
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEAUXLAYERHELPER_H
#define CASEAUXLAYERHELPER_H

#include <QList>
#include <QString>

namespace CaseAuxLayer {

struct AuxLayerTarget {
    QString typeName;
    QString displayName;
    int level = 0;
};

QList<AuxLayerTarget> targetsForDesireline(const QString& communityTableFullName,
                                           const QString& apiCreatedTable = QString());
QList<AuxLayerTarget> targetsForPathTrace(const QString& roadTableFullName);

} // namespace CaseAuxLayer

#endif // CASEAUXLAYERHELPER_H
