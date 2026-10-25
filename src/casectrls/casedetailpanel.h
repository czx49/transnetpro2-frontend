/*****************************************************************//**
 * \file   casedetailpanel.h
 * \brief  方案详情面板（标准视图与双屏对比共用单工具栏）
 * \details 地图工具栏悬浮于地图区上方；标准/双屏仅切换下方视图栈
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef CASEDETAILPANEL_H
#define CASEDETAILPANEL_H

#include <QWidget>

#include "casecomparerenderhelper.h"
#include "caseitem.h"
#include "casemaptoolbarwidget.h"

class CaseMapAttributeWidget;
class CaseMapLegendWidget;
class CaseTreeModel;
class CaseTreeView;
class CaseWorkPanel;
class QEvent;
class QComboBox;
class QLabel;
class QMenu;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QVBoxLayout;
class QgsMapMouseEvent;
class QgsVectorLayer;
class TNMapWidget;
struct MapStatus;

class CaseDetailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CaseDetailPanel(QWidget* parent = nullptr);
    ~CaseDetailPanel() override;

    void setCaseInfo(const CaseItem& caseItem);
    void setCaseList(const QList<CaseItem>& caseList);
    QStringList getSelectedLayers() const;

    void setToolbarViewMode(CaseMapToolbarWidget::ViewMode mode);
    void setCompareCases(const CompareCaseItem& leftCase, const CompareCaseItem& rightCase);
    void addDiffMarker(const MapDiffMarker& marker);
    void clearDiffMarkers();

    /** @brief false：嵌入 CasePanel 工作台时隐藏内置方案侧栏，避免与外层方案树重复 */
    void setBuiltInSchemeSidebarVisible(bool visible);

    TNMapWidget* standardMapWidget() const { return m_mapWidget; }
    CaseMapLegendWidget* standardLegendWidget() const { return m_stdLegendWidget; }
    CaseMapLegendWidget* leftCompareLegendWidget() const { return m_leftCompareLegendWidget; }
    TNMapWidget* leftCompareMapWidget() const { return m_leftMapWidget; }
    TNMapWidget* rightCompareMapWidget() const { return m_rightMapWidget; }
    CaseMapToolbarWidget::ViewMode toolbarViewMode() const;
    CaseMapToolbarWidget* schemeToolbarWidget() const { return m_toolbarWidget; }
    int leftCompareCaseId() const { return m_leftCompareCase.id; }
    int rightCompareCaseId() const { return m_rightCompareCase.id; }

    CaseWorkPanel* workPanel() const { return m_workPanel; }

    /** @brief 内置方案树（与外层工作台树共用 CaseTreeView 行为） */
    CaseTreeView* embeddedSchemeTreeView() const { return m_caseTreeView; }

    /** @brief 定义查询变更后刷新标准视图属性表（清除列筛选并 reload） */
    void refreshStandardAttributeFromMap();

    /** @brief 切换到标准视图「数据」页（不刷新属性表） */
    void switchToStandardDataView();
    CaseMapAttributeWidget* standardAttributeWidget() const { return m_stdAttributeWidget; }
    CaseMapAttributeWidget* compareAttributeWidget() const { return m_cmpAttributeWidget; }

signals:
    void newCaseRequested();
    void renderClicked();
    void dataClicked();
    void layerStateChanged(const QString& layerName, bool checked);
    void caseOpened(int caseId);
    void mapToolClicked(CaseMapToolbarWidget::ToolType toolType);
    /** @brief 需由 CasePanel 统一写入默认对比组合并切换对比视图（等同原工作台「方案对比」） */
    void schemeCompareFlowRequested();
    /** @brief 嵌入工作台时由「返回方案列表」发出，切回卡片列表 */
    void schemeListReturnRequested();
    /** @brief 基础方案工作台「改造方案」：请求新建普通方案 */
    void deriveCaseFromBaseRequested();
    /** @brief 标准视图地图右键「中微观诊断」 */
    void trafficMicroDiagnosisRequested();
    void compareCasesChanged(int leftCaseId, int rightCaseId);
    void compareRenderStyleChanged(CompareRenderStyle style);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onToolbarViewModeChanged(CaseMapToolbarWidget::ViewMode mode);
    void onToolbarToolTriggered(CaseMapToolbarWidget::ToolType toolType);
    void onCompareSelectionChanged(const QString& compareName);
    void onStdRenderClicked();
    void onStdDataClicked();
    void onCmpRenderClicked();
    void onCmpDataClicked();
    void onCmpRenderStyleComboChanged(int index);
    void onStdMapStatusChanged(const MapStatus& mapStatus);
    void onCompareMapStatusChanged(const MapStatus& mapStatus);
    void onLeftMapLayerChanged();
    void onStdMapContextMenuAboutToShow(QMenu* menu, QgsMapMouseEvent* event);

private:
    QWidget* createSidebar();
    QWidget* createMapCanvas();
    QWidget* createStandardSchemePage();
    QWidget* createCompareSchemePage();
    QWidget* createStdRenderPage();
    QWidget* createStdDataPage();
    QWidget* createStdMapFooter();
    QWidget* createCmpRenderPage();
    QWidget* createCmpDataPage();
    QWidget* createCmpMapFooter();
    void applySchemeViewMode(CaseMapToolbarWidget::ViewMode mode);
    void relayoutFloatingToolbar();
    QString currentCompareDisplayName() const;
    /** @brief 双屏对比下工作台绑定的左侧方案（含 projectId / isBase 等完整字段） */
    CaseItem leftCompareWorkCase() const;
    void setActiveCompareMap(TNMapWidget* mapWidget);
    /** @brief 双屏对比：将 source 的地理中心与比例尺同步到对侧（防重入） */
    void syncCompareMapViewportFrom(TNMapWidget* sourceMap);
    void updateStatusLabels(const MapStatus& mapStatus);
    void refreshCompareToolbarOptions();
    /** @brief 数据页隐藏悬浮地图工具栏，渲染页显示 */
    void syncFloatingToolbarForRenderDataView();
    void resetCompareRenderStyleCombo();
    /** @brief 标准视图地图 road_way 已加载且选择集非空时弹出中微观诊断菜单 */
    bool tryShowTrafficMicroContextMenu(const QPoint& globalPos);
    QgsVectorLayer* findLoadedRoadWayLayerWithSelection() const;

    CaseItem m_currentCase;
    QList<CaseItem> m_caseList;
    QList<CompareCaseItem> m_availableCompareCases;
    QList<MapDiffMarker> m_diffMarkers;
    CompareCaseItem m_leftCompareCase;
    CompareCaseItem m_rightCompareCase;

    QVBoxLayout* m_mainLayout;
    CaseTreeModel* m_caseTreeModel;
    CaseTreeView* m_caseTreeView;

    QWidget* m_toolbarHost;
    QStackedWidget* m_schemeViewStack;
    CaseMapToolbarWidget* m_toolbarWidget;

    TNMapWidget* m_mapWidget;
    CaseMapLegendWidget* m_stdLegendWidget;
    CaseMapLegendWidget* m_leftCompareLegendWidget;
    QStackedWidget* m_stdContentStack;
    CaseMapAttributeWidget* m_stdAttributeWidget;
    QPushButton* m_stdRenderButton;
    QPushButton* m_stdDataButton;

    TNMapWidget* m_leftMapWidget;
    TNMapWidget* m_rightMapWidget;
    TNMapWidget* m_activeCompareMap;
    bool m_compareViewportSyncGuard = false;
    QStackedWidget* m_cmpContentStack;
    CaseMapAttributeWidget* m_cmpAttributeWidget;
    QPushButton* m_cmpRenderButton;
    QPushButton* m_cmpDataButton;
    QComboBox* m_cmpRenderStyleCombo;

    CaseWorkPanel* m_workPanel;
    QLabel* m_caseTitleLabel;
    QLabel* m_stdFooterCrsLabel;
    QLabel* m_stdFooterCoordLabel;
    QLabel* m_stdFooterScaleLabel;
    QLabel* m_cmpFooterCrsLabel;
    QLabel* m_cmpFooterCoordLabel;
    QLabel* m_cmpFooterScaleLabel;
    QPushButton* m_sidebarCompareButton;
    QPushButton* m_returnToSchemeListButton;
    QWidget* m_schemeSidebarWidget;
    QSplitter* m_bodySplitter;
};

#endif // CASEDETAILPANEL_H
