/*****************************************************************//**
 * \file   casedetailpanel.cpp
 * \brief  方案详情面板实现（标准 + 双屏对比统一工具栏）
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "casedetailpanel.h"

#include "casecomparerenderhelper.h"
#include "casemaplayerresolve.h"
#include "casemaputils.h"
#include "casemapattributewidget.h"
#include "casemapfooter.h"
#include "casemaplegendwidget.h"
#include "casemaptoolbarwidget.h"
#include "casetreemodel.h"
#include "casetreeview.h"
#include "casewaylayerhelper.h"
#include "../publicctrls/phase2comboboxutil.h"
#include "../uis/caseworkpanel.h"
#include "../tnmapwidget/tnmapwidget.h"

#include <QAction>
#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsvectorlayer.h>

namespace {
constexpr int kFloatingToolbarTopMargin = 14;
/** 设计稿主色（活跃 Tab、分段选中等） */
constexpr char kDesignAccentBlue[] = "#1890ff";
/** 地图卡片底部状态条背景（贴近侧栏深色） */
constexpr char kDesignMapFooterBg[] = "#141c2c";
/** 地图视口白底圆角（方案-基础方案详情 / 基础数据） */
constexpr int kDesignMapViewportRadius = 10;

static const char kStdMapContextMenuQss[] =
    "QMenu {"
    "   background-color: #1a2639;"
    "   color: rgba(255,255,255,0.92);"
    "   border: 1px solid rgba(255,255,255,0.12);"
    "}"
    "QMenu::item {"
    "   padding: 8px 20px;"
    "}"
    "QMenu::item:selected {"
    "   background-color: rgba(33,150,243,0.30);"
    "}";

void applyDesignMapShellShadow(QWidget* shellWidget)
{
    if (!shellWidget) {
        return;
    }
    auto* shadow = new QGraphicsDropShadowEffect(shellWidget);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 82));
    shellWidget->setGraphicsEffect(shadow);
}

class SchemeToolbarHostWidget : public QWidget {
public:
    explicit SchemeToolbarHostWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_toolbar(nullptr)
    {
    }

    void setFloatingToolbar(QWidget* toolbar)
    {
        m_toolbar = toolbar;
        relayoutFloatingToolbar();
    }

    void relayoutFloatingToolbar()
    {
        if (!m_toolbar) {
            return;
        }
        if (auto* mapBar = qobject_cast<CaseMapToolbarWidget*>(m_toolbar)) {
            mapBar->relayoutToContent();
        } else {
            m_toolbar->adjustSize();
        }
        const int tw = m_toolbar->width();
        const int x = qMax(0, (width() - tw) / 2);
        m_toolbar->move(x, kFloatingToolbarTopMargin);
        m_toolbar->raise();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        relayoutFloatingToolbar();
    }

    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        relayoutFloatingToolbar();
    }

private:
    QWidget* m_toolbar;
};

} // namespace

CaseDetailPanel::CaseDetailPanel(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_caseTreeModel(new CaseTreeModel(this))
    , m_caseTreeView(nullptr)
    , m_toolbarHost(nullptr)
    , m_schemeViewStack(nullptr)
    , m_toolbarWidget(nullptr)
    , m_mapWidget(nullptr)
    , m_stdLegendWidget(nullptr)
    , m_leftCompareLegendWidget(nullptr)
    , m_stdContentStack(nullptr)
    , m_stdAttributeWidget(nullptr)
    , m_stdRenderButton(nullptr)
    , m_stdDataButton(nullptr)
    , m_leftMapWidget(nullptr)
    , m_rightMapWidget(nullptr)
    , m_activeCompareMap(nullptr)
    , m_cmpContentStack(nullptr)
    , m_cmpAttributeWidget(nullptr)
    , m_cmpRenderButton(nullptr)
    , m_cmpDataButton(nullptr)
    , m_cmpRenderStyleCombo(nullptr)
    , m_workPanel(nullptr)
    , m_caseTitleLabel(nullptr)
    , m_stdFooterCrsLabel(nullptr)
    , m_stdFooterCoordLabel(nullptr)
    , m_stdFooterScaleLabel(nullptr)
    , m_cmpFooterCrsLabel(nullptr)
    , m_cmpFooterCoordLabel(nullptr)
    , m_cmpFooterScaleLabel(nullptr)
    , m_sidebarCompareButton(nullptr)
    , m_returnToSchemeListButton(nullptr)
    , m_schemeSidebarWidget(nullptr)
    , m_bodySplitter(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("CaseDetailPanel { background-color: #030d20; }");

    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    QWidget* sidebar = createSidebar();
    m_schemeSidebarWidget = sidebar;

    QWidget* rightWrap = new QWidget(this);
    rightWrap->setObjectName(QStringLiteral("caseDetailRightWrap"));
    rightWrap->setAttribute(Qt::WA_StyledBackground, true);
    rightWrap->setMinimumWidth(560);
    rightWrap->setStyleSheet(QStringLiteral(
        "#caseDetailRightWrap { background-color: #030d20; }"));
    QHBoxLayout* rightLayout = new QHBoxLayout(rightWrap);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(createMapCanvas(), 1);
    m_workPanel = new CaseWorkPanel(rightWrap);
    rightLayout->addWidget(m_workPanel);
    connect(m_workPanel, &CaseWorkPanel::deriveCaseFromBaseRequested, this, &CaseDetailPanel::deriveCaseFromBaseRequested);

    if (m_toolbarWidget && m_workPanel) {
        connect(m_toolbarWidget, &CaseMapToolbarWidget::workbenchToggled, m_workPanel, &CaseWorkPanel::setExpanded);
        connect(m_workPanel, &CaseWorkPanel::workbenchExpandedChanged, this, [this](bool expanded) {
            if (m_toolbarWidget) {
                m_toolbarWidget->setWorkbenchChecked(expanded);
            }
        });
        m_toolbarWidget->setWorkbenchChecked(m_workPanel->isExpanded());
    }

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    m_bodySplitter = splitter;
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(sidebar);
    splitter->addWidget(rightWrap);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({238, 1200});
    m_mainLayout->addWidget(splitter, 1);

    if (m_sidebarCompareButton) {
        connect(m_sidebarCompareButton, &QPushButton::clicked, this, [this]() {
            emit schemeCompareFlowRequested();
        });
    }
}

CaseDetailPanel::~CaseDetailPanel()
{
}

void CaseDetailPanel::setCaseInfo(const CaseItem& caseItem)
{
    m_currentCase = caseItem;
    if (m_caseTitleLabel && m_toolbarWidget
        && m_toolbarWidget->viewMode() == CaseMapToolbarWidget::ViewMode::Standard) {
        m_caseTitleLabel->setText(caseItem.name);
    }
    if (m_workPanel && m_toolbarWidget
        && m_toolbarWidget->viewMode() == CaseMapToolbarWidget::ViewMode::Standard) {
        m_workPanel->setCurrentCase(caseItem);
    }
}

void CaseDetailPanel::setToolbarViewMode(CaseMapToolbarWidget::ViewMode mode)
{
    applySchemeViewMode(mode);
}

void CaseDetailPanel::setCompareCases(const CompareCaseItem& leftCase, const CompareCaseItem& rightCase)
{
    m_leftCompareCase = leftCase;
    m_rightCompareCase = rightCase;

    if (m_caseTitleLabel && m_toolbarWidget
        && m_toolbarWidget->viewMode() == CaseMapToolbarWidget::ViewMode::Compare) {
        m_caseTitleLabel->setText(leftCase.name);
    }
}

void CaseDetailPanel::addDiffMarker(const MapDiffMarker& marker)
{
    m_diffMarkers.append(marker);
}

void CaseDetailPanel::clearDiffMarkers()
{
    m_diffMarkers.clear();
}

CaseMapToolbarWidget::ViewMode CaseDetailPanel::toolbarViewMode() const
{
    if (!m_toolbarWidget) {
        return CaseMapToolbarWidget::ViewMode::Standard;
    }
    return m_toolbarWidget->viewMode();
}

void CaseDetailPanel::refreshStandardAttributeFromMap()
{
    if (!m_stdAttributeWidget) {
        return;
    }
    m_stdAttributeWidget->clearColumnFiltersAndRefresh();
}

void CaseDetailPanel::setBuiltInSchemeSidebarVisible(bool visible)
{
    if (!m_schemeSidebarWidget || !m_bodySplitter) {
        return;
    }

    m_schemeSidebarWidget->setVisible(visible);
    if (visible) {
        m_schemeSidebarWidget->setMinimumWidth(200);
        m_schemeSidebarWidget->setMaximumWidth(QWIDGETSIZE_MAX);
        if (m_returnToSchemeListButton) {
            m_returnToSchemeListButton->setVisible(false);
        }
        m_bodySplitter->setStretchFactor(0, 0);
        m_bodySplitter->setStretchFactor(1, 1);
        m_bodySplitter->setSizes({238, 1200});
    } else {
        m_schemeSidebarWidget->setMinimumWidth(0);
        m_schemeSidebarWidget->setMaximumWidth(0);
        if (m_returnToSchemeListButton) {
            m_returnToSchemeListButton->setVisible(true);
        }
        m_bodySplitter->setStretchFactor(0, 0);
        m_bodySplitter->setStretchFactor(1, 1);
        m_bodySplitter->setSizes({0, 1200});
    }
}

void CaseDetailPanel::setCaseList(const QList<CaseItem>& caseList)
{
    m_caseList = caseList;
    m_caseTreeModel->initData(caseList);

    QList<CompareCaseItem> compareItems;
    compareItems.reserve(caseList.size());
    for (const CaseItem& caseItem : caseList) {
        CompareCaseItem compareItem;
        compareItem.id = caseItem.id;
        compareItem.name = caseItem.name;
        compareItem.projectName = caseItem.typeName;
        compareItem.createTime = caseItem.createTime;
        compareItems.append(compareItem);
    }

    m_availableCompareCases = compareItems;

    if (compareItems.size() >= 2) {
        setCompareCases(compareItems.at(0), compareItems.at(1));
    } else if (compareItems.size() == 1) {
        setCompareCases(compareItems.at(0), compareItems.at(0));
    }

    refreshCompareToolbarOptions();

    if (!caseList.isEmpty() && m_currentCase.id <= 0) {
        setCaseInfo(caseList.first());
    }
}

QStringList CaseDetailPanel::getSelectedLayers() const
{
    return {
        QString::fromUtf8(u8"点图层（道路）"),
        QString::fromUtf8(u8"路段图层（道路）"),
        QString::fromUtf8(u8"小区图层（道路）")
    };
}

bool CaseDetailPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (!event) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress
        && m_mapWidget
        && m_mapWidget->canvas()
        && watched == m_mapWidget->canvas()) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton
            && toolbarViewMode() == CaseMapToolbarWidget::ViewMode::Standard
            && !m_mapWidget->isSelectToolSelecting()) {
            if (tryShowTrafficMicroContextMenu(mouseEvent->globalPos())) {
                return true;
            }
        }
    }

    const bool isActivateEvent =
        event->type() == QEvent::Enter
        || event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseMove;
    if (!isActivateEvent) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == m_leftMapWidget || (m_leftMapWidget && watched == m_leftMapWidget->canvas())) {
        setActiveCompareMap(m_leftMapWidget);
    } else if (watched == m_rightMapWidget || (m_rightMapWidget && watched == m_rightMapWidget->canvas())) {
        setActiveCompareMap(m_rightMapWidget);
    }

    return QWidget::eventFilter(watched, event);
}

QgsVectorLayer* CaseDetailPanel::findLoadedRoadWayLayerWithSelection() const
{
    if (!m_mapWidget || !m_mapWidget->canvas()) {
        return nullptr;
    }

    const QList<QgsMapLayer*> layers = m_mapWidget->canvas()->layers();
    for (QgsMapLayer* layer : layers) {
        auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (!vectorLayer || !vectorLayer->isValid()) {
            continue;
        }
        const QString typeName = CaseMapLayerResolve::vectorLayerWfsTypeName(vectorLayer);
        if (CaseWayLayer::wayLayerValFromTypeName(typeName) != QLatin1String("road_way")) {
            continue;
        }
        if (vectorLayer->selectedFeatureIds().isEmpty()) {
            continue;
        }
        return vectorLayer;
    }
    return nullptr;
}

bool CaseDetailPanel::tryShowTrafficMicroContextMenu(const QPoint& globalPos)
{
    if (!findLoadedRoadWayLayerWithSelection()) {
        return false;
    }

    QMenu menu(this);
    menu.setStyleSheet(QString::fromUtf8(kStdMapContextMenuQss));
    QAction* diagnosisAction = menu.addAction(QString::fromUtf8(u8"中微观诊断"));
    connect(diagnosisAction, &QAction::triggered, this, &CaseDetailPanel::trafficMicroDiagnosisRequested);
    menu.exec(globalPos);
    return true;
}

void CaseDetailPanel::onStdMapContextMenuAboutToShow(QMenu* menu, QgsMapMouseEvent* /*event*/)
{
    if (!menu || !m_mapWidget) {
        return;
    }
    if (toolbarViewMode() != CaseMapToolbarWidget::ViewMode::Standard) {
        return;
    }
    if (m_mapWidget->isSelectToolSelecting()) {
        return;
    }
    if (!findLoadedRoadWayLayerWithSelection()) {
        return;
    }

    menu->addSeparator();
    QAction* diagnosisAction = menu->addAction(QString::fromUtf8(u8"中微观诊断"));
    connect(diagnosisAction, &QAction::triggered, this, &CaseDetailPanel::trafficMicroDiagnosisRequested);
}

void CaseDetailPanel::onToolbarViewModeChanged(CaseMapToolbarWidget::ViewMode mode)
{
    if (mode == CaseMapToolbarWidget::ViewMode::Compare) {
        emit schemeCompareFlowRequested();
        return;
    }
    applySchemeViewMode(CaseMapToolbarWidget::ViewMode::Standard);
}

void CaseDetailPanel::onToolbarToolTriggered(CaseMapToolbarWidget::ToolType toolType)
{
    emit mapToolClicked(toolType);
}

void CaseDetailPanel::onCompareSelectionChanged(const QString& compareName)
{
    const QStringList parts = compareName.split(QStringLiteral("/"));
    if (parts.size() != 2) {
        return;
    }

    const QString leftName = parts.at(0).trimmed();
    const QString rightName = parts.at(1).trimmed();

    for (const CompareCaseItem& item : m_availableCompareCases) {
        if (item.name == leftName) {
            m_leftCompareCase = item;
        }
        if (item.name == rightName) {
            m_rightCompareCase = item;
        }
    }

    emit compareCasesChanged(m_leftCompareCase.id, m_rightCompareCase.id);

    if (m_caseTitleLabel && m_toolbarWidget
        && m_toolbarWidget->viewMode() == CaseMapToolbarWidget::ViewMode::Compare) {
        m_caseTitleLabel->setText(m_leftCompareCase.name);
    }
}

void CaseDetailPanel::syncFloatingToolbarForRenderDataView()
{
    if (!m_toolbarWidget || !m_schemeViewStack) {
        return;
    }

    bool showToolbar = true;
    const int schemePage = m_schemeViewStack->currentIndex();
    if (schemePage == 0 && m_stdContentStack) {
        showToolbar = (m_stdContentStack->currentIndex() == 0);
    } else if (schemePage == 1 && m_cmpContentStack) {
        showToolbar = (m_cmpContentStack->currentIndex() == 0);
    }

    m_toolbarWidget->setVisible(showToolbar);
    relayoutFloatingToolbar();
}

void CaseDetailPanel::onStdRenderClicked()
{
    if (m_stdRenderButton && m_stdDataButton) {
        m_stdRenderButton->setChecked(true);
        m_stdDataButton->setChecked(false);
    }
    if (m_stdContentStack) {
        m_stdContentStack->setCurrentIndex(0);
    }
    syncFloatingToolbarForRenderDataView();
    emit renderClicked();
}

void CaseDetailPanel::switchToStandardDataView()
{
    if (m_stdRenderButton && m_stdDataButton) {
        m_stdRenderButton->setChecked(false);
        m_stdDataButton->setChecked(true);
    }
    if (m_stdContentStack) {
        m_stdContentStack->setCurrentIndex(1);
    }
    syncFloatingToolbarForRenderDataView();
    emit dataClicked();
}

void CaseDetailPanel::onStdDataClicked()
{
    switchToStandardDataView();
    if (m_stdAttributeWidget) {
        m_stdAttributeWidget->refreshFromCurrentLayer();
    }
}

void CaseDetailPanel::onCmpRenderClicked()
{
    if (m_cmpRenderButton && m_cmpDataButton) {
        m_cmpRenderButton->setChecked(true);
        m_cmpDataButton->setChecked(false);
    }
    if (m_cmpContentStack) {
        m_cmpContentStack->setCurrentIndex(0);
    }
    syncFloatingToolbarForRenderDataView();
}

void CaseDetailPanel::onCmpDataClicked()
{
    if (m_cmpRenderButton && m_cmpDataButton) {
        m_cmpRenderButton->setChecked(false);
        m_cmpDataButton->setChecked(true);
    }
    if (m_cmpAttributeWidget) {
        m_cmpAttributeWidget->refreshFromCurrentLayer();
    }
    if (m_cmpContentStack) {
        m_cmpContentStack->setCurrentIndex(1);
    }
    syncFloatingToolbarForRenderDataView();
}

void CaseDetailPanel::onCmpRenderStyleComboChanged(int index)
{
    emit compareRenderStyleChanged(CaseCompareRender::styleFromComboIndex(index));
}

void CaseDetailPanel::resetCompareRenderStyleCombo()
{
    if (!m_cmpRenderStyleCombo) {
        return;
    }
    const bool block = m_cmpRenderStyleCombo->blockSignals(true);
    m_cmpRenderStyleCombo->setCurrentIndex(0);
    m_cmpRenderStyleCombo->blockSignals(block);
}

void CaseDetailPanel::onStdMapStatusChanged(const MapStatus& mapStatus)
{
    if (!m_schemeViewStack || m_schemeViewStack->currentIndex() != 0) {
        return;
    }
    updateStatusLabels(mapStatus);
}

void CaseDetailPanel::onCompareMapStatusChanged(const MapStatus& mapStatus)
{
    TNMapWidget* sourceMap = qobject_cast<TNMapWidget*>(sender());
    if (!sourceMap || sourceMap != m_activeCompareMap) {
        return;
    }
    if (!m_schemeViewStack || m_schemeViewStack->currentIndex() != 1) {
        return;
    }
    updateStatusLabels(mapStatus);
}

void CaseDetailPanel::onLeftMapLayerChanged()
{
    if (m_cmpAttributeWidget && m_cmpContentStack && m_cmpContentStack->currentIndex() == 1) {
        m_cmpAttributeWidget->refreshFromCurrentLayer();
    }
}

QWidget* CaseDetailPanel::createSidebar()
{
    QWidget* sidebar = new QWidget(this);
    sidebar->setMinimumWidth(200);
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setStyleSheet(
        "QWidget {"
        "   background-color: #050f1f;"
        "   border-right: 1px solid rgba(71, 102, 146, 0.35);"
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);

    QPushButton* newCaseButton = new QPushButton(QString::fromUtf8(u8"新建方案"), sidebar);
    newCaseButton->setCursor(Qt::PointingHandCursor);
    newCaseButton->setFixedHeight(42);
    newCaseButton->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 21px;"
        "   background-color: #1f84ff;"
        "   color: white;"
        "   font-size: 15px;"
        "   font-weight: 700;"
        "}"
    );
    connect(newCaseButton, &QPushButton::clicked, this, &CaseDetailPanel::newCaseRequested);
    layout->addWidget(newCaseButton);

    m_sidebarCompareButton = new QPushButton(QString::fromUtf8(u8"方案对比"), sidebar);
    m_sidebarCompareButton->setCursor(Qt::PointingHandCursor);
    m_sidebarCompareButton->setFixedHeight(42);
    m_sidebarCompareButton->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 21px;"
        "   background-color: rgba(255, 255, 255, 0.94);"
        "   color: #202733;"
        "   font-size: 15px;"
        "   font-weight: 700;"
        "}"
    );
    layout->addWidget(m_sidebarCompareButton);

    m_caseTreeView = new CaseTreeView(sidebar);
    m_caseTreeView->setCaseModel(m_caseTreeModel);
    connect(m_caseTreeView, &CaseTreeView::caseOpened, this, &CaseDetailPanel::caseOpened);
    layout->addWidget(m_caseTreeView, 1);

    return sidebar;
}

QWidget* CaseDetailPanel::createMapCanvas()
{
    QWidget* wrapper = new QWidget(this);
    wrapper->setMinimumWidth(280);
    wrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(18, 20, 18, 16);
    layout->setSpacing(12);

    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(16);

    m_caseTitleLabel = new QLabel(QString::fromUtf8(u8"基础方案"), wrapper);
    m_caseTitleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );
    titleRow->addWidget(m_caseTitleLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    titleRow->addStretch(1);

    m_returnToSchemeListButton = new QPushButton(QString::fromUtf8(u8"返回方案列表"), wrapper);
    m_returnToSchemeListButton->setCursor(Qt::PointingHandCursor);
    m_returnToSchemeListButton->setVisible(false);
    m_returnToSchemeListButton->setMinimumHeight(36);
    m_returnToSchemeListButton->setStyleSheet(
        "QPushButton {"
        "   border: 1px solid rgba(255,255,255,0.28);"
        "   border-radius: 10px;"
        "   padding: 0 14px;"
        "   background-color: transparent;"
        "   color: rgba(255,255,255,0.92);"
        "   font-size: 14px;"
        "   font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(42,163,255,0.18);"
        "}"
    );
    connect(m_returnToSchemeListButton, &QPushButton::clicked, this, &CaseDetailPanel::schemeListReturnRequested);
    titleRow->addWidget(m_returnToSchemeListButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addLayout(titleRow);

    auto* host = new SchemeToolbarHostWidget(wrapper);
    m_toolbarHost = host;
    QVBoxLayout* hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    m_schemeViewStack = new QStackedWidget(host);
    m_schemeViewStack->addWidget(createStandardSchemePage());
    m_schemeViewStack->addWidget(createCompareSchemePage());
    hostLayout->addWidget(m_schemeViewStack, 1);

    m_toolbarWidget = new CaseMapToolbarWidget(CaseMapToolbarWidget::HostContext::SchemeDetail, host);
    m_toolbarWidget->setSchemeModeSwitcherVisible(true);
    m_toolbarWidget->setViewMode(CaseMapToolbarWidget::ViewMode::Standard);
    m_toolbarWidget->setCompareSelectorVisible(false);
    host->setFloatingToolbar(m_toolbarWidget);

    connect(m_toolbarWidget, &CaseMapToolbarWidget::viewModeChanged, this, &CaseDetailPanel::onToolbarViewModeChanged);
    connect(m_toolbarWidget, &CaseMapToolbarWidget::toolTriggered, this, &CaseDetailPanel::onToolbarToolTriggered);
    connect(m_toolbarWidget,
            &CaseMapToolbarWidget::compareSelectionChanged,
            this,
            &CaseDetailPanel::onCompareSelectionChanged);

    if (m_mapWidget && m_toolbarWidget) {
        connect(m_mapWidget, &TNMapWidget::editInteractionCleared, this, [this]() {
            if (m_toolbarWidget) {
                m_toolbarWidget->setMutexTool(CaseMapToolbarWidget::ToolType::Browse);
            }
        });
    }

    layout->addWidget(host, 1);

    if (m_mapWidget) {
        updateStatusLabels(m_mapWidget->currentMapStatus());
    }

    setActiveCompareMap(m_leftMapWidget);
    return wrapper;
}

QWidget* CaseDetailPanel::createStandardSchemePage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* mapShell = new QFrame(page);
    mapShell->setObjectName(QStringLiteral("caseStdMapShell"));
    mapShell->setAttribute(Qt::WA_StyledBackground, false);
    mapShell->setStyleSheet(QStringLiteral(
        "QFrame#caseStdMapShell { background: transparent; border: none; }"));
    applyDesignMapShellShadow(mapShell);

    auto* shellLayout = new QVBoxLayout(mapShell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    QFrame* mapViewport = new QFrame(mapShell);
    mapViewport->setObjectName(QStringLiteral("caseStdMapViewport"));
    mapViewport->setAttribute(Qt::WA_StyledBackground, true);
    const QString rv = QString::number(kDesignMapViewportRadius);
    mapViewport->setStyleSheet(QString(
        "QFrame#caseStdMapViewport {"
        "   background-color: #ffffff;"
        "   border-top-left-radius: %1px;"
        "   border-top-right-radius: %1px;"
        "   border: none;"
        "}"
    ).arg(rv));

    QVBoxLayout* vpLayout = new QVBoxLayout(mapViewport);
    vpLayout->setContentsMargins(0, 0, 0, 0);
    vpLayout->setSpacing(0);

    m_stdContentStack = new QStackedWidget(mapViewport);
    m_stdContentStack->addWidget(createStdRenderPage());
    m_stdContentStack->addWidget(createStdDataPage());
    vpLayout->addWidget(m_stdContentStack, 1);

    shellLayout->addWidget(mapViewport, 1);
    shellLayout->addWidget(createStdMapFooter());

    outer->addWidget(mapShell, 1);
    return page;
}

QWidget* CaseDetailPanel::createCompareSchemePage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* mapShell = new QFrame(page);
    mapShell->setObjectName(QStringLiteral("caseCmpMapShell"));
    mapShell->setStyleSheet(QStringLiteral(
        "QFrame#caseCmpMapShell { background: transparent; border: none; }"));
    applyDesignMapShellShadow(mapShell);

    auto* shellLayout = new QVBoxLayout(mapShell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    QFrame* mapViewport = new QFrame(mapShell);
    mapViewport->setObjectName(QStringLiteral("caseCmpMapViewport"));
    mapViewport->setAttribute(Qt::WA_StyledBackground, true);
    const QString rv = QString::number(kDesignMapViewportRadius);
    mapViewport->setStyleSheet(QString(
        "QFrame#caseCmpMapViewport {"
        "   background-color: #ffffff;"
        "   border-top-left-radius: %1px;"
        "   border-top-right-radius: %1px;"
        "   border: none;"
        "}"
    ).arg(rv));

    QVBoxLayout* vpLayout = new QVBoxLayout(mapViewport);
    vpLayout->setContentsMargins(0, 0, 0, 0);
    vpLayout->setSpacing(0);

    m_cmpContentStack = new QStackedWidget(mapViewport);
    m_cmpContentStack->addWidget(createCmpRenderPage());
    m_cmpContentStack->addWidget(createCmpDataPage());
    vpLayout->addWidget(m_cmpContentStack, 1);

    shellLayout->addWidget(mapViewport, 1);
    shellLayout->addWidget(createCmpMapFooter());

    outer->addWidget(mapShell, 1);
    return page;
}

QWidget* CaseDetailPanel::createStdRenderPage()
{
    QWidget* renderPage = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(renderPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_mapWidget = new TNMapWidget(renderPage);
    CaseMapUtils::setupCaseSceneMap(m_mapWidget);
    layout->addWidget(m_mapWidget, 1);

    if (m_mapWidget->canvas()) {
        m_mapWidget->canvas()->installEventFilter(this);
        connect(m_mapWidget->canvas(), &QgsMapCanvas::contextMenuAboutToShow,
                this, &CaseDetailPanel::onStdMapContextMenuAboutToShow);
    }

    CaseMapLegendWidget* legendWidget = new CaseMapLegendWidget(m_mapWidget);
    legendWidget->setFixedWidth(185);
    m_stdLegendWidget = legendWidget;
    m_mapWidget->setBottomLeftOverlayWidget(legendWidget, 10);
    legendWidget->hide();

    connect(m_mapWidget, &TNMapWidget::mapStatusChanged, this, &CaseDetailPanel::onStdMapStatusChanged);

    return renderPage;
}

QWidget* CaseDetailPanel::createStdDataPage()
{
    QWidget* dataPage = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(dataPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stdAttributeWidget = new CaseMapAttributeWidget(dataPage);
    m_stdAttributeWidget->bindMap(m_mapWidget);
    layout->addWidget(m_stdAttributeWidget, 1);
    return dataPage;
}

QWidget* CaseDetailPanel::createStdMapFooter()
{
    QWidget* footer = new QWidget(this);
    footer->setAttribute(Qt::WA_StyledBackground, true);
    const QString br = QString::number(kDesignMapViewportRadius);
    footer->setStyleSheet(QString(
        "QWidget {"
        "   background-color: %1;"
        "   border-bottom-left-radius: %2px;"
        "   border-bottom-right-radius: %2px;"
        "   border-top: 1px solid rgba(255,255,255,0.14);"
        "   border-left: none;"
        "   border-right: none;"
        "   border-bottom: none;"
        "}"
    ).arg(QLatin1String(kDesignMapFooterBg), br));

    QHBoxLayout* layout = new QHBoxLayout(footer);
    layout->setContentsMargins(18, 11, 18, 13);
    layout->setSpacing(0);

    QFrame* segmentHost = new QFrame(footer);
    segmentHost->setStyleSheet(
        "QFrame {"
        "   background-color: rgba(255,255,255,0.06);"
        "   border-radius: 8px;"
        "   border: none;"
        "}"
    );
    QHBoxLayout* segmentLayout = new QHBoxLayout(segmentHost);
    segmentLayout->setContentsMargins(5, 4, 5, 4);
    segmentLayout->setSpacing(4);

    m_stdRenderButton = CaseMapFooter::createSegmentButton(QString::fromUtf8(u8"图层渲染"), true, segmentHost);
    m_stdDataButton = CaseMapFooter::createSegmentButton(QString::fromUtf8(u8"图层数据"), false, segmentHost);
    connect(m_stdRenderButton, &QPushButton::clicked, this, &CaseDetailPanel::onStdRenderClicked);
    connect(m_stdDataButton, &QPushButton::clicked, this, &CaseDetailPanel::onStdDataClicked);

    QButtonGroup* stdButtonGroup = new QButtonGroup(footer);
    stdButtonGroup->setExclusive(true);
    stdButtonGroup->addButton(m_stdRenderButton);
    stdButtonGroup->addButton(m_stdDataButton);

    segmentLayout->addWidget(m_stdRenderButton);
    segmentLayout->addWidget(m_stdDataButton);
    layout->addWidget(segmentHost, 0, Qt::AlignLeft | Qt::AlignVCenter);
    layout->addStretch(1);

    m_stdFooterCrsLabel = new QLabel(QString::fromUtf8(u8"坐标系：--"), footer);
    m_stdFooterCoordLabel = new QLabel(QString::fromUtf8(u8"坐标：--"), footer);
    m_stdFooterScaleLabel = new QLabel(QString::fromUtf8(u8"比例尺：--"), footer);
    m_stdFooterCrsLabel->setStyleSheet(CaseMapFooter::darkMapFooterLabelStyle());
    m_stdFooterCoordLabel->setStyleSheet(CaseMapFooter::darkMapFooterLabelStyle());
    m_stdFooterScaleLabel->setStyleSheet(CaseMapFooter::darkMapFooterLabelStyle());
    m_stdFooterCrsLabel->setMinimumWidth(120);
    m_stdFooterCoordLabel->setMinimumWidth(180);
    m_stdFooterScaleLabel->setMinimumWidth(100);
    layout->addWidget(m_stdFooterCrsLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(32);
    layout->addWidget(m_stdFooterCoordLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(32);
    layout->addWidget(m_stdFooterScaleLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    return footer;
}

QWidget* CaseDetailPanel::createCmpRenderPage()
{
    QWidget* renderPage = new QWidget(this);
    QVBoxLayout* pageLayout = new QVBoxLayout(renderPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto createMapPane = [this](TNMapWidget** outMapWidget, QWidget* parent) -> QWidget* {
        QWidget* pane = new QWidget(parent);
        pane->setAttribute(Qt::WA_StyledBackground, true);
        pane->setStyleSheet(QStringLiteral("QWidget { background-color: #ffffff; border: none; }"));
        QVBoxLayout* paneLayout = new QVBoxLayout(pane);
        paneLayout->setContentsMargins(0, 0, 0, 0);
        paneLayout->setSpacing(0);

        *outMapWidget = new TNMapWidget(pane);
        CaseMapUtils::setupCaseSceneMap(*outMapWidget);
        paneLayout->addWidget(*outMapWidget, 1);
        return pane;
    };

    QSplitter* mapSplitter = new QSplitter(Qt::Horizontal, renderPage);
    mapSplitter->setChildrenCollapsible(false);
    mapSplitter->setHandleWidth(8);
    mapSplitter->setStyleSheet(
        "QSplitter::handle {"
        "   background: rgba(71, 102, 146, 0.45);"
        "   margin: 0;"
        "}"
        "QSplitter::handle:hover {"
        "   background: rgba(42, 163, 255, 0.65);"
        "}"
    );
    mapSplitter->addWidget(createMapPane(&m_leftMapWidget, mapSplitter));
    mapSplitter->addWidget(createMapPane(&m_rightMapWidget, mapSplitter));
    mapSplitter->setStretchFactor(0, 1);
    mapSplitter->setStretchFactor(1, 1);
    pageLayout->addWidget(mapSplitter, 1);

    if (m_leftMapWidget) {
        m_leftCompareLegendWidget = new CaseMapLegendWidget(m_leftMapWidget);
        m_leftCompareLegendWidget->setFixedWidth(185);
        m_leftMapWidget->setBottomLeftOverlayWidget(m_leftCompareLegendWidget, 10);
        m_leftCompareLegendWidget->hide();

        connect(m_leftMapWidget, &TNMapWidget::mapStatusChanged, this, &CaseDetailPanel::onCompareMapStatusChanged);
        connect(m_leftMapWidget, &TNMapWidget::currentLayerChanged, this, &CaseDetailPanel::onLeftMapLayerChanged);
        m_leftMapWidget->installEventFilter(this);
        m_leftMapWidget->canvas()->installEventFilter(this);
        connect(m_leftMapWidget->canvas(), &QgsMapCanvas::extentsChanged, this, [this]() {
            syncCompareMapViewportFrom(m_leftMapWidget);
        });
    }

    if (m_rightMapWidget) {
        connect(m_rightMapWidget, &TNMapWidget::mapStatusChanged, this, &CaseDetailPanel::onCompareMapStatusChanged);
        m_rightMapWidget->installEventFilter(this);
        m_rightMapWidget->canvas()->installEventFilter(this);
        connect(m_rightMapWidget->canvas(), &QgsMapCanvas::extentsChanged, this, [this]() {
            syncCompareMapViewportFrom(m_rightMapWidget);
        });
    }

    return renderPage;
}

QWidget* CaseDetailPanel::createCmpDataPage()
{
    QWidget* dataPage = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(dataPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_cmpAttributeWidget = new CaseMapAttributeWidget(dataPage);
    m_cmpAttributeWidget->bindMap(m_leftMapWidget);
    layout->addWidget(m_cmpAttributeWidget, 1);
    return dataPage;
}

QWidget* CaseDetailPanel::createCmpMapFooter()
{
    QWidget* footer = new QWidget(this);
    footer->setAttribute(Qt::WA_StyledBackground, true);
    const QString br = QString::number(kDesignMapViewportRadius);
    footer->setStyleSheet(QString(
        "QWidget {"
        "   background-color: %1;"
        "   border-bottom-left-radius: %2px;"
        "   border-bottom-right-radius: %2px;"
        "   border-top: 1px solid rgba(255,255,255,0.14);"
        "   border-left: none;"
        "   border-right: none;"
        "   border-bottom: none;"
        "}"
    ).arg(QLatin1String(kDesignMapFooterBg), br));

    QHBoxLayout* layout = new QHBoxLayout(footer);
    layout->setContentsMargins(18, 11, 18, 13);
    layout->setSpacing(0);

    QFrame* segmentHost = new QFrame(footer);
    segmentHost->setStyleSheet(
        "QFrame {"
        "   background-color: rgba(255,255,255,0.06);"
        "   border-radius: 8px;"
        "   border: none;"
        "}"
    );
    QHBoxLayout* segmentLayout = new QHBoxLayout(segmentHost);
    segmentLayout->setContentsMargins(5, 4, 5, 4);
    segmentLayout->setSpacing(4);

    m_cmpRenderButton = CaseMapFooter::createSegmentButton(QString::fromUtf8(u8"图层渲染"), true, segmentHost);
    m_cmpDataButton = CaseMapFooter::createSegmentButton(QString::fromUtf8(u8"图层数据"), false, segmentHost);
    connect(m_cmpRenderButton, &QPushButton::clicked, this, &CaseDetailPanel::onCmpRenderClicked);
    connect(m_cmpDataButton, &QPushButton::clicked, this, &CaseDetailPanel::onCmpDataClicked);

    QButtonGroup* cmpButtonGroup = new QButtonGroup(footer);
    cmpButtonGroup->setExclusive(true);
    cmpButtonGroup->addButton(m_cmpRenderButton);
    cmpButtonGroup->addButton(m_cmpDataButton);

    segmentLayout->addWidget(m_cmpRenderButton);
    segmentLayout->addWidget(m_cmpDataButton);
    layout->addWidget(segmentHost, 0, Qt::AlignLeft | Qt::AlignVCenter);
    layout->addStretch(1);

    m_cmpFooterCrsLabel = new QLabel(QString::fromUtf8(u8"坐标系：--"), footer);
    m_cmpFooterCoordLabel = new QLabel(QString::fromUtf8(u8"坐标：--"), footer);
    m_cmpFooterScaleLabel = new QLabel(QString::fromUtf8(u8"比例尺：--"), footer);
    m_cmpFooterCrsLabel->setStyleSheet(CaseMapFooter::darkMapFooterLabelStyle());
    m_cmpFooterCoordLabel->setStyleSheet(CaseMapFooter::darkMapFooterLabelStyle());
    m_cmpFooterScaleLabel->setStyleSheet(CaseMapFooter::darkMapFooterLabelStyle());
    m_cmpFooterCrsLabel->setMinimumWidth(120);
    m_cmpFooterCoordLabel->setMinimumWidth(180);
    m_cmpFooterScaleLabel->setMinimumWidth(100);
    layout->addWidget(m_cmpFooterCrsLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(32);
    layout->addWidget(m_cmpFooterCoordLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(32);
    layout->addWidget(m_cmpFooterScaleLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    layout->addSpacing(20);

    m_cmpRenderStyleCombo = new QComboBox(footer);
    m_cmpRenderStyleCombo->setCursor(Qt::PointingHandCursor);
    m_cmpRenderStyleCombo->setMinimumWidth(120);
    m_cmpRenderStyleCombo->addItem(QString::fromUtf8(u8"无样式"));
    m_cmpRenderStyleCombo->addItem(QString::fromUtf8(u8"饱和度"));
    m_cmpRenderStyleCombo->addItem(QString::fromUtf8(u8"流量分布"));
    m_cmpRenderStyleCombo->setCurrentIndex(0);
    Phase2Ui::setupThemedCombo(m_cmpRenderStyleCombo, Phase2Ui::InputFieldTheme::DarkToolbar);
    connect(m_cmpRenderStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CaseDetailPanel::onCmpRenderStyleComboChanged);
    layout->addWidget(m_cmpRenderStyleCombo, 0, Qt::AlignRight | Qt::AlignVCenter);

    return footer;
}

void CaseDetailPanel::applySchemeViewMode(CaseMapToolbarWidget::ViewMode mode)
{
    if (!m_toolbarWidget || !m_schemeViewStack) {
        return;
    }

    const bool block = m_toolbarWidget->blockSignals(true);
    m_toolbarWidget->setViewMode(mode);
    m_toolbarWidget->setCompareSelectorVisible(mode == CaseMapToolbarWidget::ViewMode::Compare);
    m_toolbarWidget->blockSignals(block);

    m_schemeViewStack->setCurrentIndex(mode == CaseMapToolbarWidget::ViewMode::Compare ? 1 : 0);

    if (m_toolbarWidget && m_toolbarWidget->hostContext() == CaseMapToolbarWidget::HostContext::SchemeDetail) {
        m_toolbarWidget->setToolButtonsEnabled(mode != CaseMapToolbarWidget::ViewMode::Compare);
    }

    if (mode == CaseMapToolbarWidget::ViewMode::Compare) {
        resetCompareRenderStyleCombo();
        if (m_caseTitleLabel) {
            m_caseTitleLabel->setText(m_leftCompareCase.name);
        }
        setActiveCompareMap(m_leftMapWidget);
        if (m_activeCompareMap) {
            updateStatusLabels(m_activeCompareMap->currentMapStatus());
        }
        refreshCompareToolbarOptions();
        syncCompareMapViewportFrom(m_leftMapWidget);
    } else {
        if (m_caseTitleLabel) {
            m_caseTitleLabel->setText(m_currentCase.name);
        }
        if (m_workPanel) {
            m_workPanel->setCurrentCase(m_currentCase);
        }
        if (m_mapWidget) {
            updateStatusLabels(m_mapWidget->currentMapStatus());
        }
    }

    syncFloatingToolbarForRenderDataView();
}

void CaseDetailPanel::relayoutFloatingToolbar()
{
    auto* host = static_cast<SchemeToolbarHostWidget*>(m_toolbarHost);
    host->relayoutFloatingToolbar();
}

QString CaseDetailPanel::currentCompareDisplayName() const
{
    if (m_leftCompareCase.id > 0 && m_rightCompareCase.id > 0) {
        return QString::fromUtf8(u8"%1 / %2").arg(m_leftCompareCase.name, m_rightCompareCase.name);
    }
    return QString();
}

CaseItem CaseDetailPanel::leftCompareWorkCase() const
{
    CaseItem workCase;
    bool found = false;
    for (const CaseItem& c : m_caseList) {
        if (c.id == m_leftCompareCase.id) {
            workCase = c;
            found = true;
            break;
        }
    }
    if (!found) {
        workCase.id = m_leftCompareCase.id;
        workCase.createTime = m_leftCompareCase.createTime;
        workCase.projectId = m_currentCase.projectId;
    }
    workCase.name = currentCompareDisplayName();
    return workCase;
}

void CaseDetailPanel::setActiveCompareMap(TNMapWidget* mapWidget)
{
    if (!mapWidget) {
        return;
    }

    if (m_activeCompareMap == mapWidget) {
        return;
    }

    m_activeCompareMap = mapWidget;
    if (m_schemeViewStack && m_schemeViewStack->currentIndex() == 1) {
        updateStatusLabels(m_activeCompareMap->currentMapStatus());
    }
}

void CaseDetailPanel::syncCompareMapViewportFrom(TNMapWidget* sourceMap)
{
    if (m_compareViewportSyncGuard || !sourceMap) {
        return;
    }
    if (!m_schemeViewStack || m_schemeViewStack->currentIndex() != 1) {
        return;
    }
    if (!m_leftMapWidget || !m_rightMapWidget) {
        return;
    }

    TNMapWidget* targetMap = nullptr;
    if (sourceMap == m_leftMapWidget) {
        targetMap = m_rightMapWidget;
    } else if (sourceMap == m_rightMapWidget) {
        targetMap = m_leftMapWidget;
    } else {
        return;
    }

    QgsMapCanvas* srcCanvas = sourceMap->canvas();
    QgsMapCanvas* dstCanvas = targetMap->canvas();
    if (!srcCanvas || !dstCanvas) {
        return;
    }

    const double scale = srcCanvas->scale();
    if (scale <= 0.0) {
        return;
    }

    const QgsPointXY center = srcCanvas->center();
    m_compareViewportSyncGuard = true;
    dstCanvas->setCenter(center);
    dstCanvas->zoomScale(scale);
    m_compareViewportSyncGuard = false;
}

void CaseDetailPanel::updateStatusLabels(const MapStatus& mapStatus)
{
    const QString crsText =
        mapStatus.crsText.isEmpty() ? QString::fromUtf8(u8"坐标系：--") : mapStatus.crsText;
    const QString coordText =
        mapStatus.coordText.isEmpty() ? QString::fromUtf8(u8"坐标：--") : mapStatus.coordText;
    const QString scaleText =
        mapStatus.scaleText.isEmpty() ? QString::fromUtf8(u8"比例尺：--") : mapStatus.scaleText;

    if (m_stdFooterCrsLabel) {
        m_stdFooterCrsLabel->setText(crsText);
    }
    if (m_stdFooterCoordLabel) {
        m_stdFooterCoordLabel->setText(coordText);
    }
    if (m_stdFooterScaleLabel) {
        m_stdFooterScaleLabel->setText(scaleText);
    }
    if (m_cmpFooterCrsLabel) {
        m_cmpFooterCrsLabel->setText(crsText);
    }
    if (m_cmpFooterCoordLabel) {
        m_cmpFooterCoordLabel->setText(coordText);
    }
    if (m_cmpFooterScaleLabel) {
        m_cmpFooterScaleLabel->setText(scaleText);
    }
}

void CaseDetailPanel::refreshCompareToolbarOptions()
{
    if (!m_toolbarWidget) {
        return;
    }

    QStringList options;
    const QList<CompareCaseItem>& cases = m_availableCompareCases;
    for (int i = 0; i < cases.size(); ++i) {
        for (int j = i + 1; j < cases.size(); ++j) {
            options.append(QString::fromUtf8(u8"%1 / %2").arg(cases.at(i).name, cases.at(j).name));
        }
    }

    m_toolbarWidget->setCompareOptions(options);
    m_toolbarWidget->setCurrentCompareText(currentCompareDisplayName());
}
