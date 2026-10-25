/*****************************************************************//**
 * \file   casemaptoolcontroller.cpp
 * \brief  地图工具条控制器实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "casemaptoolcontroller.h"

#include "caseclipdialog.h"
#include "caseauxlayerhelper.h"
#include "casemapcanvasexporter.h"
#include "casemapexportdialog.h"
#include "casemapexportworker.h"
#include "casemaplayerresolve.h"
#include "casesymbolstyledialog.h"
#include "casewaylayerhelper.h"
#include "casewfsmapcontroller.h"

#include "../httpapis/httpapis.h"
#include "../tnmapwidget/tnlayerattributefilterdialog.h"
#include "../jsonreader/httpapijson.h"
#include "../publicctrls/framelessdialog.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "../tnmapwidget/layerstylefactory.h"
#include "../tnmapwidget/tnmapwidget.h"
#include "../tnmapwidget/tngiseditstackmanager.h"

#include "../uis/loadingdialog.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

#include <qgis.h>
#include <qgsdatasourceuri.h>
#include <qgsfeature.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmapsettings.h>
#include <qgsvectorlayer.h>

namespace {

const QString kPanelWfsProp = QStringLiteral("tnp_panel_wfs");

void showMapToolMessage(const QString& title, const QString& message, bool warning)
{
    FramelessDialog dlg(nullptr);
    dlg.setTitleText(title);
    dlg.setMinimizeButtonVisible(false);
    dlg.setMaximizeButtonVisible(false);
    dlg.resize(420, 160);
    dlg.setStyleSheet(Phase2Ui::dialogShellStyleSheet());

    QVBoxLayout* root = dlg.contentLayout();
    root->setContentsMargins(16, 12, 16, 16);
    QLabel* lab = new QLabel(message, &dlg);
    lab->setWordWrap(true);
    lab->setStyleSheet(warning ? QStringLiteral("QLabel { color: #b45309; font-size: 13px; }")
                               : QStringLiteral("QLabel { color: #ffffff; font-size: 13px; }"));
    root->addWidget(lab);
    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    QPushButton* ok = new QPushButton(QString::fromUtf8(u8"确定"), &dlg);
    ok->setCursor(Qt::PointingHandCursor);
    ok->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    QObject::connect(ok, &QPushButton::clicked, &dlg, &FramelessDialog::accept);
    row->addWidget(ok);
    root->addLayout(row);
    dlg.exec();
}

QString vectorLayerTableKey(QgsVectorLayer* layer)
{
    return CaseMapLayerResolve::vectorLayerWfsTypeName(layer);
}

QString resolveLayerTableFullName(const CaseMapToolContext& ctx, QgsVectorLayer* layer, QString* errorMessage)
{
    if (!ctx.resolveTableFullName) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"图层树未就绪。");
        }
        return {};
    }
    return ctx.resolveTableFullName(layer, errorMessage);
}

void removeTraceTmpLayers(TNMapWidget* map)
{
    if (!map || !map->canvas()) {
        return;
    }
    map->canvas()->stopRendering();
    const QList<QgsMapLayer*> layers = map->canvas()->layers();
    for (QgsMapLayer* layer : layers) {
        auto* vl = qobject_cast<QgsVectorLayer*>(layer);
        if (!vl || !vl->dataProvider()) {
            continue;
        }
        if (!vl->customProperty(kPanelWfsProp).toBool()) {
            continue;
        }
        const QString key = vectorLayerTableKey(vl);
        if (key.contains(QStringLiteral("trace_tmp"))) {
            map->removeLayer(vl->dataProvider()->dataSourceUri());
        }
    }
}

void schedulePathTraceStyle(TNMapWidget* map, const QString& roadTableFullName,
                            const std::function<void(QgsVectorLayer*, const QString&)>& syncLegend)
{
    if (!map || roadTableFullName.trimmed().isEmpty()) {
        return;
    }

    const QString traceTypeName = CaseWayLayer::deriveTraceTmpTypeName(roadTableFullName);
    if (traceTypeName.isEmpty()) {
        return;
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(traceTypeName);
    auto applyStyle = [map, uri, syncLegend]() -> bool {
        if (!map) {
            return false;
        }
        auto* vectorLayer = qobject_cast<QgsVectorLayer*>(map->getLayer(uri));
        if (!vectorLayer || !vectorLayer->isValid()) {
            return false;
        }
        LayerStyleFactory::applyPathTraceStyle(vectorLayer);
        if (syncLegend) {
            syncLegend(vectorLayer, QStringLiteral("path_flow"));
        }
        if (map->canvas()) {
            map->canvas()->refresh();
        }
        return true;
    };

    if (applyStyle()) {
        return;
    }
    QTimer::singleShot(150, map, [applyStyle]() { applyStyle(); });
    QTimer::singleShot(400, map, [applyStyle]() { applyStyle(); });
    QTimer::singleShot(900, map, [applyStyle]() { applyStyle(); });
}

} // namespace

CaseMapToolController::CaseMapToolController(QObject* parent)
    : QObject(parent)
    , m_flowConnActive(false)
    , m_aiWayConnActive(false)
{
}

CaseMapToolController::~CaseMapToolController()
{
    disconnectPendingMapSelections(nullptr);
    if (m_gisAsyncBusy) {
        endGisAsyncOp();
    }
}

bool CaseMapToolController::beginGisAsyncOp(const CaseMapToolContext& ctx)
{
    if (m_gisAsyncBusy) {
        return false;
    }
    m_gisAsyncBusy = true;
    m_gisAsyncBusyNotifier = ctx.onGisAsyncBusyChanged;
    if (m_gisAsyncBusyNotifier) {
        m_gisAsyncBusyNotifier(true);
    }
    return true;
}

void CaseMapToolController::endGisAsyncOp()
{
    if (!m_gisAsyncBusy) {
        return;
    }
    m_gisAsyncBusy = false;
    if (m_gisAsyncBusyNotifier) {
        m_gisAsyncBusyNotifier(false);
    }
    m_gisAsyncBusyNotifier = nullptr;
}

void CaseMapToolController::disconnectAiWaySelection(TNMapWidget* map)
{
    if (m_aiWayConnActive) {
        QObject::disconnect(m_aiWaySelectionConn);
        m_aiWayConnActive = false;
    }
    if (map) {
        map->activateSpatialSelectTool(TNMapWidget::SpatialToolType::RectSelect, false);
    }
}

void CaseMapToolController::cancelAiWaySelect(TNMapWidget* map)
{
    disconnectAiWaySelection(map);
}

void CaseMapToolController::disconnectPendingMapSelections(TNMapWidget* map)
{
    disconnectFlowSelection(map);
    disconnectAiWaySelection(map);
    disconnectCompareExtentPick(map);
}

void CaseMapToolController::disconnectCompareExtentPick(TNMapWidget* map)
{
    if (m_compareExtentConnActive) {
        QObject::disconnect(m_compareExtentSelectedConn);
        QObject::disconnect(m_compareExtentCancelledConn);
        QObject::disconnect(m_compareExtentTooSmallConn);
        m_compareExtentConnActive = false;
    }
    if (map) {
        map->activateCompareExtentTool(false);
    }
}

void CaseMapToolController::cancelCompareExtentPick(TNMapWidget* map)
{
    disconnectCompareExtentPick(map);
}

void CaseMapToolController::disconnectFlowSelection(TNMapWidget* map)
{
    if (m_flowConnActive) {
        QObject::disconnect(m_flowSelectionConn);
        m_flowConnActive = false;
    }
    if (map) {
        map->activateSpatialSelectTool(TNMapWidget::SpatialToolType::PointSelect, false);
    }
}

void CaseMapToolController::runTool(TNMapWidget* map, CaseMapToolbarWidget::ToolType tool, const CaseMapToolContext& ctx)
{
    if (!map) {
        return;
    }

    switch (tool) {
    case CaseMapToolbarWidget::ToolType::Browse:
    case CaseMapToolbarWidget::ToolType::Select:
    case CaseMapToolbarWidget::ToolType::Edit:
    case CaseMapToolbarWidget::ToolType::AddFeature:
    case CaseMapToolbarWidget::ToolType::Delete:
        disconnectPendingMapSelections(map);
        if (tool == CaseMapToolbarWidget::ToolType::AddFeature) {
            map->setAddFeatureTargetLayer(map->currentLayer());
            if (ctx.prepareAddFeature) {
                ctx.prepareAddFeature(map);
            }
            if (ctx.prefillFeatureAttributes) {
                map->setFeatureAttributePrefillHandler(ctx.prefillFeatureAttributes);
            } else {
                map->clearFeatureAttributePrefillHandler();
            }
            map->restoreAddFeatureTargetLayer();
        } else {
            map->clearFeatureAttributePrefillHandler();
            map->setAddFeatureTargetLayer(nullptr);
        }
        applyMutexTool(map, tool);
        break;
    case CaseMapToolbarWidget::ToolType::UndoEdits:
        disconnectPendingMapSelections(map);
        runUndoEdits(map);
        break;
    case CaseMapToolbarWidget::ToolType::SaveEdits:
        disconnectPendingMapSelections(map);
        runSaveEdits(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::Clip:
        disconnectPendingMapSelections(map);
        runClip(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::Symbol:
        disconnectPendingMapSelections(map);
        runSymbol(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::ClearSymbol:
        disconnectPendingMapSelections(map);
        runClearSymbol(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::Desireline:
        disconnectPendingMapSelections(map);
        runDesireline(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::Flow:
        disconnectPendingMapSelections(map);
        runFlowTrace(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::AttributeFilter:
        disconnectPendingMapSelections(map);
        runAttributeFilter(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::ExportMap:
        disconnectPendingMapSelections(map);
        runExportMap(map, ctx);
        break;
    case CaseMapToolbarWidget::ToolType::RefreshData:
        break;
    default:
        break;
    }
}

void CaseMapToolController::applyMutexTool(TNMapWidget* map, CaseMapToolbarWidget::ToolType tool)
{
    switch (tool) {
    case CaseMapToolbarWidget::ToolType::Browse:
        map->deactivateEditTools();
        map->setPanable();
        break;
    case CaseMapToolbarWidget::ToolType::Select:
        map->deactivateEditTools();
        // 单一要素选择工具：单击点选、拖拽框选
        map->setSelectMode(TNMapWidget::PointSelect);
        break;
    case CaseMapToolbarWidget::ToolType::Edit:
        map->enableDeleteFeature(false);
        map->enableAddFeature(false);
        map->enableEditFeature(true);
        break;
    case CaseMapToolbarWidget::ToolType::AddFeature:
        map->enableDeleteFeature(false);
        map->enableEditFeature(false);
        map->enableAddFeature(true);
        break;
    case CaseMapToolbarWidget::ToolType::Delete:
        map->enableAddFeature(false);
        map->enableDeleteFeature(true);
        break;
    default:
        break;
    }
}

void CaseMapToolController::runClip(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (ctx.projectId <= 0 || ctx.authToken.isEmpty()) {
        showMapToolMessage(QString::fromUtf8(u8"裁剪"), QString::fromUtf8(u8"项目或登录状态无效。"), true);
        return;
    }
    CaseClipLayerDialog dlg(map, ctx.projectId, ctx.caseId, ctx.authToken, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        map->canvas()->refresh();
    }
}

void CaseMapToolController::runSymbol(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (!map) {
        return;
    }
    map->setStyleContext(ctx.projectId, ctx.caseId);
    CaseSymbolStyleDialog dialog(map, ctx.projectId, ctx.caseId, nullptr);
    dialog.exec();
}

void CaseMapToolController::runClearSymbol(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (!map) {
        return;
    }

    CaseWfsMap::resetPanelWfsDefaultSymbols(map);
    if (ctx.isBaseCase && map->canvas()) {
        const QList<QgsMapLayer*> layers = map->canvas()->layers();
        for (QgsMapLayer* layer : layers) {
            auto* vl = qobject_cast<QgsVectorLayer*>(layer);
            if (!vl || !vl->isValid()) {
                continue;
            }
            const QString typeName = vectorLayerTableKey(vl);
            const QString layerVal = LayerStyleFactory::resolveLayerVal(vl, QString(), typeName);
            if (!CaseWayLayer::shouldApplyDefaultTypeFilter(true, layerVal, typeName)) {
                continue;
            }
            CaseWayLayer::applyDefaultTypeFilter(vl);
            CaseWayLayer::scheduleDefaultTypeFilter(vl);
        }
    }
    if (ctx.uncheckTraceTmpLayers) {
        ctx.uncheckTraceTmpLayers();
    }
    if (map->canvas()) {
        map->canvas()->refresh();
    }
}

void CaseMapToolController::runDesireline(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (ctx.projectId <= 0 || ctx.authToken.isEmpty()) {
        showMapToolMessage(QString::fromUtf8(u8"生成期望线"), QString::fromUtf8(u8"项目或登录状态无效。"), true);
        return;
    }
    if (!ctx.isBaseDataMap && ctx.caseId <= 0) {
        showMapToolMessage(QString::fromUtf8(u8"生成期望线"), QString::fromUtf8(u8"请先打开方案。"), true);
        return;
    }

    QgsVectorLayer* layer = qobject_cast<QgsVectorLayer*>(map->selectedLayer());
    if (!layer) {
        layer = qobject_cast<QgsVectorLayer*>(map->currentLayer());
    }
    if (!layer || layer->geometryType() != Qgis::GeometryType::Polygon) {
        showMapToolMessage(QString::fromUtf8(u8"生成期望线"), QString::fromUtf8(u8"请选择面图层。"), true);
        return;
    }

    QString resolveError;
    const QString tableFullName = resolveLayerTableFullName(ctx, layer, &resolveError);
    if (tableFullName.isEmpty()) {
        showMapToolMessage(QString::fromUtf8(u8"生成期望线"), resolveError, true);
        return;
    }

    QPointer<TNMapWidget> mapGuard(map);
    const bool isBaseDataMap = ctx.isBaseDataMap;
    const int caseId = ctx.caseId;
    auto onSchemeChanged = ctx.onSchemeDataChanged;
    auto onBaseChanged = ctx.onBaseDataChanged;
    auto loadSchemeAux = ctx.loadSchemeAuxiliaryLayers;
    auto loadBaseAux = ctx.loadBaseAuxiliaryLayers;
    auto onDesirelineSucceeded = ctx.onDesirelineSucceeded;

    if (!beginGisAsyncOp(ctx)) {
        return;
    }

    HttpApis::createExpectLine(ctx.authToken, ctx.projectId, ctx.caseId, tableFullName,
        [this, mapGuard, tableFullName, isBaseDataMap, caseId, onSchemeChanged, onBaseChanged, loadSchemeAux, loadBaseAux,
         onDesirelineSucceeded](bool success, const QSharedPointer<PropertyJson>& response, const QString& errorMsg) {
            if (success) {
                QString createdTable;
                if (!response.isNull()) {
                    createdTable = response->value<QString>().trimmed();
                    if (!createdTable.isEmpty()) {
                        qInfo() << QString::fromUtf8(u8"生成期望线结果表：%1").arg(createdTable);
                    }
                }
                const QList<CaseAuxLayer::AuxLayerTarget> auxTargets =
                    CaseAuxLayer::targetsForDesireline(tableFullName, createdTable);

                CaseWfsMap::refreshWfsCapabilitiesCache();

                auto loadAuxiliaryLayers = [mapGuard, isBaseDataMap, caseId, loadSchemeAux, loadBaseAux, auxTargets]() {
                    if (isBaseDataMap) {
                        if (loadBaseAux && mapGuard) {
                            loadBaseAux(mapGuard, auxTargets);
                        }
                    } else if (loadSchemeAux && mapGuard) {
                        loadSchemeAux(mapGuard, caseId, auxTargets);
                    }
                };

                const auto finishDesirelineUi = [this, loadAuxiliaryLayers, onDesirelineSucceeded]() {
                    loadAuxiliaryLayers();
                    if (onDesirelineSucceeded) {
                        onDesirelineSucceeded([this](bool postOk, const QString& postErr) {
                            if (postOk) {
                                showMapToolMessage(QString::fromUtf8(u8"生成期望线"),
                                    QString::fromUtf8(u8"生成期望线成功，地图已加载并应用 OD 渲染样式。"),
                                    false);
                                return;
                            }
                            const QString msg = postErr.trimmed().isEmpty()
                                ? QString::fromUtf8(u8"生成期望线成功，但 OD 图层加载或渲染样式应用失败。")
                                : QString::fromUtf8(u8"生成期望线成功，但 %1").arg(postErr.trimmed());
                            showMapToolMessage(QString::fromUtf8(u8"生成期望线"), msg, true);
                        });
                        return;
                    }
                    showMapToolMessage(QString::fromUtf8(u8"生成期望线"),
                        QString::fromUtf8(u8"生成期望线成功。"), false);
                };

                if (isBaseDataMap) {
                    if (onBaseChanged) {
                        onBaseChanged(finishDesirelineUi);
                    } else {
                        finishDesirelineUi();
                    }
                } else if (onSchemeChanged) {
                    onSchemeChanged(finishDesirelineUi);
                } else {
                    finishDesirelineUi();
                }
            } else {
                const QString msg =
                    errorMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"接口失败") : errorMsg.trimmed();
                showMapToolMessage(QString::fromUtf8(u8"生成期望线"), msg, true);
            }
            endGisAsyncOp();
        });
}

void CaseMapToolController::runFlowTrace(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (ctx.projectId <= 0 || ctx.authToken.isEmpty()) {
        showMapToolMessage(QString::fromUtf8(u8"道路溯源"), QString::fromUtf8(u8"项目或登录状态无效。"), true);
        return;
    }
    if (!ctx.isBaseDataMap && ctx.caseId <= 0) {
        showMapToolMessage(QString::fromUtf8(u8"道路溯源"), QString::fromUtf8(u8"请先打开方案。"), true);
        return;
    }

    QgsVectorLayer* layer = qobject_cast<QgsVectorLayer*>(map->selectedLayer());
    if (!layer) {
        layer = qobject_cast<QgsVectorLayer*>(map->currentLayer());
    }
    if (!layer || layer->geometryType() != Qgis::GeometryType::Line) {
        showMapToolMessage(QString::fromUtf8(u8"道路溯源"), QString::fromUtf8(u8"请选择线要素图层（道路）。"), true);
        return;
    }

    const int fieldIndex = layer->fields().indexOf(QStringLiteral("link_id"));
    if (fieldIndex < 0) {
        showMapToolMessage(QString::fromUtf8(u8"道路溯源"), QString::fromUtf8(u8"图层缺少 link_id 字段。"), true);
        return;
    }

    QString resolveError;
    const QString tableFullName = resolveLayerTableFullName(ctx, layer, &resolveError);
    if (tableFullName.isEmpty()) {
        showMapToolMessage(QString::fromUtf8(u8"道路溯源"), resolveError, true);
        return;
    }

    disconnectPendingMapSelections(map);

    map->setCurrentLayer(layer);
    map->activateSpatialSelectTool(TNMapWidget::SpatialToolType::PointSelect, true);
    map->showMessage(QString::fromUtf8(u8"请在地图上点选一条路段"), 2500);

    QPointer<QgsVectorLayer> roadLayerGuard(layer);
    QPointer<TNMapWidget> mapGuard(map);
    const int linkFieldIndex = fieldIndex;
    const bool isBaseDataMap = ctx.isBaseDataMap;
    const int caseId = ctx.caseId;
    auto onSchemeChanged = ctx.onSchemeDataChanged;
    auto onBaseChanged = ctx.onBaseDataChanged;
    auto loadSchemeAux = ctx.loadSchemeAuxiliaryLayers;
    auto loadBaseAux = ctx.loadBaseAuxiliaryLayers;
    auto syncLegend = ctx.syncMapLegendFromGraduatedLayer;
    const QList<CaseAuxLayer::AuxLayerTarget> auxTargets = CaseAuxLayer::targetsForPathTrace(tableFullName);

    m_flowSelectionConn = connect(map, &TNMapWidget::selectionChanged, this,
        [this, mapGuard, roadLayerGuard, ctx, tableFullName, linkFieldIndex, isBaseDataMap, caseId,
         onSchemeChanged, onBaseChanged, loadSchemeAux, loadBaseAux, syncLegend, auxTargets](
            const QgsVectorLayer* selLayer, const QgsFeatureIds& selected, const QgsFeatureIds& /*deselected*/) {
            if (!mapGuard || !roadLayerGuard || !selLayer) {
                return;
            }

            if (selLayer != roadLayerGuard.data()) {
                if (auto* otherLayer = qobject_cast<QgsVectorLayer*>(mapGuard->selectedLayer())) {
                    if (otherLayer != roadLayerGuard.data()) {
                        otherLayer->removeSelection();
                    }
                }
                return;
            }

            if (selected.isEmpty()) {
                mapGuard->showMessage(QString::fromUtf8(u8"未选中要素，请重新点选路段"), 2000);
                return;
            }

            QgsFeature feature = roadLayerGuard->getFeature(*selected.constBegin());
            bool ok = false;
            const int linkId = feature.attribute(linkFieldIndex).toInt(&ok);
            if (!ok) {
                showMapToolMessage(QString::fromUtf8(u8"道路溯源"), QString::fromUtf8(u8"link_id 无效。"), true);
                return;
            }

            if (!beginGisAsyncOp(ctx)) {
                showMapToolMessage(QString::fromUtf8(u8"道路溯源"),
                    QString::fromUtf8(u8"其他 GIS 操作进行中，请稍候。"), true);
                return;
            }

            QObject::disconnect(m_flowSelectionConn);
            m_flowConnActive = false;
            mapGuard->activateSpatialSelectTool(TNMapWidget::SpatialToolType::PointSelect, false);
            roadLayerGuard->removeSelection();

            mapGuard->showMessage(
                QString::fromUtf8(u8"正在对 link_id = %1 执行道路溯源").arg(linkId), 2500);

            HttpApis::getTrace(ctx.authToken, ctx.projectId, ctx.caseId, tableFullName, linkId,
                [this, mapGuard, tableFullName, isBaseDataMap, caseId, linkId, onSchemeChanged, onBaseChanged,
                 loadSchemeAux, loadBaseAux, syncLegend, auxTargets](
                    bool success, const QSharedPointer<TraceListJson>& response, const QString& errorMsg) {
                    if (success) {
                        int pathCount = 0;
                        if (!response.isNull()) {
                            pathCount = response->j_data().size();
                        }

                        CaseWfsMap::refreshWfsCapabilitiesCache();
                        if (mapGuard) {
                            removeTraceTmpLayers(mapGuard.data());
                        }

                        auto loadAuxiliaryLayers = [mapGuard, isBaseDataMap, caseId, loadSchemeAux, loadBaseAux,
                                                    syncLegend, auxTargets, tableFullName]() {
                            if (isBaseDataMap) {
                                if (loadBaseAux && mapGuard) {
                                    loadBaseAux(mapGuard, auxTargets);
                                }
                            } else if (loadSchemeAux && mapGuard) {
                                loadSchemeAux(mapGuard, caseId, auxTargets);
                            }
                            if (mapGuard) {
                                schedulePathTraceStyle(mapGuard.data(), tableFullName, syncLegend);
                            }
                        };

                        if (isBaseDataMap) {
                            if (onBaseChanged) {
                                onBaseChanged(loadAuxiliaryLayers);
                            } else {
                                loadAuxiliaryLayers();
                            }
                        } else if (onSchemeChanged) {
                            onSchemeChanged(loadAuxiliaryLayers);
                        } else {
                            loadAuxiliaryLayers();
                        }

                        if (mapGuard) {
                            mapGuard->showMessage(QString::fromUtf8(u8"道路溯源成功"), 2000);
                        }
                        const QString detail = pathCount > 0
                            ? QString::fromUtf8(u8"道路溯源成功，共 %1 条路径。").arg(pathCount)
                            : QString::fromUtf8(u8"道路溯源成功。");
                        showMapToolMessage(QString::fromUtf8(u8"道路溯源"), detail, false);
                        qInfo() << QString::fromUtf8(u8"道路溯源 link_id=%1 完成，路径数=%2")
                                       .arg(linkId)
                                       .arg(pathCount);
                    } else {
                        const QString msg =
                            errorMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"接口失败") : errorMsg.trimmed();
                        showMapToolMessage(QString::fromUtf8(u8"道路溯源"), msg, true);
                    }
                    endGisAsyncOp();
                });
        });
    m_flowConnActive = true;
}

void CaseMapToolController::runAiWaySelect(TNMapWidget* map,
                                           QgsVectorLayer* wayLayer,
                                           std::function<void(const QStringList& linkIds)> onComplete)
{
    disconnectPendingMapSelections(map);

    if (!map || !wayLayer || !onComplete) {
        if (onComplete) {
            onComplete({});
        }
        return;
    }

    const int linkFieldIndex = wayLayer->fields().indexOf(QStringLiteral("link_id"));
    if (linkFieldIndex < 0) {
        showMapToolMessage(QString::fromUtf8(u8"数据选择"), QString::fromUtf8(u8"图层缺少 link_id 字段。"), true);
        onComplete({});
        return;
    }

    QPointer<TNMapWidget> mapPtr = map;
    QPointer<QgsVectorLayer> wayLayerPtr = wayLayer;

    map->setCurrentLayer(wayLayer);
    map->activateSpatialSelectTool(TNMapWidget::SpatialToolType::RectSelect, true);
    map->showMessage(QString::fromUtf8(u8"请在地图上框选路段"), 2500);

    m_aiWaySelectionConn = connect(map, &TNMapWidget::selectionChanged, this,
        [this, mapPtr, wayLayerPtr, linkFieldIndex, onComplete](
            const QgsVectorLayer* selLayer, const QgsFeatureIds& /*selected*/,
            const QgsFeatureIds& /*deselected*/) {
            if (!mapPtr || !wayLayerPtr || selLayer != wayLayerPtr.data()) {
                return;
            }

            QObject::disconnect(m_aiWaySelectionConn);
            m_aiWayConnActive = false;
            mapPtr->activateSpatialSelectTool(TNMapWidget::SpatialToolType::RectSelect, false);

            QStringList linkIds;
            const QgsFeatureIds featureIds = wayLayerPtr->selectedFeatureIds();
            linkIds.reserve(featureIds.size());
            for (QgsFeatureId fid : featureIds) {
                const QgsFeature feature = wayLayerPtr->getFeature(fid);
                if (!feature.isValid()) {
                    continue;
                }
                bool ok = false;
                const int linkId = feature.attribute(linkFieldIndex).toInt(&ok);
                if (ok) {
                    linkIds.append(QString::number(linkId));
                }
            }

            if (linkIds.isEmpty()) {
                mapPtr->showMessage(QString::fromUtf8(u8"未选中路段，请重试"), 2000);
            }

            onComplete(linkIds);
        });
    m_aiWayConnActive = true;
}

void CaseMapToolController::runCompareExtentPick(TNMapWidget* map,
                                                 std::function<void(const QString& checkExtentGeoJson)> onComplete)
{
    disconnectPendingMapSelections(map);

    if (!map || !onComplete) {
        if (onComplete) {
            onComplete({});
        }
        return;
    }

    TnMapToolCompareExtent* extentTool = map->compareExtentTool();
    if (!extentTool) {
        onComplete({});
        return;
    }

    QPointer<TNMapWidget> mapPtr = map;

    auto finish = [this, mapPtr, onComplete](const QString& geoJson) {
        disconnectCompareExtentPick(mapPtr.data());
        onComplete(geoJson);
    };

    m_compareExtentSelectedConn = connect(extentTool, &TnMapToolCompareExtent::extentSelected, this,
        [finish](const QString& geoJson) {
            finish(geoJson.trimmed().isEmpty() ? QString() : geoJson);
        });
    m_compareExtentCancelledConn = connect(extentTool, &TnMapToolCompareExtent::extentPickCancelled, this,
        [finish]() {
            finish({});
        });
    m_compareExtentTooSmallConn = connect(extentTool, &TnMapToolCompareExtent::extentTooSmall, this,
        [mapPtr]() {
            if (mapPtr) {
                mapPtr->showMessage(QString::fromUtf8(u8"框选范围过小，请重新框选"), 2500);
            }
        });
    m_compareExtentConnActive = true;

    map->activateCompareExtentTool(true);
    map->showMessage(QString::fromUtf8(u8"请在地图上框选对比地理范围"), 2500);
}

void CaseMapToolController::runAttributeFilter(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (!map) {
        return;
    }
    if (!ctx.schemeLayerTree) {
        showMapToolMessage(QString::fromUtf8(u8"按属性过滤"), QString::fromUtf8(u8"图层树未就绪。"), true);
        return;
    }

    QString errorMsg;
    QgsVectorLayer* layer =
        CaseMapLayerResolve::resolveVectorLayer(ctx.schemeLayerTree, map, ctx.caseId, &errorMsg);
    if (!layer) {
        showMapToolMessage(QString::fromUtf8(u8"按属性过滤"), errorMsg, true);
        return;
    }

    TNLayerAttributeFilterDialog dialog(map, layer, nullptr);
    if (ctx.onFilterApplied) {
        QObject::connect(&dialog, &TNLayerAttributeFilterDialog::filterApplied,
            [cb = ctx.onFilterApplied]() { cb(); });
    }
    dialog.exec();
}

void CaseMapToolController::runExportMap(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (!map || !map->canvas()) {
        showMapToolMessage(QString::fromUtf8(u8"保存画布"), QString::fromUtf8(u8"地图画布未就绪。"), true);
        return;
    }

    CaseMapExportDialog dialog(map, ctx.exportNameContext, nullptr);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const CaseMapExportOptions options = dialog.options();
    if (options.outputPath.trimmed().isEmpty()) {
        showMapToolMessage(QString::fromUtf8(u8"保存画布"), QString::fromUtf8(u8"输出路径为空。"), true);
        return;
    }

    exportMapWithOptions(map, options, MapExportFeedback::Manual);
}

void CaseMapToolController::exportMapWithOptions(TNMapWidget* map,
                                                 const CaseMapExportOptions& options,
                                                 MapExportFeedback feedback)
{
    if (!map || !map->canvas()) {
        showMapToolMessage(QString::fromUtf8(u8"保存画布"), QString::fromUtf8(u8"地图画布未就绪。"), true);
        return;
    }
    if (options.outputPath.trimmed().isEmpty()) {
        showMapToolMessage(QString::fromUtf8(u8"保存画布"), QString::fromUtf8(u8"输出路径为空。"), true);
        return;
    }

    map->canvas()->stopRendering();
    map->canvas()->waitWhileRendering();
    // GUI 线程预先抓取图例 overlay，供子线程渲染后合成
    CaseMapExportOptions exportOptions = options;
    exportOptions.legendOverlay = map->grabBottomLeftOverlay();
    const QgsMapSettings settings = CaseMapCanvasExporter::buildExportSettings(map, exportOptions);

    auto* loading = new LoadingDialog(QString::fromUtf8(u8"正在输出地图，请稍候…"), nullptr,
        LoadingDialog::Theme::MapExport);
    loading->setAttribute(Qt::WA_DeleteOnClose);
    loading->showNonBlocking();

    auto* thread = new QThread(this);
    auto* worker = new CaseMapExportWorker();
    worker->moveToThread(thread);

    const QPointer<TNMapWidget> mapGuard(map);
    const QPointer<LoadingDialog> loadingGuard(loading);
    const QString outputPath = options.outputPath.trimmed();
    const bool isAuto = (feedback == MapExportFeedback::Auto);

    connect(thread, &QThread::started, worker, [worker, settings, exportOptions]() {
        worker->exportToFile(settings, exportOptions);
    });
    connect(worker, &CaseMapExportWorker::finished, thread, &QThread::quit);
    connect(worker, &CaseMapExportWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(worker, &CaseMapExportWorker::finished, this,
        [mapGuard, loadingGuard, outputPath, isAuto](bool success, const QString& errorMessage) {
            if (loadingGuard) {
                loadingGuard->closeDialog();
            }
            if (mapGuard && mapGuard->canvas()) {
                mapGuard->canvas()->refresh();
            }
            if (success) {
                if (isAuto) {
                    qInfo() << QString(u8"自动保存画布成功：%1").arg(outputPath);
                } else {
                    showMapToolMessage(QString::fromUtf8(u8"保存画布"),
                        QString::fromUtf8(u8"地图已保存至：%1").arg(outputPath), false);
                }
            } else {
                const QString msg =
                    errorMessage.trimmed().isEmpty() ? QString::fromUtf8(u8"导出失败。") : errorMessage.trimmed();
                showMapToolMessage(QString::fromUtf8(u8"保存画布"), msg, true);
            }
        },
        Qt::QueuedConnection);

    thread->start();
}

void CaseMapToolController::runUndoEdits(TNMapWidget* map)
{
    if (!map) {
        return;
    }

    TNGisEditStackManager* stack = map->editStackManager();
    if (!stack || !stack->canUndo()) {
        return;
    }

    stack->undo();
}

void CaseMapToolController::runSaveEdits(TNMapWidget* map, const CaseMapToolContext& ctx)
{
    if (!map) {
        showMapToolMessage(QString::fromUtf8(u8"保存编辑"), QString::fromUtf8(u8"地图未就绪。"), true);
        return;
    }

    TNGisEditStackManager* stack = map->editStackManager();
    if (!stack) {
        showMapToolMessage(QString::fromUtf8(u8"保存编辑"), QString::fromUtf8(u8"编辑栈未就绪。"), true);
        return;
    }

    if (!stack->hasUnsavedChanges()) {
        showMapToolMessage(QString::fromUtf8(u8"保存编辑"), QString::fromUtf8(u8"当前没有待保存的编辑内容。"), false);
        return;
    }

    QString summary;
    const bool ok = stack->saveAll(&summary);
    showMapToolMessage(QString::fromUtf8(u8"保存编辑"), summary, !ok);

    if (ctx.onFilterApplied) {
        ctx.onFilterApplied();
    }
}
