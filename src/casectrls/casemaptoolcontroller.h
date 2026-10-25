/*****************************************************************//**
 * \file   casemaptoolcontroller.h
 * \brief  方案/基础数据地图工具条动作分发
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef CASEMAPTOOLCONTROLLER_H
#define CASEMAPTOOLCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

#include "caseauxlayerhelper.h"
#include "casemapexportdialog.h"
#include "casemaptoolbarwidget.h"

class CaseTreeView;
class TNMapWidget;
class QgsFeature;
class QgsVectorLayer;

struct CaseMapToolContext {
    int projectId = -1;
    int caseId = -1;
    QString authToken;
    bool isBaseDataMap = false;
    /** @brief 当前方案是否为基础方案；清除符号化时恢复路段默认 type 过滤 */
    bool isBaseCase = false;
    CaseMapExportNameContext exportNameContext;
    CaseTreeView* schemeLayerTree = nullptr;
    std::function<void()> onFilterApplied;
    std::function<void(std::function<void()> afterTreeSynced)> onSchemeDataChanged;
    std::function<void(std::function<void()> afterTreeSynced)> onBaseDataChanged;
    std::function<QString(QgsVectorLayer* layer, QString* errorMessage)> resolveTableFullName;
    std::function<void(TNMapWidget* map, int caseId, const QList<CaseAuxLayer::AuxLayerTarget>& targets)>
        loadSchemeAuxiliaryLayers;
    std::function<void(TNMapWidget* map, const QList<CaseAuxLayer::AuxLayerTarget>& targets)> loadBaseAuxiliaryLayers;
    std::function<void(bool)> onGisAsyncBusyChanged;
    /** @brief 清除符号化时静默反勾选侧栏 trace_tmp 节点；由 CasePanel 注入 */
    std::function<void()> uncheckTraceTmpLayers;
    /** @brief 道路溯源样式应用后同步图例；由 CasePanel 注入 */
    std::function<void(QgsVectorLayer* layer, const QString& fieldName)> syncMapLegendFromGraduatedLayer;
    /** @brief 期望线 API 成功且辅助图层加载完成后调用；由 CasePanel 注入后置逻辑 */
    std::function<void(std::function<void(bool ok, const QString& err)> onComplete)> onDesirelineSucceeded;
    /** @brief 激活添加要素工具前准备（如自动加载点图层） */
    std::function<void(TNMapWidget* map)> prepareAddFeature;
    /** @brief 添加要素绘制完成后的属性预填 */
    std::function<void(QgsVectorLayer* layer, QgsFeature& feature, QStringList* readOnlyFieldNames)> prefillFeatureAttributes;
};

enum class MapExportFeedback {
    Manual = 0,
    Auto
};

class CaseMapToolController : public QObject {
    Q_OBJECT

public:
    explicit CaseMapToolController(QObject* parent = nullptr);
    ~CaseMapToolController() override;

    void runTool(TNMapWidget* map, CaseMapToolbarWidget::ToolType tool, const CaseMapToolContext& ctx);
    void exportMapWithOptions(TNMapWidget* map,
                              const CaseMapExportOptions& options,
                              MapExportFeedback feedback = MapExportFeedback::Manual);
    void runAiWaySelect(TNMapWidget* map,
                        QgsVectorLayer* wayLayer,
                        std::function<void(const QStringList& linkIds)> onComplete);
    void cancelAiWaySelect(TNMapWidget* map);
    void runCompareExtentPick(TNMapWidget* map,
                              std::function<void(const QString& checkExtentGeoJson)> onComplete);
    void cancelCompareExtentPick(TNMapWidget* map);

private:
    void applyMutexTool(TNMapWidget* map, CaseMapToolbarWidget::ToolType tool);
    void runClip(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runSymbol(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runClearSymbol(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runDesireline(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runFlowTrace(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runAttributeFilter(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runExportMap(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runSaveEdits(TNMapWidget* map, const CaseMapToolContext& ctx);
    void runUndoEdits(TNMapWidget* map);
    void disconnectFlowSelection(TNMapWidget* map);
    void disconnectAiWaySelection(TNMapWidget* map);
    void disconnectCompareExtentPick(TNMapWidget* map);
    void disconnectPendingMapSelections(TNMapWidget* map);
    bool beginGisAsyncOp(const CaseMapToolContext& ctx);
    void endGisAsyncOp();

    QMetaObject::Connection m_flowSelectionConn;
    bool m_flowConnActive = false;
    QMetaObject::Connection m_aiWaySelectionConn;
    bool m_aiWayConnActive = false;
    QMetaObject::Connection m_compareExtentSelectedConn;
    QMetaObject::Connection m_compareExtentCancelledConn;
    QMetaObject::Connection m_compareExtentTooSmallConn;
    bool m_compareExtentConnActive = false;
    bool m_gisAsyncBusy = false;
    std::function<void(bool)> m_gisAsyncBusyNotifier;
};

#endif // CASEMAPTOOLCONTROLLER_H
