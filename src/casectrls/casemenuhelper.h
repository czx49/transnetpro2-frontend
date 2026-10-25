#ifndef CASEMENUHELPER_H
#define CASEMENUHELPER_H

#include "basedatatableview.h"
#include "caseitem.h"

#include "../jsonreader/httpapijson.h"

#include <QJsonArray>
#include <QList>

QString layerCategoryFromApiVal(const QString& valRaw, const QString& typeNameFallback);

LayerItem layerItemFromMenuChild(const MenuChildrenJson& child);

QList<LayerGroupItem> parseLayerGroupsFromJsonArray(const QJsonArray& listArray);

QList<LayerGroupItem> parseLayerGroupsFromCaseJson(const CaseJson& raw);

QList<LayerGroupItem> parseLayerGroups(const QList<MenuJson>& menus);

QList<LayerItem> flattenLayerGroups(const QList<LayerGroupItem>& groups);

QList<BaseDataLayerGroupNodeItem> buildBaseDataLayerGroups(const QList<MenuJson>& menus);

#endif // CASEMENUHELPER_H
