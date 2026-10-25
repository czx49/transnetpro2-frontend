/*****************************************************************//**
 * \file   casemaplayerresolve.cpp
 * \brief  方案图层树 → 地图矢量图层
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casemaplayerresolve.h"

#include "basedatatableview.h"
#include "casetreemodel.h"
#include "casetreeview.h"
#include "casewfsmapcontroller.h"

#include "../tnmapwidget/tnmapwidget.h"

#include <qgsdatasourceuri.h>
#include <qgsvectorlayer.h>

namespace {

QString fail(QString* errorMessage, const QString& msg)
{
    if (errorMessage) {
        *errorMessage = msg;
    }
    return {};
}

QModelIndex findCaseIndex(CaseTreeModel* model, int caseId)
{
    if (!model || caseId <= 0) {
        return QModelIndex();
    }
    const int rows = model->rowCount(QModelIndex());
    for (int r = 0; r < rows; ++r) {
        const QModelIndex idx = model->index(r, 0);
        if (idx.isValid() && idx.data(Qt::UserRole).toInt() == caseId) {
            return idx;
        }
    }
    return QModelIndex();
}

QString tableFullNameFromLayerIndex(const QModelIndex& layerIdx)
{
    if (!layerIdx.isValid()) {
        return {};
    }
    return layerIdx.data(Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
}

QString findCheckedSchemeTableFullName(CaseTreeModel* model, const QModelIndex& caseIdx, const QString& wfsTypeName)
{
    if (!model || !caseIdx.isValid() || wfsTypeName.isEmpty()) {
        return {};
    }

    const QString target = wfsTypeName.trimmed();
    const int topRows = model->rowCount(caseIdx);
    for (int i = 0; i < topRows; ++i) {
        const QModelIndex topIdx = model->index(i, 0, caseIdx);
        if (!topIdx.isValid()) {
            continue;
        }

        const QVariantMap topMeta = topIdx.data(Qt::UserRole).toMap();
        const bool isGroup = topMeta.value(QStringLiteral("nodeKind")).toString() == QStringLiteral("group");

        if (isGroup) {
            const int layerRows = model->rowCount(topIdx);
            for (int j = 0; j < layerRows; ++j) {
                const QModelIndex layerIdx = model->index(j, 0, topIdx);
                if (!layerIdx.isValid()) {
                    continue;
                }
                if (tableFullNameFromLayerIndex(layerIdx).compare(target, Qt::CaseInsensitive) != 0) {
                    continue;
                }
                if (layerIdx.data(Qt::CheckStateRole).toInt() != static_cast<int>(Qt::Checked)) {
                    continue;
                }
                return tableFullNameFromLayerIndex(layerIdx);
            }
        } else {
            if (tableFullNameFromLayerIndex(topIdx).compare(target, Qt::CaseInsensitive) != 0) {
                continue;
            }
            if (topIdx.data(Qt::CheckStateRole).toInt() != static_cast<int>(Qt::Checked)) {
                continue;
            }
            return tableFullNameFromLayerIndex(topIdx);
        }
    }
    return {};
}

} // namespace

namespace CaseMapLayerResolve {

QString vectorLayerWfsTypeName(QgsVectorLayer* layer)
{
    if (!layer || !layer->dataProvider()) {
        return {};
    }
    QgsDataSourceUri uri(layer->dataProvider()->dataSourceUri());
    const QString table = uri.table();
    if (!table.isEmpty()) {
        return table;
    }
    return uri.param(QStringLiteral("typename"));
}

QgsVectorLayer* resolveVectorLayer(CaseTreeView* tree, TNMapWidget* map, int expectedCaseId, QString* errorMessage)
{
    auto failLayer = [&](const QString& msg) -> QgsVectorLayer* {
        if (errorMessage) {
            *errorMessage = msg;
        }
        return nullptr;
    };

    if (!tree || !map) {
        return failLayer(QString::fromUtf8(u8"地图或图层树未就绪。"));
    }

    const QModelIndex layerIdx = tree->currentIndex();
    if (!tree->isSchemeLayerIndex(layerIdx)) {
        return failLayer(QString::fromUtf8(u8"请在图层树中选择一个图层。"));
    }

    const QModelIndex caseIdx = tree->schemeCaseIndexForLayer(layerIdx);
    if (!caseIdx.isValid()) {
        return failLayer(QString::fromUtf8(u8"无法识别图层所属方案。"));
    }

    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    if (expectedCaseId > 0 && caseId != expectedCaseId) {
        return failLayer(QString::fromUtf8(u8"请选择当前方案下的图层。"));
    }

    const Qt::CheckState checkState = static_cast<Qt::CheckState>(layerIdx.data(Qt::CheckStateRole).toInt());
    if (checkState != Qt::Checked) {
        return failLayer(QString::fromUtf8(u8"请先在图层树中勾选该图层以加载到地图。"));
    }

    const QString typeName = layerIdx.data(Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        return failLayer(QString::fromUtf8(u8"图层缺少类型名，无法定位。"));
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(typeName);
    if (uri.isEmpty()) {
        return failLayer(QString::fromUtf8(u8"无法构建图层数据源。"));
    }

    QgsMapLayer* mapLayer = map->getLayer(uri);
    auto* vectorLayer = qobject_cast<QgsVectorLayer*>(mapLayer);
    if (!vectorLayer || !vectorLayer->isValid()) {
        return failLayer(QString::fromUtf8(u8"图层尚未加载到地图，请先勾选可见。"));
    }

    map->selectLayerByUri(uri);
    return vectorLayer;
}

QString resolveTableFullName(CaseTreeView* tree, TNMapWidget* map, int expectedCaseId, QgsVectorLayer* layer,
                             QString* errorMessage)
{
    if (!tree || !map || !layer) {
        return fail(errorMessage, QString::fromUtf8(u8"地图或图层未就绪。"));
    }

    const QString wfsTypeName = vectorLayerWfsTypeName(layer).trimmed();
    if (wfsTypeName.isEmpty()) {
        return fail(errorMessage, QString::fromUtf8(u8"无法解析图层表名。"));
    }

    CaseTreeModel* model = tree->caseModel();
    if (!model) {
        return fail(errorMessage, QString::fromUtf8(u8"图层树未就绪。"));
    }

    const QModelIndex caseIdx = findCaseIndex(model, expectedCaseId);
    if (!caseIdx.isValid()) {
        return fail(errorMessage, QString::fromUtf8(u8"无法定位当前方案。"));
    }

    const QString tableFullName = findCheckedSchemeTableFullName(model, caseIdx, wfsTypeName);
    if (tableFullName.isEmpty()) {
        return fail(errorMessage, QString::fromUtf8(u8"请先在图层树中勾选该图层以加载到地图。"));
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(tableFullName);
    if (uri.isEmpty() || !map->getLayer(uri)) {
        return fail(errorMessage, QString::fromUtf8(u8"图层尚未加载到地图，请先勾选可见。"));
    }

    return tableFullName;
}

QString resolveTableFullNameFromBaseData(const QString& wfsTypeName, const QVariantMap& nodeUserData,
                                         QString* errorMessage)
{
    const QString target = wfsTypeName.trimmed();
    if (target.isEmpty()) {
        return fail(errorMessage, QString::fromUtf8(u8"无法解析图层表名。"));
    }
    if (nodeUserData.isEmpty()) {
        return fail(errorMessage, QString::fromUtf8(u8"请先在图层树中勾选该图层以加载到地图。"));
    }

    const QString value =
        nodeUserData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
    if (value.isEmpty() || value.compare(target, Qt::CaseInsensitive) != 0) {
        return fail(errorMessage, QString::fromUtf8(u8"请先在图层树中勾选该图层以加载到地图。"));
    }

    return value;
}

} // namespace CaseMapLayerResolve
