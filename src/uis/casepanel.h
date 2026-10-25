/*****************************************************************//**
 * \file   casepanel.h
 * \brief  方案工作台
 * \details 管理项目方案列表、基础数据、标准视图和双屏对比视图
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef CASEPANEL_H
#define CASEPANEL_H

#include <QFrame>
#include <QSet>
#include <QStringList>
#include <QWidget>

#include <functional>

#include "../casectrls/caseauxlayerhelper.h"
#include "../casectrls/caseitem.h"
#include "../casectrls/casemapexportdialog.h"
#include "../casectrls/casecomparerenderhelper.h"
#include "../casectrls/casewaylayerstyleapply.h"
#include "../casectrls/casemaptoolbarwidget.h"

#include "caseworkpanel.h"

class QModelIndex;

class BaseDataTableView;
struct BaseDataLayerGroupNodeItem;
struct BaseDataToolNodeItem;
class CaseDetailPanel;
class CaseMapAttributeWidget;
class CaseMapLegendWidget;
class CaseMapToolController;
class CaseTreeModel;
class CaseTreeView;
class QButtonGroup;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class TNMapWidget;
class QVBoxLayout;
class QgsVectorLayer;

class CaseInfoWidget : public QFrame {
    Q_OBJECT

public:
    explicit CaseInfoWidget(QWidget* parent = nullptr);
    ~CaseInfoWidget() override;

    void setCaseInfo(const CaseItem& caseItem);
    int caseId() const;

signals:
    void viewButtonClicked(int caseId);
    void deleteButtonClicked(int caseId);

private:
    QLabel* m_nameLabel;
    QLabel* m_statsLabel;
    QLabel* m_timeLabel;
    QPushButton* m_viewButton;
    QPushButton* m_deleteButton;
    int m_caseId;
};

class CasePanel : public QWidget {
    Q_OBJECT

public:
    explicit CasePanel(QWidget* parent = nullptr);
    ~CasePanel() override;

    void setProjectId(int projectId);
    void setProjectName(const QString& projectName);
    /** 关闭项目 Tab 前释放异步资源，避免销毁期间回调或子控件重复删除 */
    void prepareForClose();
    void setCaseList(const QList<CaseItem>& caseList);
    void addCase(const CaseItem& item);
    void removeCase(int caseId);
    void updateCase(const CaseItem& item);

signals:
    void newCaseRequested(int caseId);
    void caseCompareRequested();
    void caseOpened(int caseId);
    void caseEdited(int caseId);
    void caseDeleted(int caseId);
    void caseRenamed(int caseId, const QString& newName);
    void caseCloned(int caseId);

private slots:
    void onSchemeTabClicked();
    void onBaseDataTabClicked();
    void onNewCaseClicked();
    void onCaseCompareClicked();
    void onCaseOpened(int caseId);
    void onCaseEdited(int caseId);
    void onCaseDeleted(int caseId);
    void onCaseRenameRequested(int caseId);
    void onCaseRenamed(int caseId, const QString& newName);
    void onCaseCloned(int caseId);
    void onCaseOverwriteRequested(int caseId);
    void onUiEventNotify(int uiEvent, const QVariantList& params);
    void onSchemeListReturnRequested();
    void onCaseTreeLayerCheckChanged(const QModelIndex& index, Qt::CheckState state);
    void onSchemeLayerActivated(int caseId, const QString& wfsTypeName);
    void onSchemeLayerDataViewRequested(int caseId, const QString& wfsTypeName);
    void onBaseDataLayerDataViewRequested(const QString& wfsTypeName, const QVariantMap& userData);
    void onBaseDataLayerCheckChanged(const QString& layerName, bool checked, const QVariantMap& userData);
    void onDetailCompareCasesChanged(int leftCaseId, int rightCaseId);
    void onCompareRenderStyleChanged(CompareRenderStyle style);
    void onDetailToolbarViewModeChanged(CaseMapToolbarWidget::ViewMode mode);
    void onSchemeMapToolTriggered(CaseMapToolbarWidget::ToolType toolType);
    void onBaseMapToolTriggered(CaseMapToolbarWidget::ToolType toolType);
    void onAnalysisResultViewed(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                const QString& resultName);
    void onAnalysisModuleChanged(CaseWorkPanel::AnalysisModule module);
    void onWorkStepRunFinished(const QString& stepKey);
    void onAnalysisDataRequested(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                 const QString& tableName, const QString& resultName);
    void onAiDataSelectRequested(CaseWorkPanel::AnalysisModule module, const QString& stepKey, bool activate);
    void onTrafficMicroDiagnosisRequested();
    void onBaseRenderClicked();
    void onBaseDataClicked();
    void onSchemeLayerCsvImportRequested(int caseId, const QString& table, const QString& layerDisplayName);
    void onSchemeLayerExportRequested(int caseId, const QString& table, const QString& exportType, const QString& layerDisplayName);
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    void onSchemeLayerZoomToRequested(int caseId, const QString& wfsTypeName, const QString& layerDisplayName);
    void onBaseDataLayerZoomToRequested(const QString& wfsTypeName, const QString& layerDisplayName);
#endif
    void onBaseDataLayerCsvImportRequested(const QString& table, const QString& layerDisplayName);
    void onBaseDataLayerExportRequested(const QString& table, const QString& exportType, const QString& layerDisplayName);
    void openPopGenerationDialog();
    void finishPopGenerationSuccess();
    void openCommunityGenerationDialog();
    void finishCommunityGenerationSuccess();

private:
    void initUi();
    QWidget* createWorkspacePage();
    QWidget* createSchemeSidebar();
    QWidget* createSchemeContentPage();
    QWidget* createBaseDataPage();
    QWidget* createModeSwitchTabs();
    QWidget* createBaseMapCanvas();
    QWidget* createBaseMapFooter(QWidget* parentWidget);
    void syncBaseFloatingToolbarForRenderDataView();
    void switchToBaseDataView();
    void refreshBaseAttributeFromMap();
    void updateModeButtons();
    void refreshCaseCards();
    void refreshCasesFromGlobalState();
    void refreshCurrentCaseLayerTreeInPlace(std::function<void(bool synced)> onComplete = nullptr);
    void refreshBaseDataLayerTreeInPlace(std::function<void(bool synced)> onComplete = nullptr);
    void refreshBaseDataTreeFromProjectDetail();
    void reloadBaseDataMenusFromApi(std::function<void(bool success)> onComplete = nullptr);
    QSet<QString> collectBaseDataTypenames(const QList<BaseDataLayerGroupNodeItem>& groups) const;
    QStringList collectCaseTypenames(int caseId = -1) const;
    void scheduleProjectSchemaWarm(bool prioritizeCaseId = false, int caseId = -1);
    /** @brief 手动强制刷新 WFS Caps，并按当前基础数据或当前方案预热 Schema */
    void refreshWfsServiceInfo(bool isBaseDataMap);
    void loadAuxiliaryLayersOnSchemeMap(TNMapWidget* map, int caseId,
                                        const QList<CaseAuxLayer::AuxLayerTarget>& targets);
    void loadAuxiliaryLayersOnBaseMap(TNMapWidget* map, const QList<CaseAuxLayer::AuxLayerTarget>& targets);
    QList<BaseDataLayerGroupNodeItem> buildBaseLayerGroupNodes() const;
    QList<BaseDataToolNodeItem> buildBaseToolNodes() const;
    void applyProjectOpenState(int stateValue, const QString& errorMessage);
    void refreshProjectCasesAndBaseData(bool clearDetailWfs);
    void refreshProjectCasesInPlace();
    void resetSchemeWorkspaceNavigation(const char* reason = nullptr);
    bool isSchemeDetailViewActive() const;
    int schemeWorkspaceCurrentIndex() const;
    void openCaseById(int caseId);
    void enterSchemeCompareView();
    int findBaseCaseId() const;
    bool ensureBaseCaseDetailForCompare(int baseCaseId);
    CompareCaseItem toCompareItem(const CaseItem& item) const;
    void onBaseDataToolNodeClicked(const QString& toolName, const QVariantMap& userData);
    void clearDetailPanelWfsLayers();
    void requestDrcsvLayerImport(int caseId, const QString& table, const QString& layerDisplayName);
    /** @brief SHP 导入：收集伴生文件打包 zip 后经 /api/files/drshpzip 上传 */
    void requestDrshpzipLayerImport(int caseId, const QString& table, const QString& shpPath, const QString& token);
    void requestProjectTableExport(int caseId, const QString& table, const QString& exportType, const QString& layerDisplayName);
    void runBaseDataOdGeneration(const QString& dialogTitle);
    void runBaseDataNetworkAnalysis(const QString& dialogTitle);
    void runBaseDataPopGeneration(const QString& dialogTitle);
    void runBaseDataCommunityGeneration(const QString& dialogTitle);
    void runBaseDataOdEstimation(const QString& dialogTitle);
    void ensureOtherOdLayerLoadedAndStyled(const std::function<void(bool success, const QString& error)>& done);
    void applyOtherOdStyleAfterDesireline(TNMapWidget* map,
                                          bool isBaseDataMap,
                                          int caseId,
                                          const std::function<void(bool success, const QString& error)>& done);
    bool currentCaseItem(CaseItem& out) const;
    bool isCurrentCaseBase() const;
    void applyAnalysisResultOnMap(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                  const QString& resultName);
    void drainPendingAnalysisResultView();
    void applyCompareRenderStyleToBothMaps();
    void syncLeftCompareLegendFromWayLayer(QgsVectorLayer* layer, WayLayerAnalysisStyle style);
    void resetSchemeDetailLegendToDefault();
    void syncSchemeDetailLegendFromWayLayer(QgsVectorLayer* layer, const QString& fieldName);
    void resetBaseMapLegendToDefault();
    void syncBaseMapLegendFromLayer(QgsVectorLayer* layer, const QString& fieldName);
    void autoSaveAnalysisMapCanvas(TNMapWidget* map, const QString& resultName);
    CaseMapExportNameContext mapExportNameContext(int caseId) const;
    void showAnalysisDataDialog(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                const QString& resultName);
    QgsVectorLayer* wayLayerOnStandardMap(CaseWorkPanel::AnalysisModule module, QString* errorOut = nullptr,
                                          QString* wfsUriOut = nullptr);
    QModelIndex findCaseTreeIndex(int caseId) const;
    void applyCaseTreeWfsToMap(TNMapWidget* map, int caseId, bool invalidatePending = true);
    void applyWfsForCaseTreeLayerIndex(TNMapWidget* map, const QModelIndex& layerIdx);
    void syncSchemeDetailWfsFromCaseTree();
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    TNMapWidget* mapWidgetForSchemeCase(int caseId) const;
#endif

    int m_projectId;
    QString m_projectName;
    QList<CaseItem> m_caseList;
    int m_currentCaseId;
    int m_pendingOpenCaseId = -1; ///< 新建改造方案成功后待自动打开的方案 id
    QSet<int> m_beforeCreateCaseIds; ///< 新建方案前的方案 id 快照（接口未返回 id 时按差集兜底）
    bool m_pendingOpenByDiff = false; ///< 创建接口未返回可解析 id，待列表刷新后按快照差集识别新方案

    QVBoxLayout* m_mainLayout;
    QWidget* m_workspacePage;
    QStackedWidget* m_sidebarStack;
    QStackedWidget* m_workspaceContentStack;
    QStackedWidget* m_schemeWorkspaceStack;
    int m_savedSchemeWorkspaceIndex;

    QPushButton* m_schemeTabButton;
    QPushButton* m_baseDataTabButton;
    QButtonGroup* m_modeButtonGroup;
    QLabel* m_projectNameLabel;
    QPushButton* m_newCaseButton;
    QPushButton* m_compareButton;
    QPushButton* m_newCaseMainButton;

    CaseTreeModel* m_caseModel;
    CaseTreeView* m_caseTreeView;
    BaseDataTableView* m_baseDataView;

    QScrollArea* m_caseScrollArea;
    QWidget* m_caseListWidget;
    QVBoxLayout* m_caseListLayout;
    TNMapWidget* m_baseMapWidget;
    CaseMapLegendWidget* m_baseLegendWidget = nullptr;
    QStackedWidget* m_baseContentStack = nullptr;
    CaseMapAttributeWidget* m_baseAttributeWidget = nullptr;
    QPushButton* m_baseRenderButton = nullptr;
    QPushButton* m_baseDataButton = nullptr;
    QWidget* m_baseMapToolbarHost = nullptr;
    QLabel* m_baseCrsStatusLabel;
    QLabel* m_baseCoordStatusLabel;
    QLabel* m_baseScaleStatusLabel;

    CaseDetailPanel* m_detailPanel;
    CompareRenderStyle m_compareRenderStyle = CompareRenderStyle::None;
    CaseMapToolbarWidget* m_baseMapToolbar;
    CaseMapToolController* m_mapToolController;
    QString m_activeAiDataSelectStepKey;
    QString m_lastToolOpenKey;
    qint64 m_lastToolOpenAtMs;
    bool m_uiEventConnected;
    bool m_preparedForClose = false;
    QString m_popGenTypeName;
    QString m_popGenDisplayName;
    QString m_popGenRasterPath;
    QString m_popGenToken;
    QString m_popGenTitle;
    QString m_popGenSuccessMessage;
    QString m_communityGenRoadWayTypeName;
    QString m_communityGenRoadWayDisplayName;
    QString m_communityGenCommunityTypeName;
    QString m_communityGenTargetTableDisplay;
    QString m_communityGenRasterPath;
    QString m_communityGenToken;
    QString m_communityGenTitle;
    QString m_communityGenSuccessMessage;

    /** @brief 上次基础数据树 WFS typename 快照；用于避免打开项目时无条件 force refresh Caps */
    QSet<QString> m_lastBaseDataTypenames;
    bool m_hasBaseDataTypenamesSnapshot = false;
    bool m_wfsRefreshBusy = false;
    int m_analysisApplyGeneration = 0;
    bool m_analysisStyleApplying = false;
    bool m_hasPendingAnalysisView = false;
    CaseWorkPanel::AnalysisModule m_pendingAnalysisModule = CaseWorkPanel::AnalysisModule::MotorVehicle;
    QString m_pendingAnalysisResultKey;
    QString m_pendingAnalysisResultName;
};

#endif // CASEPANEL_H
