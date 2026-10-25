/*****************************************************************//**
 * \file   casemaplayerresolve.h
 * \brief  从方案图层树解析已加载的地图矢量图层
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEMAPLAYERRESOLVE_H
#define CASEMAPLAYERRESOLVE_H

#include <QString>
#include <QVariantMap>

class CaseTreeView;
class TNMapWidget;
class QgsVectorLayer;

namespace CaseMapLayerResolve {

QgsVectorLayer* resolveVectorLayer(CaseTreeView* tree, TNMapWidget* map, int expectedCaseId, QString* errorMessage);

/** @brief 从方案树已勾选节点取 API value 作为 tablefullname */
QString resolveTableFullName(CaseTreeView* tree, TNMapWidget* map, int expectedCaseId, QgsVectorLayer* layer,
                             QString* errorMessage);

/** @brief 从基础数据树已勾选节点取 API value 作为 tablefullname */
QString resolveTableFullNameFromBaseData(const QString& wfsTypeName, const QVariantMap& nodeUserData,
                                         QString* errorMessage);

QString vectorLayerWfsTypeName(QgsVectorLayer* layer);

} // namespace CaseMapLayerResolve

#endif // CASEMAPLAYERRESOLVE_H
