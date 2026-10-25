#include "appsession.h"
#include "offlineapi.h"
#include "offlineimportdialog.h"
/*****************************************************************//**
 * \file   casepanel.cpp
 * \brief  方案工具栏实现
 * \details 实现项目方案栏、基础数据栏、标准视图与双屏对比预览
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "casepanel.h"

#include "../casectrls/basedatatableview.h"
#include "../casectrls/casemaplayerresolve.h"
#include "../casectrls/casemapattributewidget.h"
#include "../casectrls/casemapfooter.h"
#include "../casectrls/casemaplegendwidget.h"
#include "../casectrls/casemaputils.h"
#include "../casectrls/casewaylayerhelper.h"
#include "../casectrls/casewayfeatureaddhelper.h"
#include "../casectrls/casecomparerenderhelper.h"
#include "../casectrls/casewaylayerstyleapply.h"
#include "../casectrls/casedetailpanel.h"
#include "../casectrls/casecomparisondialog.h"
#include "../casectrls/casewfsmapcontroller.h"
#include "../casectrls/casemaptoolbarwidget.h"
#include "../casectrls/casemaptoolcontroller.h"
#include "../casectrls/caseotherodmaphelper.h"
#include "../casectrls/casemapexportdialog.h"
#include "../casectrls/casemenuhelper.h"
#include "../casectrls/casepopgenerationdialog.h"
#include "../casectrls/casepopzonalstathelper.h"
#include "../casectrls/casecommunitygenerationdialog.h"
#include "../casectrls/caseodtypedialog.h"
#include "../casectrls/casetreemodel.h"
#include "../casectrls/casetreeview.h"
#include "../casectrls/casetype.h"
#include "../casectrls/newcasedialog.h"
#include "../casectrls/renamecasedialog.h"
#include "../httpapis/httpapis.h"
#include "httpapijson.h"
#include "../tnmapwidget/tnmapwidget.h"
#include "../tnmapwidget/tngiseditstackmanager.h"

#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include "qgsgraduatedsymbolrenderer.h"
#include "../publicctrls/messagedialog.h"
#include "../tnmapwidget/layerstylefactory.h"
#include "../utils/filedownloader.h"
#include "loadingdialog.h"
#include "caseworkpanel.h"
#include "databrowserdialog.h"
#include "odestimationdialog.h"
#include "uiglobalstate.h"
#include "webview2dialog.h"

#include <QDebug>
#include <QPointer>
#include <QTimer>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QEventLoop>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFrame>
#include <functional>
#include <memory>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMap>
#include <QModelIndex>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>
#include <QVBoxLayout>

namespace {
constexpr qint64 kToolDoubleClickThrottleMs = 300;
constexpr int kBaseMapFloatingToolbarTopMargin = 14;

const QString kRoadCommunityLayerVal = QStringLiteral("road_community");
const QString kRoadWayLayerVal = QStringLiteral("road_way");
const QString kRoadLayerGroupValue = QStringLiteral("road");
const QString kOtherOdLayerVal = QStringLiteral("other_od");

QString wayLayerValForModule(CaseWorkPanel::AnalysisModule module)
{
    switch (module) {
        case CaseWorkPanel::AnalysisModule::PublicTransport:
            return QStringLiteral("bus_way");
        case CaseWorkPanel::AnalysisModule::SlowTraffic:
            return QStringLiteral("slow_road_way");
        case CaseWorkPanel::AnalysisModule::MotorVehicle:
        default:
            return QStringLiteral("road_way");
    }
}

QString applicationBaseDir()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    if (dir.dirName() == QStringLiteral("depends")) {
        dir.cdUp();
        appDir = dir.absolutePath();
    }
    return appDir;
}

void requestCaseComparisonReport(QWidget* parent, int projectId, const QList<int>& caseIds,
                                 const QString& checkExtent, const QString& type)
{
    if (!parent || projectId <= 0 || caseIds.size() < 2) {
        return;
    }

    const QString extentText = checkExtent.trimmed();
    if (extentText.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"对比地理范围无效。"));
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    auto* loading = new LoadingDialog(QString::fromUtf8(u8"方案对比加载中…"), nullptr);
    loading->showNonBlocking();

    QStringList caseIdStrings;
    caseIdStrings.reserve(caseIds.size());
    for (int caseId : caseIds) {
        caseIdStrings.append(QString::number(caseId));
    }
    const QString caseIdsText = caseIdStrings.join(QStringLiteral(","));

    QPointer<QWidget> parentGuard(parent);
    HttpApis::caseComparisonView(token,
                                 projectId,
                                 caseIdsText,
                                 extentText,
                                 type,
                                 [parentGuard, loading](bool suc,
                                                        const QSharedPointer<CaseReportViewJson>& res,
                                                        const QString& errMsg) {
                                     if (loading) {
                                         loading->closeDialog();
                                         loading->deleteLater();
                                     }
                                     if (!parentGuard) {
                                         return;
                                     }
                                     if (!suc || res.isNull()) {
                                         TnUi::showWarning(QString::fromUtf8(u8"方案对比"),
                                             errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"加载失败")
                                                                        : errMsg.trimmed());
                                         return;
                                     }

                                     const QString html = res->j_content().trimmed();
                                     if (html.isEmpty()) {
                                         TnUi::showWarning(QString::fromUtf8(u8"方案对比"),
                                             QString::fromUtf8(u8"接口未返回 HTML 内容"));
                                         return;
                                     }

                                     openHtmlReportWithFallback(parentGuard->window(),
                                                                QString::fromUtf8(u8"方案对比"),
                                                                html,
                                                                QStringLiteral("case_comparison.html"));
                                 });
}

class BaseMapToolbarHostWidget : public QWidget {
public:
    explicit BaseMapToolbarHostWidget(QWidget* parent = nullptr)
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
        m_toolbar->move(x, kBaseMapFloatingToolbarTopMargin);
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

/** 项目方案页外层背景（与主面板一致） */
constexpr char kSchemePageOuterBg[] = "#030d20";
/** 列表圆角容器背景（与左侧栏 #050f1f 对齐设计稿层次） */
constexpr char kSchemeListShellBg[] = "#050f1f";
/** 与 CaseDetailPanel 设计稿统一的地图底栏、圆角 */
constexpr char kDesignBaseMapFooterBg[] = "#141c2c";
constexpr int kDesignBaseMapViewportRadius = 10;

void applyBaseDataMapShellShadow(QWidget* shellWidget)
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

void bindEditStackToolbar(TNMapWidget* map, CaseMapToolbarWidget* toolbar)
{
    if (!map || !toolbar) {
        return;
    }
    const auto sync = [map, toolbar]() {
        TNGisEditStackManager* stack = map->editStackManager();
        toolbar->setSaveEditsEnabled(stack && stack->hasUnsavedChanges());
        toolbar->setUndoEditsEnabled(stack && stack->canUndo());
        QString tip = QString::fromUtf8(u8"撤回修改");
        if (stack) {
            const QString layerName = stack->peekUndoLayerName();
            if (!layerName.isEmpty()) {
                tip = QString::fromUtf8(u8"撤回修改：%1").arg(layerName);
            }
        }
        toolbar->setUndoEditsToolTip(tip);
    };
    if (TNGisEditStackManager* stack = map->editStackManager()) {
        QObject::connect(stack, &TNGisEditStackManager::editStackChanged, toolbar, sync);
    }
    sync();
}

void relayoutBaseMapToolbarHost(QWidget* host)
{
    if (!host) {
        return;
    }
    static_cast<BaseMapToolbarHostWidget*>(host)->relayoutFloatingToolbar();
}

QPushButton* createGhostButton(const QString& text, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(132, 32);
    button->setIcon(QIcon(QStringLiteral(":/icons/icons/compare_view@2x.png")));
    button->setIconSize(QSize(14, 14));
    button->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 16px;"
        "   background-color: rgba(255,255,255,0.94);"
        "   color: #202733;"
        "   font-size: 13px;"
        "   font-weight: 700;"
        "   padding: 0 12px;"
        "}"
    );
    return button;
}

QString normalizeFixedType(const QString& value)
{
    const QString lower = value.trimmed().toLower();
    if (lower.contains("point") || lower.contains(QString::fromUtf8(u8"点"))) {
        return QStringLiteral("point");
    }
    if (lower.contains("way") || lower.contains(QString::fromUtf8(u8"路段"))) {
        return QStringLiteral("way");
    }
    if (lower.contains("community") || lower.contains(QString::fromUtf8(u8"小区"))) {
        return QStringLiteral("community");
    }
    return QString();
}

QString roadLayerDisplayName(const QString& key)
{
    if (key == QStringLiteral("point")) {
        return QString::fromUtf8(u8"点图层（道路）");
    }
    if (key == QStringLiteral("way")) {
        return QString::fromUtf8(u8"路段图层（道路）");
    }
    if (key == QStringLiteral("community")) {
        return QString::fromUtf8(u8"小区图层（道路）");
    }
    return QString();
}

QString busLayerDisplayName(const QString& key)
{
    if (key == QStringLiteral("point")) {
        return QString::fromUtf8(u8"点图层（公交）");
    }
    if (key == QStringLiteral("way")) {
        return QString::fromUtf8(u8"路段图层（公交）");
    }
    if (key == QStringLiteral("community")) {
        return QString::fromUtf8(u8"小区图层（公交）");
    }
    return QString();
}

QString otherLayerDisplayName(const QString& key)
{
    static const QMap<QString, QString> mappings = {
        {QStringLiteral("od"), QString::fromUtf8(u8"OD出行矩阵")},
        {QStringLiteral("taz_socioeconomic"), QString::fromUtf8(u8"交通分区社会经济属性")},
        {QStringLiteral("trip_production_attraction"), QString::fromUtf8(u8"出行发生-吸引量")},
        {QStringLiteral("observation"), QString::fromUtf8(u8"交通观测数据")},
        {QStringLiteral("trip_distribution_hist"), QString::fromUtf8(u8"出行分布历史数据")}
    };
    return mappings.value(key.trimmed().toLower());
}

QString caseDisplayName(const QString& rawCaseName)
{
    // 方案显示名不再拼接类型前缀，直接使用后端返回的名称
    const QString safeRawName = rawCaseName.trimmed().isEmpty() ? QString::fromUtf8(u8"未命名方案") : rawCaseName.trimmed();
    return safeRawName;
}

} // namespace

CaseInfoWidget::CaseInfoWidget(QWidget* parent)
    : QFrame(parent)
    , m_nameLabel(new QLabel(this))
    , m_statsLabel(new QLabel(this))
    , m_timeLabel(new QLabel(this))
    , m_viewButton(new QPushButton(u8"查看", this))
    , m_deleteButton(new QPushButton(u8"删除", this))
    , m_caseId(-1)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "CaseInfoWidget {"
        "   background-color: #0c2345;"
        "   border-radius: 16px;"
        "   border: 1px solid rgba(64, 111, 179, 0.36);"
        "}"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(12);

    m_nameLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );
    mainLayout->addWidget(m_nameLabel);

    m_statsLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(255,255,255,0.88);"
        "   font-size: 14px;"
        "}"
    );
    mainLayout->addWidget(m_statsLabel);

    QHBoxLayout* footerLayout = new QHBoxLayout();
    footerLayout->setSpacing(12);

    m_timeLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(180,196,222,0.62);"
        "   font-size: 14px;"
        "}"
    );
    footerLayout->addWidget(m_timeLabel);
    footerLayout->addStretch();

    const QString buttonStyle =
        "QPushButton {"
        "   min-width: 80px;"
        "   min-height: 32px;"
        "   border: none;"
        "   border-radius: 16px;"
        "   background-color: #1f84ff;"
        "   color: white;"
        "   font-size: 14px;"
        "   font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3193ff;"
        "}";

    m_viewButton->setCursor(Qt::PointingHandCursor);
    m_viewButton->setIcon(QIcon(QStringLiteral(":/icons/icons/view.png")));
    m_viewButton->setIconSize(QSize(14, 14));
    m_viewButton->setStyleSheet(buttonStyle);
    connect(m_viewButton, &QPushButton::clicked, this, [this]() {
        emit viewButtonClicked(m_caseId);
    });
    footerLayout->addWidget(m_viewButton);

    m_deleteButton->setCursor(Qt::PointingHandCursor);
    m_deleteButton->setIcon(QIcon(QStringLiteral(":/icons/icons/delete.png")));
    m_deleteButton->setIconSize(QSize(14, 14));
    m_deleteButton->setStyleSheet(
        "QPushButton {"
        "   min-width: 80px;"
        "   min-height: 32px;"
        "   border: none;"
        "   border-radius: 16px;"
        "   background-color: rgba(31,132,255,0.18);"
        "   color: white;"
        "   font-size: 14px;"
        "   font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(31,132,255,0.30);"
        "}"
    );
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        emit deleteButtonClicked(m_caseId);
    });
    footerLayout->addWidget(m_deleteButton);

    mainLayout->addLayout(footerLayout);
}

CaseInfoWidget::~CaseInfoWidget()
{
}

void CaseInfoWidget::setCaseInfo(const CaseItem& caseItem)
{
    m_caseId = caseItem.id;
    m_nameLabel->setText(caseItem.name);
    m_statsLabel->setText(
        QString(u8"路段总数：%1          节点总数：%2          小区总数：%3          OD点对：%4")
            .arg(caseItem.linkCount)
            .arg(caseItem.nodeCount)
            .arg(caseItem.zoneCount)
            .arg(caseItem.odPairCount));
    m_timeLabel->setText(QString(u8"时间：%1").arg(caseItem.createTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
    m_deleteButton->setVisible(caseItem.isBase != 1);
}

int CaseInfoWidget::caseId() const
{
    return m_caseId;
}

CasePanel::CasePanel(QWidget* parent)
    : QWidget(parent)
    , m_projectId(0)
    , m_currentCaseId(-1)
    , m_mainLayout(new QVBoxLayout(this))
    , m_workspacePage(nullptr)
    , m_sidebarStack(nullptr)
    , m_workspaceContentStack(nullptr)
    , m_schemeWorkspaceStack(nullptr)
    , m_savedSchemeWorkspaceIndex(0)
    , m_schemeTabButton(nullptr)
    , m_baseDataTabButton(nullptr)
    , m_modeButtonGroup(nullptr)
    , m_projectNameLabel(nullptr)
    , m_newCaseButton(nullptr)
    , m_compareButton(nullptr)
    , m_newCaseMainButton(nullptr)
    , m_caseModel(new CaseTreeModel(this))
    , m_caseTreeView(nullptr)
    , m_baseDataView(nullptr)
    , m_caseScrollArea(nullptr)
    , m_caseListWidget(nullptr)
    , m_caseListLayout(nullptr)
    , m_baseMapWidget(nullptr)
    , m_baseCrsStatusLabel(nullptr)
    , m_baseCoordStatusLabel(nullptr)
    , m_baseScaleStatusLabel(nullptr)
    , m_detailPanel(new CaseDetailPanel(this))
    , m_baseMapToolbar(nullptr)
    , m_mapToolController(nullptr)
    , m_lastToolOpenAtMs(0)
    , m_uiEventConnected(false)
{
    initUi();
}

CasePanel::~CasePanel()
{
    prepareForClose();

    if (m_mapToolController && m_detailPanel) {
        if (TNMapWidget* map = m_detailPanel->standardMapWidget()) {
            m_mapToolController->cancelAiWaySelect(map);
        }
        if (TNMapWidget* leftMap = m_detailPanel->leftCompareMapWidget()) {
            m_mapToolController->cancelAiWaySelect(leftMap);
        }
        if (TNMapWidget* rightMap = m_detailPanel->rightCompareMapWidget()) {
            m_mapToolController->cancelAiWaySelect(rightMap);
        }
    }
}

void CasePanel::setProjectName(const QString& projectName)
{
    m_projectName = projectName;
    if (m_projectNameLabel) {
        m_projectNameLabel->setText(projectName);
    }
}

void CasePanel::prepareForClose()
{
    if (m_preparedForClose) {
        return;
    }
    m_preparedForClose = true;

    if (m_uiEventConnected) {
        disconnect(&UIG(), &UIGlobalState::uiEventNotify, this, &CasePanel::onUiEventNotify);
        m_uiEventConnected = false;
    }

    CaseWfsMap::invalidatePendingLoads();
    CaseWfsMap::clearLoadQueueIdleCallback();
    CaseWfsMap::clearBatchLoadFailureReconcileCallback();
    CaseWfsMap::clearCapabilitiesWarmFinishedCallback();
    m_wfsRefreshBusy = false;
    CaseWayFeatureAdd::clearNodeIndexCache();

    // 属性表持有图层与画布指针，删层前先解绑
    if (m_baseAttributeWidget) {
        m_baseAttributeWidget->bindMap(nullptr);
    }
    if (m_detailPanel) {
        if (CaseMapAttributeWidget* attr = m_detailPanel->standardAttributeWidget()) {
            attr->bindMap(nullptr);
        }
        if (CaseMapAttributeWidget* attr = m_detailPanel->compareAttributeWidget()) {
            attr->bindMap(nullptr);
        }
    }

    clearDetailPanelWfsLayers();
    if (m_baseMapWidget) {
        CaseWfsMap::removePanelWfsLayers(m_baseMapWidget);
    }
    if (m_detailPanel && m_detailPanel->workPanel()) {
        m_detailPanel->workPanel()->prepareForShutdown();
    }
}

void CasePanel::setProjectId(int projectId)
{
    if (!m_uiEventConnected) {
        connect(&UIG(), &UIGlobalState::uiEventNotify, this, &CasePanel::onUiEventNotify);
        m_uiEventConnected = true;
    }

    if (m_projectId != projectId) {
        m_hasBaseDataTypenamesSnapshot = false;
        m_lastBaseDataTypenames.clear();
        // 切换项目后差集快照已失效，清掉待打开状态，避免误开新项目里的方案
        m_pendingOpenCaseId = -1;
        m_pendingOpenByDiff = false;
        m_beforeCreateCaseIds.clear();
    }
    m_projectId = projectId;
    if (m_baseMapWidget) {
        m_baseMapWidget->setStyleContext(m_projectId, 0);
    }
    if (m_detailPanel) {
        if (TNMapWidget* map = m_detailPanel->standardMapWidget()) {
            map->setStyleContext(m_projectId, m_currentCaseId);
        }
        if (TNMapWidget* leftMap = m_detailPanel->leftCompareMapWidget()) {
            leftMap->setStyleContext(m_projectId, m_detailPanel->leftCompareCaseId());
        }
        if (TNMapWidget* rightMap = m_detailPanel->rightCompareMapWidget()) {
            rightMap->setStyleContext(m_projectId, m_detailPanel->rightCompareCaseId());
        }
    }

    if (m_projectId > 0 && UIG().currentProjectId() == m_projectId) {
        applyProjectOpenState(static_cast<int>(UIG().projectOpenState()), UIG().projectOpenError());
    }
}

void CasePanel::setCaseList(const QList<CaseItem>& caseList)
{
    const int preservedCaseId =
        (isSchemeDetailViewActive() && m_currentCaseId > 0) ? m_currentCaseId : -1;

    if (caseList.isEmpty()) {
        clearDetailPanelWfsLayers();
        if (m_baseMapWidget) {
            CaseWfsMap::removePanelWfsLayers(m_baseMapWidget);
        }
    }

    m_caseList = caseList;
    m_caseModel->initData(caseList);
    m_detailPanel->setCaseList(caseList);
    refreshCaseCards();

    // 创建接口未返回可解析 id 时，按创建前快照的差集识别新方案（基础方案不跳转）
    if (m_pendingOpenCaseId <= 0 && m_pendingOpenByDiff && !m_beforeCreateCaseIds.isEmpty()) {
        for (const CaseItem& item : caseList) {
            if (m_beforeCreateCaseIds.contains(item.id)) {
                continue;
            }
            if (item.isBase != 1) {
                m_pendingOpenCaseId = item.id;
            }
            m_pendingOpenByDiff = false;
            m_beforeCreateCaseIds.clear();
            break;
        }
    }

    // 新建改造方案成功后自动跳转打开新方案详情（列表中未找到时保留，待下次刷新消费）
    if (m_pendingOpenCaseId > 0 && !caseList.isEmpty()) {
        for (const CaseItem& item : caseList) {
            if (item.id != m_pendingOpenCaseId) {
                continue;
            }
            const int pendingId = m_pendingOpenCaseId;
            m_pendingOpenCaseId = -1;
            m_pendingOpenByDiff = false;
            m_beforeCreateCaseIds.clear();
            if (item.isBase != 1) {
                openCaseById(pendingId);
                // openCaseById 已完成详情切换与树聚焦，直接返回，避免下方 preserved 分支用旧方案覆盖
                return;
            }
            break;
        }
    }

    if (caseList.isEmpty()) {
        m_currentCaseId = -1;
        CaseItem emptyCase {};
        emptyCase.name = QString::fromUtf8(u8"未选择方案");
        m_detailPanel->setCaseInfo(emptyCase);
        return;
    }

    if (preservedCaseId > 0) {
        for (const CaseItem& item : caseList) {
            if (item.id == preservedCaseId) {
                m_currentCaseId = preservedCaseId;
                m_detailPanel->setCaseInfo(item);
                // initData 重置模型会清掉树的展开/选中状态，这里恢复聚焦当前方案
                if (m_caseTreeView) {
                    m_caseTreeView->focusCaseNode(m_currentCaseId);
                }
                return;
            }
        }

        resetSchemeWorkspaceNavigation("setCaseList:currentCaseRemoved");
        m_currentCaseId = caseList.first().id;
        m_detailPanel->setCaseInfo(caseList.first());
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
                             QString::fromUtf8(u8"当前方案已不存在，已返回方案列表。"));
        return;
    }

    m_currentCaseId = caseList.first().id;
    m_detailPanel->setCaseInfo(caseList.first());
}

void CasePanel::addCase(const CaseItem& item)
{
    m_caseList.append(item);
    setCaseList(m_caseList);
}

void CasePanel::removeCase(int caseId)
{
    for (int i = 0; i < m_caseList.size(); ++i) {
        if (m_caseList.at(i).id == caseId) {
            m_caseList.removeAt(i);
            break;
        }
    }
    setCaseList(m_caseList);
}

void CasePanel::updateCase(const CaseItem& item)
{
    for (int i = 0; i < m_caseList.size(); ++i) {
        if (m_caseList.at(i).id == item.id) {
            m_caseList[i] = item;
            break;
        }
    }
    setCaseList(m_caseList);
}

void CasePanel::onSchemeTabClicked()
{
    m_sidebarStack->setCurrentIndex(0);
    m_workspaceContentStack->setCurrentIndex(0);
    if (m_schemeWorkspaceStack) {
        m_schemeWorkspaceStack->setCurrentIndex(m_savedSchemeWorkspaceIndex);
    }
    updateModeButtons();
}

void CasePanel::onBaseDataTabClicked()
{
    if (m_workspaceContentStack && m_workspaceContentStack->currentIndex() == 0 && m_schemeWorkspaceStack) {
        m_savedSchemeWorkspaceIndex = m_schemeWorkspaceStack->currentIndex();
    }
    m_sidebarStack->setCurrentIndex(1);
    m_workspaceContentStack->setCurrentIndex(1);
    updateModeButtons();
}

void CasePanel::onNewCaseClicked()
{
    if (m_projectId <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前项目无效，无法新建方案。"));
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    NewCaseDialog dialog(this);
    dialog.reset();
    dialog.applyNewCaseMode(m_caseList.isEmpty());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString caseName = dialog.caseName().trimmed();
    const int caseTypeId = dialog.caseTypeId();
    // 记录创建前的方案 id 快照：创建接口可能不返回可解析的方案 id，刷新后按差集兜底识别
    m_pendingOpenCaseId = -1; // 丢弃上一次未消费的待打开 id，避免与新方案叠加
    m_beforeCreateCaseIds.clear();
    for (const CaseItem& item : m_caseList) {
        m_beforeCreateCaseIds.insert(item.id);
    }
    m_pendingOpenByDiff = false;
    HttpApis::addCase(token, caseName, caseTypeId, {}, {}, {}, m_projectId, [this](bool suc,
                                                                                    const QSharedPointer<CaseJson>& response,
                                                                                    const QString& errMsg) {
        if (!suc || response.isNull()) {
            const QString message = errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"新建方案失败") : errMsg.trimmed();
            TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
            return;
        }

        UIG().reloadCurrentProjectCases();
        // 新建改造方案成功后，待方案列表刷新完自动打开新方案（基础方案不跳转）
        if (response->j_id() > 0) {
            m_pendingOpenCaseId = response->j_id();
        } else {
            qInfo() << QString::fromUtf8(u8"新建方案接口未返回可解析的方案 id（j_id=%1），刷新后按列表差集识别。")
                           .arg(response->j_id());
            m_pendingOpenByDiff = true;
        }
        emit newCaseRequested(response->j_id());
    });
}

void CasePanel::onCaseCompareClicked()
{
    if (m_projectId <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"当前项目无效。"));
        return;
    }
    if (m_caseList.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"当前项目暂无方案可对比。"));
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    TnUi::showInformation(QString::fromUtf8(u8"方案对比"),
                             QString::fromUtf8(u8"请先在基础方案地图上框选对比地理范围。"));

    const int baseCaseId = findBaseCaseId();
    if (baseCaseId <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"未找到基础方案。"));
        return;
    }

    if (!ensureBaseCaseDetailForCompare(baseCaseId)) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"无法打开基础方案地图。"));
        return;
    }

    if (!m_mapToolController || !m_detailPanel) {
        return;
    }

    TNMapWidget* map = m_detailPanel->standardMapWidget();
    if (!map) {
        TnUi::showWarning(QString::fromUtf8(u8"方案对比"), QString::fromUtf8(u8"基础方案地图未就绪。"));
        return;
    }

    QPointer<CasePanel> self(this);
    m_mapToolController->runCompareExtentPick(map, [self](const QString& checkExtent) {
        if (!self || checkExtent.trimmed().isEmpty()) {
            return;
        }

        CaseComparisonDialog dialog(self->m_caseList, self);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const QList<int> caseIds = dialog.selectedCaseIds();
        QString type = QStringLiteral("motor");
        if (self->m_detailPanel) {
            if (CaseWorkPanel* workPanel = self->m_detailPanel->workPanel()) {
                type = workPanel->grpcTransportType();
            }
        }
        requestCaseComparisonReport(self, self->m_projectId, caseIds, checkExtent, type);
    });
}

void CasePanel::enterSchemeCompareView()
{
    if (m_caseList.isEmpty()) {
        return;
    }

    if (m_caseList.size() == 1) {
        m_detailPanel->setCompareCases(toCompareItem(m_caseList.first()), toCompareItem(m_caseList.first()));
    } else {
        m_detailPanel->setCompareCases(toCompareItem(m_caseList.at(0)), toCompareItem(m_caseList.at(1)));
    }

    m_compareRenderStyle = CompareRenderStyle::None;
    m_detailPanel->setToolbarViewMode(CaseMapToolbarWidget::ViewMode::Compare);
    syncSchemeDetailWfsFromCaseTree();
    if (m_schemeWorkspaceStack) {
        m_schemeWorkspaceStack->setCurrentWidget(m_detailPanel);
        m_savedSchemeWorkspaceIndex = m_schemeWorkspaceStack->currentIndex();
    }
    emit caseCompareRequested();
}

void CasePanel::onCaseOpened(int caseId)
{
    qDebug() << "[CasePanelNav] onCaseOpened caseId=" << caseId
             << "stackIndex=" << schemeWorkspaceCurrentIndex();
    openCaseById(caseId);
    qDebug() << "[CasePanelNav] onCaseOpened done stackIndex=" << schemeWorkspaceCurrentIndex();
    emit caseOpened(caseId);
}

void CasePanel::onCaseEdited(int caseId)
{
    if (m_projectId <= 0 || caseId <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前项目或方案无效，无法编辑。"));
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    CaseJson caseJson;
    bool foundJson = false;
    for (const CaseJson& raw : UIG().currentProjectCases()) {
        if (raw.j_id() == caseId) {
            caseJson = raw;
            foundJson = true;
            break;
        }
    }
    if (!foundJson) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"未找到方案数据，请稍后重试。"));
        return;
    }

    QString rawName;
    int typeId = caseJson.j_case_type_id();
    for (const CaseItem& item : m_caseList) {
        if (item.id == caseId) {
            rawName = item.parameters.value(QStringLiteral("rawName")).toString().trimmed();
            if (rawName.isEmpty()) {
                rawName = item.name;
            }
            typeId = item.typeId;
            break;
        }
    }
    if (rawName.isEmpty()) {
        rawName = caseJson.j_name();
    }

    QStringList roadTypes;
    for (const QString& s : caseJson.j_road_types()) {
        roadTypes.append(s);
    }
    QStringList busTypes;
    for (const QString& s : caseJson.j_bus_types()) {
        busTypes.append(s);
    }
    QStringList otherTypes;
    for (const QString& s : caseJson.j_other_types()) {
        otherTypes.append(s);
    }

    NewCaseDialog dialog(this);
    dialog.reset();
    dialog.setTitleText(QString::fromUtf8(u8"编辑方案"));
    dialog.setCaseName(rawName);
    dialog.setCaseTypeId(typeId);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString newName = dialog.caseName().trimmed();
    const int newTypeId = dialog.caseTypeId();

    HttpApis::editCase(token,
                       caseId,
                       newName,
                       newTypeId,
                       roadTypes,
                       busTypes,
                       otherTypes,
                       [this, caseId](bool suc, const QSharedPointer<ResponseJson>& response, const QString& errMsg) {
                           Q_UNUSED(response)
                           if (!suc) {
                               const QString message =
                                   errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"编辑方案失败") : errMsg.trimmed();
                               TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
                               return;
                           }

                           UIG().reloadCurrentProjectCases();
                           emit caseEdited(caseId);
                       });
}

void CasePanel::onCaseDeleted(int caseId)
{
    if (caseId <= 0) {
        return;
    }

    QString caseLabel;
    bool isBaseCase = false;
    for (const CaseItem& item : m_caseList) {
        if (item.id == caseId) {
            caseLabel = item.name;
            isBaseCase = item.isBase == 1;
            break;
        }
    }
    if (isBaseCase) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"基础方案不可删除。"));
        return;
    }
    if (caseLabel.trimmed().isEmpty()) {
        caseLabel = QString::number(caseId);
    }

    if (!TnUi::confirmYesNo(QString::fromUtf8(u8"确认删除"),
                            QString::fromUtf8(u8"确定要删除方案 \"%1\" 吗？").arg(caseLabel))) {
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    HttpApis::deleteCase(token, caseId, [this, caseId](bool suc, const QSharedPointer<ResponseJson>& response, const QString& errMsg) {
        Q_UNUSED(response)
        if (!suc) {
            const QString message = errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"删除方案失败") : errMsg.trimmed();
            TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
            return;
        }

        UIG().reloadCurrentProjectCases();
        if (m_detailPanel && m_detailPanel->workPanel()) {
            m_detailPanel->workPanel()->clearSessionsForCase(caseId);
        }
        emit caseDeleted(caseId);
    });
}

void CasePanel::onCaseRenameRequested(int caseId)
{
    if (caseId <= 0) {
        return;
    }

    QString initialName;
    for (const CaseItem& item : m_caseList) {
        if (item.id == caseId) {
            initialName = item.parameters.value(QStringLiteral("rawName")).toString().trimmed();
            if (initialName.isEmpty()) {
                initialName = item.name;
            }
            if (initialName.isEmpty()) {
                initialName = item.name;
            }
            break;
        }
    }

    RenameCaseDialog dialog(this);
    dialog.setCaseName(initialName);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString newName = dialog.caseName().trimmed();
    if (newName.isEmpty()) {
        return;
    }

    onCaseRenamed(caseId, newName);
}

void CasePanel::onCaseRenamed(int caseId, const QString& newName)
{
    if (caseId <= 0 || newName.trimmed().isEmpty()) {
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    HttpApis::editCaseName(token, caseId, newName.trimmed(), [this, caseId, newName](bool suc,
                                                                                     const QSharedPointer<ResponseJson>& response,
                                                                                     const QString& errMsg) {
        Q_UNUSED(response)
        if (!suc) {
            const QString message = errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"重命名方案失败") : errMsg.trimmed();
            TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
            return;
        }

        UIG().reloadCurrentProjectCases();
        emit caseRenamed(caseId, newName);
    });
}

void CasePanel::onCaseOverwriteRequested(int caseId)
{
    if (caseId <= 0) {
        return;
    }

    bool isBaseCase = false;
    QString caseLabel;
    for (const CaseItem& item : m_caseList) {
        if (item.id == caseId) {
            isBaseCase = item.isBase == 1;
            caseLabel = item.name;
            break;
        }
    }
    if (!isBaseCase) {
        return;
    }
    if (caseLabel.trimmed().isEmpty()) {
        caseLabel = QString::fromUtf8(u8"基础方案");
    }

    if (!TnUi::confirmYesNo(QString::fromUtf8(u8"确认覆盖"),
                            //QString::fromUtf8(u8"确定要用方案 \"%1\" 覆盖项目基础数据吗？").arg(caseLabel),
                            QString::fromUtf8(u8"确定要覆盖项目基础方案数据吗？"))) {
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    HttpApis::copyProjectCase(token, caseId, [this, caseId](bool suc, const QSharedPointer<ResponseJson>& response, const QString& errMsg) {
        Q_UNUSED(response)
        if (!suc) {
            const QString message = errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"覆盖基础方案失败") : errMsg.trimmed();
            TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
            return;
        }

        UIG().reloadCurrentProjectCases();
        if (m_currentCaseId == caseId) {
            syncSchemeDetailWfsFromCaseTree();
        }
    });
}

void CasePanel::onSchemeLayerCsvImportRequested(int caseId, const QString& table, const QString& layerDisplayName)
{
    if (m_projectId <= 0 || caseId <= 0 || table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前项目或图层数据无效，无法导入。"));
        return;
    }
    requestDrcsvLayerImport(caseId, table, layerDisplayName);
}

void CasePanel::onSchemeLayerExportRequested(int caseId,
                                             const QString& table,
                                             const QString& exportType,
                                             const QString& layerDisplayName)
{
    if (m_projectId <= 0 || caseId <= 0 || table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前项目或图层数据无效，无法导出。"));
        return;
    }
    requestProjectTableExport(caseId, table, exportType, layerDisplayName);
}

void CasePanel::onBaseDataLayerCsvImportRequested(const QString& table, const QString& layerDisplayName)
{
    if (m_projectId <= 0 || table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前项目或图层数据无效，无法导入。"));
        return;
    }
    requestDrcsvLayerImport(0, table, layerDisplayName);
}

void CasePanel::onBaseDataLayerExportRequested(const QString& table,
                                               const QString& exportType,
                                               const QString& layerDisplayName)
{
    if (m_projectId <= 0 || table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前项目或图层数据无效，无法导出。"));
        return;
    }
    requestProjectTableExport(0, table, exportType, layerDisplayName);
}

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
void CasePanel::onSchemeLayerZoomToRequested(int caseId, const QString& wfsTypeName, const QString& layerDisplayName)
{
    Q_UNUSED(layerDisplayName)

    TNMapWidget* map = mapWidgetForSchemeCase(caseId);
    if (!map) {
        TnUi::showInformation(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"当前地图未激活该方案下的图层。请切换到对应方案或双屏对比中的左/右侧方案，并勾选显示该图层后再试。"));
        return;
    }

    const QString trimmed = wfsTypeName.trimmed();
    if (trimmed.isEmpty()) {
        TnUi::showInformation(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"该图层缺少 WFS 类型名（typename），无法缩放至图层范围。"));
        return;
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(trimmed);
    QgsMapLayer* layer = map->getLayer(uri);
    if (!layer) {
        TnUi::showInformation(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"地图中尚未加载该图层。请先在列表中勾选显示该图层，待加载完成后再使用「缩放至图层」。"));
        return;
    }

    if (TNMapWidget::isAttributeOnlyMapLayer(layer)) {
        TnUi::showInformation(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"该表无地理范围，无法缩放至图层。"));
        return;
    }

    map->zoomToLayer(layer);
}

void CasePanel::onBaseDataLayerZoomToRequested(const QString& wfsTypeName, const QString& layerDisplayName)
{
    Q_UNUSED(layerDisplayName)

    if (!m_baseMapWidget) {
        return;
    }

    const QString trimmed = wfsTypeName.trimmed();
    if (trimmed.isEmpty()) {
        TnUi::showInformation(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"该图层缺少 WFS 类型名（value），无法缩放至图层范围。"));
        return;
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(trimmed);
    QgsMapLayer* layer = m_baseMapWidget->getLayer(uri);
    if (!layer) {
        TnUi::showInformation(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"地图中尚未加载该图层。请先在列表中勾选显示该图层，待加载完成后再使用「缩放至图层」。"));
        return;
    }

    if (TNMapWidget::isAttributeOnlyMapLayer(layer)) {
        TnUi::showInformation(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"该表无地理范围，无法缩放至图层。"));
        return;
    }

    m_baseMapWidget->zoomToLayer(layer);
}
#endif

namespace {

/**
 * @brief 收集 SHP 及同名伴生文件，用程序目录下的 7z.exe 打包为标准 zip
 * @param shpPath     选中的 .shp 文件路径
 * @param errorMessage 失败原因输出
 * @return 成功返回 zip 文件路径，失败返回空字符串
 */
QString packShpSidecarFilesToZip(const QString& shpPath, QString* errorMessage)
{
    const QFileInfo shpInfo(shpPath);
    const QString baseName = shpInfo.completeBaseName();

    // 工作目录切到 shp 所在目录，只传文件名，避免 zip 内保留目录层级
    QStringList files;
    files << shpInfo.fileName();
    // 同目录同名伴生文件，存在才收集
    static const QLatin1String kSidecarExts[] = {
        QLatin1String("dbf"), QLatin1String("shx"), QLatin1String("prj"),
        QLatin1String("cpg"), QLatin1String("qpj")
    };
    for (const QLatin1String& ext : kSidecarExts) {
        const QString candidate = QDir(shpInfo.absolutePath()).filePath(baseName + QLatin1Char('.') + ext);
        if (QFileInfo::exists(candidate)) {
            files << QFileInfo(candidate).fileName();
        }
    }

    const QString sevenZip = QCoreApplication::applicationDirPath() + QStringLiteral("/7z.exe");
    if (!QFileInfo::exists(sevenZip)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"未找到打包程序 7z.exe（应位于程序安装目录）。");
        }
        return QString();
    }

    // 打包输出到临时目录
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString zipDir = QDir(tempRoot).filePath(QStringLiteral("tnp_shp_import"));
    if (!QDir().mkpath(zipDir)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"创建临时打包目录失败。");
        }
        return QString();
    }
    const QString zipPath = QDir(zipDir).filePath(baseName + QStringLiteral(".zip"));
    QFile::remove(zipPath);

    // 标准 ZIP 压缩协议：7z a -tzip
    QProcess process;
    process.setProgram(sevenZip);
    process.setArguments(QStringList() << QStringLiteral("a")
                                       << QStringLiteral("-tzip")
                                       << QStringLiteral("-y")
                                       << zipPath << files);
    process.setWorkingDirectory(shpInfo.absolutePath());
    // 打包期间显示等待光标，避免界面像假死
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    process.start();
    const bool started = process.waitForStarted(5000);
    const bool finished = started && process.waitForFinished(120000);
    QGuiApplication::restoreOverrideCursor();
    if (!finished) {
        process.kill();
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"打包 SHP 文件失败：7z.exe 无法启动或执行超时。");
        }
        return QString();
    }
    if (process.exitCode() != 0 || !QFileInfo::exists(zipPath)) {
        const QString detail = QString::fromLocal8Bit(process.readAllStandardError().trimmed());
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"打包 SHP 文件失败：%1").arg(detail);
        }
        return QString();
    }
    return zipPath;
}

} // namespace

void CasePanel::requestDrcsvLayerImport(int caseId, const QString& table, const QString& layerDisplayName)
{
    if(OfflineApi::enabled()) {
        const QString selected=selectOfflineImportSource(this,layerDisplayName,table);if(selected.isEmpty())return;
        HttpApis::uploadCSV(AppSession::instance().token(),QStringLiteral("local"),m_projectId,caseId,table,selected,
            [this,caseId,table](bool ok,const QSharedPointer<ResponseJson>&,const QString& error){
                if(!ok){TnUi::showWarning(u8"导入失败",error);return;}
                TnUi::showInformation(u8"导入完成",u8"本地图层数据已更新。");
                for(auto* map:findChildren<TNMapWidget*>()) {
                    const QString uri=CaseWfsMap::buildWfsDataSourceUri(table);
                    if(auto* layer=qobject_cast<QgsVectorLayer*>(map->getLayer(uri))){layer->reload();layer->triggerRepaint();}
                }
                if(caseId==0)UIG().reloadCurrentProjectDetail();else UIG().reloadCurrentProjectCases();
            });
        return;
    }

    Q_UNUSED(layerDisplayName)

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(nullptr,
        QString::fromUtf8(u8"选择要导入的文件"),
        QString(),
        QString::fromUtf8(u8"CSV/TXT/XLSX (*.csv *.txt *.xlsx *.xls);;SHP (*.shp);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    // SHP 走 zip 打包上传流程
    if (QFileInfo(path).suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) == 0) {
        requestDrshpzipLayerImport(caseId, table, path, token);
        return;
    }

    QString typeLower = QFileInfo(path).suffix().toLower();
    if (typeLower == QLatin1String("xls")) {
        typeLower = QLatin1String("xlsx");
    }

    static const QSet<QString> kAllowed({ QStringLiteral("csv"), QStringLiteral("txt"), QStringLiteral("xlsx") });
    if (!kAllowed.contains(typeLower)) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请选择 CSV、TXT 或 XLSX 文件。"));
        return;
    }

    HttpApis::uploadCSV(token, typeLower, m_projectId, caseId, table, path,
        [caseId](bool suc, const QSharedPointer<ResponseJson>& response, const QString& errMsg) {
            if (!suc) {
                const QString message =
                    errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"导入失败") : errMsg.trimmed();
                TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
                return;
            }

            const QString okMsg = response.isNull() ? QString() : response->j_message().trimmed();
            TnUi::showInformation(QString::fromUtf8(u8"导入"),
                okMsg.isEmpty() ? QString::fromUtf8(u8"导入已完成。") : okMsg);

            CaseWfsMap::refreshWfsCapabilitiesCache();
            if (caseId == 0) {
                return;
            }
            UIG().reloadCurrentProjectCases();
        });
}

void CasePanel::requestDrshpzipLayerImport(int caseId, const QString& table, const QString& shpPath, const QString& token)
{
    // 收集伴生文件并打包为 zip，再经 drshpzip 接口上传
    QString packError;
    const QString zipPath = packShpSidecarFilesToZip(shpPath, &packError);
    if (zipPath.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"导入"), packError);
        return;
    }

    HttpApis::uploadShpZip(token, QStringLiteral("shp"), m_projectId, caseId, table, zipPath,
        [caseId, zipPath](bool suc, const QSharedPointer<ResponseJson>& response, const QString& errMsg) {
            // 上传结束后清理临时打包文件
            QFile::remove(zipPath);
            if (!suc) {
                const QString message =
                    errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"导入失败") : errMsg.trimmed();
                TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
                return;
            }

            const QString okMsg = response.isNull() ? QString() : response->j_message().trimmed();
            TnUi::showInformation(QString::fromUtf8(u8"导入"),
                okMsg.isEmpty() ? QString::fromUtf8(u8"导入已完成。") : okMsg);

            CaseWfsMap::refreshWfsCapabilitiesCache();
            if (caseId == 0) {
                return;
            }
            UIG().reloadCurrentProjectCases();
        });
}

void CasePanel::requestProjectTableExport(int caseId,
                                          const QString& table,
                                          const QString& exportType,
                                          const QString& layerDisplayName)
{
    Q_UNUSED(layerDisplayName)

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    const QString typeLower = exportType.trimmed().toLower();
    if (typeLower != QLatin1String("xlsx") && typeLower != QLatin1String("csv")) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"不支持的导出格式。"));
        return;
    }

    HttpApis::exportProjectTable(token, typeLower, m_projectId, caseId, table,
        [this, table](bool suc, const QSharedPointer<ProjectExportResult>& response, const QString& errMsg) {
            if (!suc || response.isNull()) {
                const QString message =
                    errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"导出失败") : errMsg.trimmed();
                TnUi::showWarning(QString::fromUtf8(u8"导出"), message);
                return;
            }

            const QString downloadUrl = response->j_downloadUrl().trimmed();
            const QString fileName = response->j_fileName().trimmed();
            if (downloadUrl.isEmpty()) {
                TnUi::showWarning(QString::fromUtf8(u8"导出"), QString::fromUtf8(u8"导出接口未返回下载地址。"));
                return;
            }

            auto* downloader = new FileDownloader(qApp);
            QObject::connect(downloader, &FileDownloader::finished, downloader,
                [downloader, table](bool success, const QString& filePath, const QString& downloadErrMsg) {
                    if (success) {
                        TnUi::showInformation(QString::fromUtf8(u8"导出"),
                            QString::fromUtf8(u8"表 %1 已导出至：\n%2").arg(table, filePath));
                    } else {
                        const QString message = downloadErrMsg.trimmed().isEmpty()
                            ? QString::fromUtf8(u8"下载失败")
                            : downloadErrMsg.trimmed();
                        TnUi::showWarning(QString::fromUtf8(u8"导出"), message);
                    }
                    downloader->deleteLater();
                });

            downloader->download(downloadUrl, QString(), fileName, nullptr);
        });
}

void CasePanel::onCaseCloned(int caseId)
{
    if (caseId <= 0) {
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    QString baseRawName;
    for (const CaseItem& item : m_caseList) {
        if (item.id == caseId) {
            baseRawName = item.parameters.value(QStringLiteral("rawName")).toString().trimmed();
            if (baseRawName.isEmpty()) {
                baseRawName = item.name;
            }
            break;
        }
    }
    if (baseRawName.isEmpty()) {
        baseRawName = QString::fromUtf8(u8"方案");
    }

    const QString cloneName = QString(u8"%1-副本").arg(baseRawName);

    HttpApis::copyCase(token, caseId, cloneName, [this, caseId](bool suc, const QSharedPointer<CaseJson>& response, const QString& errMsg) {
        Q_UNUSED(response)
        if (!suc) {
            const QString message = errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"克隆方案失败") : errMsg.trimmed();
            TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
            return;
        }

        UIG().reloadCurrentProjectCases();
        emit caseCloned(caseId);
    });
}

void CasePanel::initUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("CasePanel { background-color: #030d20; }");

    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_workspacePage = createWorkspacePage();
    m_mainLayout->addWidget(m_workspacePage, 1);

    m_detailPanel->setBuiltInSchemeSidebarVisible(false);

    connect(m_caseTreeView, &CaseTreeView::caseOpened, this, &CasePanel::onCaseOpened);
    connect(m_caseTreeView, &CaseTreeView::caseEdited, this, &CasePanel::onCaseEdited);
    connect(m_caseTreeView, &CaseTreeView::caseDeleted, this, &CasePanel::onCaseDeleted);
    connect(m_caseTreeView, &CaseTreeView::caseRenameRequested, this, &CasePanel::onCaseRenameRequested);
    connect(m_caseTreeView, &CaseTreeView::caseCloned, this, &CasePanel::onCaseCloned);
    connect(m_caseTreeView, &CaseTreeView::caseOverwriteRequested, this, &CasePanel::onCaseOverwriteRequested);
    connect(m_caseTreeView, &CaseTreeView::schemeLayerCsvImportRequested, this, &CasePanel::onSchemeLayerCsvImportRequested);
    connect(m_caseTreeView, &CaseTreeView::schemeLayerExportRequested, this, &CasePanel::onSchemeLayerExportRequested);
    connect(m_caseTreeView, &CaseTreeView::schemeLayerActivated, this, &CasePanel::onSchemeLayerActivated);
    connect(m_caseTreeView, &CaseTreeView::schemeLayerDataViewRequested, this, &CasePanel::onSchemeLayerDataViewRequested);
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    connect(m_caseTreeView, &CaseTreeView::schemeLayerZoomToRequested, this, &CasePanel::onSchemeLayerZoomToRequested);
#endif
    if (CaseTreeView* embeddedTree = m_detailPanel->embeddedSchemeTreeView()) {
        connect(embeddedTree, &CaseTreeView::schemeLayerCsvImportRequested, this, &CasePanel::onSchemeLayerCsvImportRequested);
        connect(embeddedTree, &CaseTreeView::schemeLayerExportRequested, this, &CasePanel::onSchemeLayerExportRequested);
        connect(embeddedTree, &CaseTreeView::schemeLayerDataViewRequested, this, &CasePanel::onSchemeLayerDataViewRequested);
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
        connect(embeddedTree, &CaseTreeView::schemeLayerZoomToRequested, this, &CasePanel::onSchemeLayerZoomToRequested);
#endif
    }
    connect(m_detailPanel, &CaseDetailPanel::newCaseRequested, this, &CasePanel::onNewCaseClicked);
    connect(m_detailPanel, &CaseDetailPanel::deriveCaseFromBaseRequested, this, &CasePanel::onNewCaseClicked);
    connect(m_detailPanel, &CaseDetailPanel::schemeCompareFlowRequested, this, &CasePanel::enterSchemeCompareView);
    connect(m_detailPanel, &CaseDetailPanel::caseOpened, this, &CasePanel::onCaseOpened);
    connect(m_detailPanel, &CaseDetailPanel::schemeListReturnRequested, this, &CasePanel::onSchemeListReturnRequested);
    connect(m_detailPanel, &CaseDetailPanel::trafficMicroDiagnosisRequested, this,
            &CasePanel::onTrafficMicroDiagnosisRequested);
    if (CaseWorkPanel* workPanel = m_detailPanel->workPanel()) {
        connect(workPanel, &CaseWorkPanel::analysisResultViewed, this, &CasePanel::onAnalysisResultViewed);
        connect(workPanel, &CaseWorkPanel::analysisDataRequested, this, &CasePanel::onAnalysisDataRequested);
        connect(workPanel, &CaseWorkPanel::aiDataSelectRequested, this, &CasePanel::onAiDataSelectRequested);
        connect(workPanel, &CaseWorkPanel::analysisModuleChanged, this, &CasePanel::onAnalysisModuleChanged);
        connect(workPanel, &CaseWorkPanel::stepRunFinished, this, &CasePanel::onWorkStepRunFinished);
    }
    connect(m_caseModel, &CaseTreeModel::checkStateChanged, this, &CasePanel::onCaseTreeLayerCheckChanged);
    connect(m_baseDataView, &BaseDataTableView::layerStateChangedWithData, this, &CasePanel::onBaseDataLayerCheckChanged);
    connect(m_baseDataView, &BaseDataTableView::baseDataLayerDataViewRequested, this,
            &CasePanel::onBaseDataLayerDataViewRequested);
    connect(m_detailPanel, &CaseDetailPanel::compareCasesChanged, this, &CasePanel::onDetailCompareCasesChanged);
    connect(m_detailPanel, &CaseDetailPanel::compareRenderStyleChanged, this, &CasePanel::onCompareRenderStyleChanged);
    if (CaseMapToolbarWidget* schemeTb = m_detailPanel->schemeToolbarWidget()) {
        connect(schemeTb, &CaseMapToolbarWidget::viewModeChanged, this, &CasePanel::onDetailToolbarViewModeChanged);
    }

    m_mapToolController = new CaseMapToolController(this);
    connect(m_detailPanel, &CaseDetailPanel::mapToolClicked, this, &CasePanel::onSchemeMapToolTriggered);
    if (m_baseMapToolbar) {
        connect(m_baseMapToolbar, &CaseMapToolbarWidget::toolTriggered, this, &CasePanel::onBaseMapToolTriggered);
    }
    if (m_baseMapWidget && m_baseMapToolbar) {
        bindEditStackToolbar(m_baseMapWidget, m_baseMapToolbar);
        connect(m_baseMapWidget, &TNMapWidget::editInteractionCleared, this, [this]() {
            if (m_baseMapToolbar) {
                m_baseMapToolbar->setMutexTool(CaseMapToolbarWidget::ToolType::Browse);
            }
        });
    }
    if (m_detailPanel && m_detailPanel->standardMapWidget() && m_detailPanel->schemeToolbarWidget()) {
        bindEditStackToolbar(m_detailPanel->standardMapWidget(), m_detailPanel->schemeToolbarWidget());
    }

    CaseWfsMap::setBatchLoadFailureReconcileCallback([this](TNMapWidget* map) {
        if (m_baseDataView && map == m_baseMapWidget) {
            m_baseDataView->reconcileCheckedLayersWithMap(map);
        }
    });
}

QWidget* CasePanel::createWorkspacePage()
{
    QWidget* page = new QWidget(this);
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(QStringLiteral("QWidget { background-color: #030d20; }"));
    QHBoxLayout* pageLayout = new QHBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto* workspaceSplitter = new QSplitter(Qt::Horizontal, page);
    workspaceSplitter->setChildrenCollapsible(false);
    workspaceSplitter->setHandleWidth(6);
    workspaceSplitter->setStyleSheet(
        "QSplitter::handle {"
        "   background: rgba(71, 102, 146, 0.35);"
        "   margin: 6px 0;"
        "}"
        "QSplitter::handle:hover {"
        "   background: rgba(42, 163, 255, 0.55);"
        "}"
    );

    QWidget* sidebarArea = new QWidget(workspaceSplitter);
    sidebarArea->setMinimumWidth(200);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarArea);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    sidebarLayout->addWidget(createModeSwitchTabs());

    m_sidebarStack = new QStackedWidget(sidebarArea);
    m_sidebarStack->setMinimumWidth(0);
    m_sidebarStack->addWidget(createSchemeSidebar());
    m_baseDataView = new BaseDataTableView(sidebarArea);
    m_baseDataView->setMinimumWidth(0);
    connect(m_baseDataView, &BaseDataTableView::toolNodeClicked, this, &CasePanel::onBaseDataToolNodeClicked);
    connect(m_baseDataView, &BaseDataTableView::baseDataLayerCsvImportRequested, this,
        &CasePanel::onBaseDataLayerCsvImportRequested);
    connect(m_baseDataView, &BaseDataTableView::baseDataLayerExportRequested, this,
        &CasePanel::onBaseDataLayerExportRequested);
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    connect(m_baseDataView, &BaseDataTableView::baseDataLayerZoomToRequested, this,
        &CasePanel::onBaseDataLayerZoomToRequested);
#endif
    m_sidebarStack->addWidget(m_baseDataView);
    sidebarLayout->addWidget(m_sidebarStack, 1);

    QWidget* contentArea = new QWidget(workspaceSplitter);
    contentArea->setAttribute(Qt::WA_StyledBackground, true);
    contentArea->setStyleSheet(QStringLiteral("QWidget { background-color: #030d20; }"));
    QVBoxLayout* contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(0);

    m_workspaceContentStack = new QStackedWidget(contentArea);
    m_workspaceContentStack->setAttribute(Qt::WA_StyledBackground, true);
    m_workspaceContentStack->setStyleSheet(QStringLiteral("QStackedWidget { background-color: #030d20; border: none; }"));

    m_schemeWorkspaceStack = new QStackedWidget(contentArea);
    m_schemeWorkspaceStack->setAttribute(Qt::WA_StyledBackground, true);
    m_schemeWorkspaceStack->setStyleSheet(QStringLiteral("QStackedWidget { background-color: #030d20; border: none; }"));
    m_schemeWorkspaceStack->addWidget(createSchemeContentPage());
    m_schemeWorkspaceStack->addWidget(m_detailPanel);

    m_workspaceContentStack->addWidget(m_schemeWorkspaceStack);
    m_workspaceContentStack->addWidget(createBaseDataPage());
    contentLayout->addWidget(m_workspaceContentStack, 1);

    workspaceSplitter->addWidget(sidebarArea);
    workspaceSplitter->addWidget(contentArea);
    workspaceSplitter->setStretchFactor(0, 0);
    workspaceSplitter->setStretchFactor(1, 1);
    workspaceSplitter->setSizes({238, 1400});

    pageLayout->addWidget(workspaceSplitter, 1);

    return page;
}

QWidget* CasePanel::createSchemeSidebar()
{
    QWidget* sidebar = new QWidget(this);
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setStyleSheet(
        "QWidget {"
        "   background-color: #050f1f;"
        "   border-right: 1px solid rgba(71, 102, 146, 0.35);"
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(20, 16, 20, 14);
    layout->setSpacing(10);

    m_newCaseButton = new QPushButton(u8"新建方案", sidebar);
    m_newCaseButton->setCursor(Qt::PointingHandCursor);
    m_newCaseButton->setFixedSize(132, 32);
    m_newCaseButton->setIcon(QIcon(QStringLiteral(":/icons/icons/add_project.png")));
    m_newCaseButton->setIconSize(QSize(14, 14));
    m_newCaseButton->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 16px;"
        "   background-color: #1f84ff;"
        "   color: white;"
        "   font-size: 13px;"
        "   font-weight: 700;"
        "   padding: 0 12px;"
        "}"
    );
    connect(m_newCaseButton, &QPushButton::clicked, this, &CasePanel::onNewCaseClicked);
    layout->addWidget(m_newCaseButton, 0, Qt::AlignHCenter);

    m_compareButton = createGhostButton(u8"方案对比", sidebar);
    connect(m_compareButton, &QPushButton::clicked, this, &CasePanel::onCaseCompareClicked);
    layout->addWidget(m_compareButton, 0, Qt::AlignHCenter);
    layout->addSpacing(10);

    m_caseTreeView = new CaseTreeView(sidebar);
    m_caseTreeView->setCaseModel(m_caseModel);
    layout->addWidget(m_caseTreeView, 1);
    return sidebar;
}

QWidget* CasePanel::createSchemeContentPage()
{
    QWidget* page = new QWidget(this);
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setObjectName(QStringLiteral("caseSchemeWorkspacePage"));
    page->setStyleSheet(QStringLiteral(
        "QWidget#caseSchemeWorkspacePage { background-color: %1; }"
    ).arg(QLatin1String(kSchemePageOuterBg)));

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 10);
    headerLayout->setSpacing(16);
    m_projectNameLabel = new QLabel(u8"项目方案", page);
    m_projectNameLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );
    headerLayout->addWidget(m_projectNameLabel);
    headerLayout->addStretch();

    m_newCaseMainButton = new QPushButton(u8"新建方案", page);
    m_newCaseMainButton->setCursor(Qt::PointingHandCursor);
    m_newCaseMainButton->setMinimumSize(140, 36);
    m_newCaseMainButton->setIcon(QIcon(QStringLiteral(":/icons/icons/add_project.png")));
    m_newCaseMainButton->setIconSize(QSize(14, 14));
    m_newCaseMainButton->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 18px;"
        "   background-color: #1f84ff;"
        "   color: white;"
        "   font-size: 15px;"
        "   font-weight: 700;"
        "}"
    );
    connect(m_newCaseMainButton, &QPushButton::clicked, this, &CasePanel::onNewCaseClicked);
    headerLayout->addWidget(m_newCaseMainButton);
    layout->addLayout(headerLayout);

    QFrame* headerRule = new QFrame(page);
    headerRule->setFixedHeight(1);
    headerRule->setStyleSheet(QStringLiteral(
        "QFrame { background-color: rgba(71, 102, 146, 0.45); max-height: 1px; border: none; }"));
    layout->addWidget(headerRule);

    QFrame* listShell = new QFrame(page);
    listShell->setObjectName(QStringLiteral("caseSchemeListShell"));
    listShell->setAttribute(Qt::WA_StyledBackground, true);
    listShell->setStyleSheet(QStringLiteral(
        "QFrame#caseSchemeListShell {"
        "   background-color: %1;"
        "   border-radius: 14px;"
        "   border: 1px solid rgba(71, 102, 146, 0.35);"
        "}"
    ).arg(QLatin1String(kSchemeListShellBg)));

    QVBoxLayout* shellLayout = new QVBoxLayout(listShell);
    shellLayout->setContentsMargins(14, 14, 14, 14);
    shellLayout->setSpacing(0);

    m_caseScrollArea = new QScrollArea(listShell);
    m_caseScrollArea->setWidgetResizable(true);
    m_caseScrollArea->setFrameShape(QFrame::NoFrame);

    m_caseListWidget = new QWidget(m_caseScrollArea);
    m_caseListWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_caseListWidget->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
    m_caseListLayout = new QVBoxLayout(m_caseListWidget);
    m_caseListLayout->setContentsMargins(0, 0, 0, 0);
    m_caseListLayout->setSpacing(16);
    m_caseScrollArea->setWidget(m_caseListWidget);

    if (QWidget* scrollViewport = m_caseScrollArea->viewport()) {
        scrollViewport->setAttribute(Qt::WA_StyledBackground, true);
        scrollViewport->setAutoFillBackground(false);
        scrollViewport->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
    }

    m_caseScrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: rgba(0,0,0,0.22); width: 10px; margin: 0; border-radius: 5px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.22); min-height: 24px; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

    shellLayout->addWidget(m_caseScrollArea, 1);
    layout->addWidget(listShell, 1);
    return page;
}

QWidget* CasePanel::createBaseDataPage()
{
    QWidget* page = new QWidget(this);
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(QStringLiteral("QWidget { background-color: #030d20; }"));
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createBaseMapCanvas(), 1);
    return page;
}

QWidget* CasePanel::createModeSwitchTabs()
{
    QWidget* header = new QWidget(this);
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setStyleSheet(
        "QWidget {"
        "   background-color: #050f1f;"
        "   border-right: 1px solid rgba(71, 102, 146, 0.35);"
        "}"
    );

    QHBoxLayout* layout = new QHBoxLayout(header);
    layout->setContentsMargins(20, 16, 20, 12);
    layout->setSpacing(18);

    m_schemeTabButton = new QPushButton(QString::fromUtf8(u8"项目方案"), header);
    m_baseDataTabButton = new QPushButton(QString::fromUtf8(u8"基础数据"), header);

    const QString tabStyle =
        "QPushButton {"
        "   border: none;"
        "   padding: 0 0 8px 0;"
        "   color: rgba(255,255,255,0.84);"
        "   background: transparent;"
        "   font-size: 15px;"
        "   font-weight: 700;"
        "}"
        "QPushButton:checked {"
        "   color: #2aa3ff;"
        "   border-bottom: 2px solid #2aa3ff;"
        "}";

    m_schemeTabButton->setCheckable(true);
    m_baseDataTabButton->setCheckable(true);
    m_schemeTabButton->setStyleSheet(tabStyle);
    m_baseDataTabButton->setStyleSheet(tabStyle);

    m_modeButtonGroup = new QButtonGroup(header);
    m_modeButtonGroup->setExclusive(true);
    m_modeButtonGroup->addButton(m_schemeTabButton);
    m_modeButtonGroup->addButton(m_baseDataTabButton);
    m_schemeTabButton->setChecked(true);

    connect(m_schemeTabButton, &QPushButton::clicked, this, &CasePanel::onSchemeTabClicked);
    connect(m_baseDataTabButton, &QPushButton::clicked, this, &CasePanel::onBaseDataTabClicked);

    layout->addWidget(m_schemeTabButton);
    layout->addWidget(m_baseDataTabButton);
    layout->addStretch();
    return header;
}

QWidget* CasePanel::createBaseMapCanvas()
{
    // 地图卡片区域样式对齐 CaseDetailPanel 标准视图（设计稿：方案-基础数据）
    QWidget* wrapper = new QWidget(this);
    wrapper->setMinimumWidth(280);
    wrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(18, 20, 18, 16);
    layout->setSpacing(12);

    QLabel* titleLabel = new QLabel(QString::fromUtf8(u8"基础数据"), wrapper);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );
    layout->addWidget(titleLabel);

    auto* host = new BaseMapToolbarHostWidget(wrapper);
    QVBoxLayout* hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    QWidget* page = new QWidget(host);
    QVBoxLayout* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* mapShell = new QFrame(page);
    mapShell->setObjectName(QStringLiteral("caseBaseDataMapShell"));
    mapShell->setStyleSheet(QStringLiteral(
        "QFrame#caseBaseDataMapShell { background: transparent; border: none; }"));
    applyBaseDataMapShellShadow(mapShell);

    auto* shellLayout = new QVBoxLayout(mapShell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    QFrame* mapViewport = new QFrame(mapShell);
    mapViewport->setObjectName(QStringLiteral("caseBaseDataMapViewport"));
    mapViewport->setAttribute(Qt::WA_StyledBackground, true);
    const QString rv = QString::number(kDesignBaseMapViewportRadius);
    mapViewport->setStyleSheet(QString(
        "QFrame#caseBaseDataMapViewport {"
        "   background-color: #ffffff;"
        "   border-top-left-radius: %1px;"
        "   border-top-right-radius: %1px;"
        "   border: none;"
        "}"
    ).arg(rv));

    QVBoxLayout* vpLayout = new QVBoxLayout(mapViewport);
    vpLayout->setContentsMargins(0, 0, 0, 0);
    vpLayout->setSpacing(0);

    m_baseContentStack = new QStackedWidget(mapViewport);

    QWidget* renderPage = new QWidget(mapViewport);
    QVBoxLayout* renderLayout = new QVBoxLayout(renderPage);
    renderLayout->setContentsMargins(0, 0, 0, 0);
    renderLayout->setSpacing(0);

    m_baseMapWidget = new TNMapWidget(renderPage);
    m_baseMapWidget->setStyleContext(m_projectId, 0);
    CaseMapUtils::setupCaseSceneMap(m_baseMapWidget);
    renderLayout->addWidget(m_baseMapWidget, 1);

    m_baseLegendWidget = new CaseMapLegendWidget(m_baseMapWidget);
    m_baseLegendWidget->setFixedWidth(185);
    m_baseMapWidget->setBottomLeftOverlayWidget(m_baseLegendWidget, 10);
    m_baseLegendWidget->hide();

    m_baseContentStack->addWidget(renderPage);

    QWidget* dataPage = new QWidget(mapViewport);
    QVBoxLayout* dataLayout = new QVBoxLayout(dataPage);
    dataLayout->setContentsMargins(0, 0, 0, 0);
    dataLayout->setSpacing(0);
    m_baseAttributeWidget = new CaseMapAttributeWidget(dataPage);
    m_baseAttributeWidget->bindMap(m_baseMapWidget);
    dataLayout->addWidget(m_baseAttributeWidget, 1);
    m_baseContentStack->addWidget(dataPage);

    vpLayout->addWidget(m_baseContentStack, 1);

    shellLayout->addWidget(mapViewport, 1);
    shellLayout->addWidget(createBaseMapFooter(mapShell));

    m_baseMapToolbar = new CaseMapToolbarWidget(CaseMapToolbarWidget::HostContext::BaseData, host);
    m_baseMapToolbar->setSchemeModeSwitcherVisible(false);
    m_baseMapToolbar->setCompareSelectorVisible(false);
    host->setFloatingToolbar(m_baseMapToolbar);
    m_baseMapToolbarHost = host;

    outer->addWidget(mapShell, 1);
    hostLayout->addWidget(page, 1);

    layout->addWidget(host, 1);

    return wrapper;
}

QWidget* CasePanel::createBaseMapFooter(QWidget* parentWidget)
{
    QWidget* statusBar = new QWidget(parentWidget);
    statusBar->setAttribute(Qt::WA_StyledBackground, true);
    const QString br = QString::number(kDesignBaseMapViewportRadius);
    statusBar->setStyleSheet(QString(
        "QWidget {"
        "   background-color: %1;"
        "   border-bottom-left-radius: %2px;"
        "   border-bottom-right-radius: %2px;"
        "   border-top: 1px solid rgba(255,255,255,0.14);"
        "   border-left: none;"
        "   border-right: none;"
        "   border-bottom: none;"
        "}"
    ).arg(QLatin1String(kDesignBaseMapFooterBg), br));

    QHBoxLayout* layout = new QHBoxLayout(statusBar);
    layout->setContentsMargins(18, 11, 18, 13);
    layout->setSpacing(0);

    QFrame* segmentHost = new QFrame(statusBar);
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

    m_baseRenderButton = CaseMapFooter::createSegmentButton(QString::fromUtf8(u8"图层渲染"), true, segmentHost);
    m_baseDataButton = CaseMapFooter::createSegmentButton(QString::fromUtf8(u8"图层数据"), false, segmentHost);
    connect(m_baseRenderButton, &QPushButton::clicked, this, &CasePanel::onBaseRenderClicked);
    connect(m_baseDataButton, &QPushButton::clicked, this, &CasePanel::onBaseDataClicked);

    QButtonGroup* baseButtonGroup = new QButtonGroup(statusBar);
    baseButtonGroup->setExclusive(true);
    baseButtonGroup->addButton(m_baseRenderButton);
    baseButtonGroup->addButton(m_baseDataButton);

    segmentLayout->addWidget(m_baseRenderButton);
    segmentLayout->addWidget(m_baseDataButton);
    layout->addWidget(segmentHost, 0, Qt::AlignLeft | Qt::AlignVCenter);
    layout->addStretch(1);

    m_baseCrsStatusLabel = new QLabel(QString::fromUtf8(u8"坐标系：WGS84"), statusBar);
    m_baseCoordStatusLabel = new QLabel(QString::fromUtf8(u8"坐标：116.4074, 39.9042"), statusBar);
    m_baseScaleStatusLabel = new QLabel(QString::fromUtf8(u8"比例尺：1:150000"), statusBar);

    const QList<QLabel*> labels = {m_baseCrsStatusLabel, m_baseCoordStatusLabel, m_baseScaleStatusLabel};
    for (QLabel* label : labels) {
        label->setStyleSheet(CaseMapFooter::darkMapFooterLabelStyle());
    }
    m_baseCrsStatusLabel->setMinimumWidth(120);
    m_baseCoordStatusLabel->setMinimumWidth(180);
    m_baseScaleStatusLabel->setMinimumWidth(100);
    layout->addWidget(m_baseCrsStatusLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(32);
    layout->addWidget(m_baseCoordStatusLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(32);
    layout->addWidget(m_baseScaleStatusLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    if (m_baseMapWidget) {
        const MapStatus mapStatus = m_baseMapWidget->currentMapStatus();
        if (!mapStatus.crsText.isEmpty()) {
            m_baseCrsStatusLabel->setText(mapStatus.crsText);
        }
        if (!mapStatus.coordText.isEmpty()) {
            m_baseCoordStatusLabel->setText(mapStatus.coordText);
        }
        if (!mapStatus.scaleText.isEmpty()) {
            m_baseScaleStatusLabel->setText(mapStatus.scaleText);
        }

        connect(m_baseMapWidget, &TNMapWidget::mapStatusChanged, this, [this](const MapStatus& mapStatus) {
            if (m_baseCrsStatusLabel && !mapStatus.crsText.isEmpty()) {
                m_baseCrsStatusLabel->setText(mapStatus.crsText);
            }
            if (m_baseCoordStatusLabel && !mapStatus.coordText.isEmpty()) {
                m_baseCoordStatusLabel->setText(mapStatus.coordText);
            }
            if (m_baseScaleStatusLabel && !mapStatus.scaleText.isEmpty()) {
                m_baseScaleStatusLabel->setText(mapStatus.scaleText);
            }
        });
    }

    return statusBar;
}

void CasePanel::onBaseRenderClicked()
{
    if (m_baseRenderButton && m_baseDataButton) {
        m_baseRenderButton->setChecked(true);
        m_baseDataButton->setChecked(false);
    }
    if (m_baseContentStack) {
        m_baseContentStack->setCurrentIndex(0);
    }
    syncBaseFloatingToolbarForRenderDataView();
}

void CasePanel::switchToBaseDataView()
{
    if (m_baseRenderButton && m_baseDataButton) {
        m_baseRenderButton->setChecked(false);
        m_baseDataButton->setChecked(true);
    }
    if (m_baseContentStack) {
        m_baseContentStack->setCurrentIndex(1);
    }
    syncBaseFloatingToolbarForRenderDataView();
}

void CasePanel::onBaseDataClicked()
{
    switchToBaseDataView();
    if (m_baseAttributeWidget) {
        m_baseAttributeWidget->refreshFromCurrentLayer();
    }
}

void CasePanel::syncBaseFloatingToolbarForRenderDataView()
{
    if (!m_baseMapToolbar) {
        return;
    }

    bool showToolbar = true;
    if (m_baseContentStack) {
        showToolbar = (m_baseContentStack->currentIndex() == 0);
    }
    m_baseMapToolbar->setVisible(showToolbar);
    relayoutBaseMapToolbarHost(m_baseMapToolbarHost);
}

void CasePanel::refreshBaseAttributeFromMap()
{
    if (m_baseAttributeWidget) {
        m_baseAttributeWidget->clearColumnFiltersAndRefresh();
    }
}

void CasePanel::updateModeButtons()
{
    const bool schemeMode = m_workspaceContentStack->currentIndex() == 0;
    m_schemeTabButton->setChecked(schemeMode);
    m_baseDataTabButton->setChecked(!schemeMode);
}

void CasePanel::refreshCaseCards()
{
    if (!m_caseListLayout) {
        return;
    }

    while (QLayoutItem* child = m_caseListLayout->takeAt(0)) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    for (const CaseItem& caseItem : m_caseList) {
        CaseInfoWidget* card = new CaseInfoWidget(m_caseListWidget);
        card->setCaseInfo(caseItem);
        connect(card, &CaseInfoWidget::viewButtonClicked, this, &CasePanel::onCaseOpened);
        connect(card, &CaseInfoWidget::deleteButtonClicked, this, &CasePanel::onCaseDeleted);
        m_caseListLayout->addWidget(card);
    }
    m_caseListLayout->addStretch();
}

void CasePanel::refreshCasesFromGlobalState()
{
    QList<CaseItem> caseItems;
    const QList<CaseJson> rawCases = UIG().currentProjectCases();
    caseItems.reserve(rawCases.size());

    for (const CaseJson& raw : rawCases) {
        CaseItem item;
        item.id = raw.j_id();
        const QString rawName = raw.j_name();
        item.typeId = raw.j_case_type_id();
        item.typeName = caseTypeIdDisplayName(item.typeId);
        item.isBase = raw.j_isbase();
        item.name = caseDisplayName(rawName);
        item.projectId = raw.j_project_id();
        item.createTime = QDateTime::fromString(raw.j_created_at(), Qt::ISODate);
        if (!item.createTime.isValid()) {
            item.createTime = QDateTime::currentDateTime();
        }
        item.status = raw.j_status() == 1 ? QString::fromUtf8(u8"运行中") : QString::fromUtf8(u8"背景");

        const CaseCountJson count = raw.j_case_count();
        item.linkCount = count.j_road_way();
        item.nodeCount = count.j_road_point();
        item.zoneCount = count.j_road_community();
        item.odPairCount = count.j_other_od();
        item.remark = QString::fromUtf8(u8"");
        item.parameters.clear();
        item.parameters.insert(QStringLiteral("rawName"), rawName);

        QStringList roadLayers;
        const QList<QString> roadTypes = raw.j_road_types();
        for (const QString& rawType : roadTypes) {
            const QString normalized = normalizeFixedType(rawType);
            const QString display = roadLayerDisplayName(normalized);
            if (!display.isEmpty() && !roadLayers.contains(display)) {
                roadLayers.append(display);
            }
        }

        QStringList busLayers;
        const QList<QString> busTypes = raw.j_bus_types();
        for (const QString& rawType : busTypes) {
            const QString normalized = normalizeFixedType(rawType);
            const QString display = busLayerDisplayName(normalized);
            if (!display.isEmpty() && !busLayers.contains(display)) {
                busLayers.append(display);
            }
        }

        QStringList otherLayers;
        const QList<QString> otherTypes = raw.j_other_types();
        for (const QString& rawType : otherTypes) {
            const QString display = otherLayerDisplayName(rawType);
            if (!display.isEmpty() && !otherLayers.contains(display)) {
                otherLayers.append(display);
            }
        }

        item.parameters.insert(QStringLiteral("roadLayers"), roadLayers);
        item.parameters.insert(QStringLiteral("busLayers"), busLayers);
        item.parameters.insert(QStringLiteral("otherLayers"), otherLayers);

        item.caseLayerGroups = parseLayerGroupsFromCaseJson(raw);
        item.caseLayers = flattenLayerGroups(item.caseLayerGroups);

        caseItems.append(item);
    }

    setCaseList(caseItems);
}

void CasePanel::refreshCurrentCaseLayerTreeInPlace(std::function<void(bool synced)> onComplete)
{
    if (m_currentCaseId <= 0 || !m_detailPanel || !m_schemeWorkspaceStack || !m_caseModel) {
        if (onComplete) {
            onComplete(false);
        }
        return;
    }
    if (m_schemeWorkspaceStack->currentWidget() != m_detailPanel) {
        if (onComplete) {
            onComplete(false);
        }
        return;
    }

    const int caseId = m_currentCaseId;
    const CaseLayerTreeSnapshot snapshot = m_caseModel->captureLayerCheckStates(caseId);

    UIG().fetchCurrentProjectCasesQuiet([this, caseId, snapshot, onComplete](
            bool success, const QList<CaseJson>& cases, const QString& errorMessage) {
        Q_UNUSED(errorMessage)
        if (!success || !m_caseModel) {
            if (onComplete) {
                onComplete(false);
            }
            return;
        }

        CaseJson matched;
        bool found = false;
        for (const CaseJson& raw : cases) {
            if (raw.j_id() == caseId) {
                matched = raw;
                found = true;
                break;
            }
        }
        if (!found) {
            if (onComplete) {
                onComplete(false);
            }
            return;
        }

        const QList<LayerGroupItem> newGroups = parseLayerGroupsFromCaseJson(matched);
        bool layerListChanged = false;
        if (!m_caseModel->syncCaseLayerGroups(caseId, newGroups, snapshot, &layerListChanged)) {
            if (onComplete) {
                onComplete(false);
            }
            return;
        }

        if (layerListChanged) {
            CaseWfsMap::refreshWfsCapabilitiesCache();
        }

        for (CaseItem& item : m_caseList) {
            if (item.id == caseId) {
                item.caseLayerGroups = newGroups;
                item.caseLayers = flattenLayerGroups(newGroups);
                for (LayerItem& layer : item.caseLayers) {
                    const QString tn = layer.wfsTypeName.trimmed();
                    if (snapshot.checkByTypename.contains(tn)) {
                        layer.checked = snapshot.checkByTypename.value(tn) == Qt::Checked;
                    }
                }
                break;
            }
        }

        scheduleProjectSchemaWarm(true, caseId);

        if (onComplete) {
            onComplete(true);
        }
    });
}

void CasePanel::loadAuxiliaryLayersOnSchemeMap(TNMapWidget* map, int caseId,
                                               const QList<CaseAuxLayer::AuxLayerTarget>& targets)
{
    Q_UNUSED(map)
    if (!m_caseModel || caseId <= 0) {
        return;
    }

    for (const CaseAuxLayer::AuxLayerTarget& target : targets) {
        const QString typeName = target.typeName.trimmed();
        if (typeName.isEmpty()) {
            continue;
        }
        if (!m_caseModel->setLayerCheckedByTypename(caseId, typeName, true)) {
            qWarning() << QString::fromUtf8(u8"辅助图层节点尚未出现在方案树中：%1").arg(typeName);
        }
    }
}

void CasePanel::loadAuxiliaryLayersOnBaseMap(TNMapWidget* map, const QList<CaseAuxLayer::AuxLayerTarget>& targets)
{
    Q_UNUSED(map)
    if (!m_baseDataView) {
        return;
    }

    for (const CaseAuxLayer::AuxLayerTarget& target : targets) {
        const QString typeName = target.typeName.trimmed();
        if (typeName.isEmpty()) {
            continue;
        }
        if (!m_baseDataView->setLayerCheckedByTypename(typeName, true)) {
            qWarning() << QString::fromUtf8(u8"辅助图层节点尚未出现在基础数据树中：%1").arg(typeName);
        }
    }
}

void CasePanel::refreshBaseDataLayerTreeInPlace(std::function<void(bool synced)> onComplete)
{
    if (!m_baseDataView) {
        if (onComplete) {
            onComplete(false);
        }
        return;
    }
    if (m_workspaceContentStack && m_workspaceContentStack->currentIndex() != 1) {
        if (onComplete) {
            onComplete(false);
        }
        return;
    }

    const BaseDataLayerTreeSnapshot snapshot = m_baseDataView->captureLayerCheckStates();

    UIG().fetchCurrentProjectDetailQuiet([this, snapshot, onComplete](bool success, const QString& errorMessage) {
        Q_UNUSED(errorMessage)
        if (!success || !m_baseDataView) {
            if (onComplete) {
                onComplete(false);
            }
            return;
        }

        BaseDataLayerTreeSnapshot filteredSnapshot = snapshot;
        if (m_baseMapWidget) {
            for (auto it = filteredSnapshot.checkByTypename.begin(); it != filteredSnapshot.checkByTypename.end(); ++it) {
                if (it.value() != Qt::Checked) {
                    continue;
                }
                const QString uri = CaseWfsMap::buildWfsDataSourceUri(it.key());
                if (!m_baseMapWidget->getLayer(uri)) {
                    it.value() = Qt::Unchecked;
                }
            }
        }

        const QList<BaseDataLayerGroupNodeItem> newGroups = buildBaseLayerGroupNodes();
        bool layerListChanged = false;
        if (!m_baseDataView->syncLayerGroups(newGroups, filteredSnapshot, &layerListChanged)) {
            if (onComplete) {
                onComplete(false);
            }
            return;
        }

        m_lastBaseDataTypenames = collectBaseDataTypenames(newGroups);
        m_hasBaseDataTypenamesSnapshot = true;

        if (layerListChanged) {
            CaseWfsMap::refreshWfsCapabilitiesCache();
        }
        scheduleProjectSchemaWarm(false, -1);

        if (onComplete) {
            onComplete(true);
        }
    });
}

QSet<QString> CasePanel::collectBaseDataTypenames(const QList<BaseDataLayerGroupNodeItem>& groups) const
{
    QSet<QString> typenames;
    for (const BaseDataLayerGroupNodeItem& group : groups) {
        for (const BaseDataLayerNodeItem& layer : group.layers) {
            const QString tn =
                layer.userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (!tn.isEmpty()) {
                typenames.insert(tn);
            }
        }
    }
    return typenames;
}

QStringList CasePanel::collectCaseTypenames(int caseId) const
{
    QStringList typenames;
    QSet<QString> seen;
    for (const CaseItem& item : m_caseList) {
        if (caseId > 0 && item.id != caseId) {
            continue;
        }
        for (const LayerItem& layer : item.caseLayers) {
            const QString tn = layer.wfsTypeName.trimmed();
            if (tn.isEmpty() || seen.contains(tn)) {
                continue;
            }
            seen.insert(tn);
            typenames.append(tn);
        }
    }
    return typenames;
}

void CasePanel::scheduleProjectSchemaWarm(bool prioritizeCaseId, int caseId)
{
    const QStringList baseTypenames(m_lastBaseDataTypenames.begin(), m_lastBaseDataTypenames.end());

    if (prioritizeCaseId && caseId > 0) {
        // Entering a case: warm that case first; keep base data warm, never scan all cases.
        CaseWfsMap::scheduleSchemaWarm(collectCaseTypenames(caseId), true);
        CaseWfsMap::scheduleSchemaWarm(baseTypenames, false);
        return;
    }

    // Project open / base-tree rebuild: base-data typenames only (keeps list UI responsive).
    CaseWfsMap::scheduleSchemaWarm(baseTypenames, false);
}

void CasePanel::refreshWfsServiceInfo(bool isBaseDataMap)
{
    if (m_wfsRefreshBusy) {
        return;
    }

    CaseMapToolbarWidget* toolbar = nullptr;
    if (isBaseDataMap) {
        toolbar = m_baseMapToolbar;
    } else if (m_detailPanel) {
        toolbar = m_detailPanel->schemeToolbarWidget();
    }

    m_wfsRefreshBusy = true;
    if (toolbar) {
        toolbar->setRefreshDataBusy(true);
    }

    QPointer<CasePanel> self(this);
    CaseWfsMap::setCapabilitiesWarmFinishedCallback([self, isBaseDataMap](bool succeeded) {
        if (!self) {
            return;
        }
        self->m_wfsRefreshBusy = false;
        CaseMapToolbarWidget* doneToolbar = nullptr;
        if (isBaseDataMap) {
            doneToolbar = self->m_baseMapToolbar;
        } else if (self->m_detailPanel) {
            doneToolbar = self->m_detailPanel->schemeToolbarWidget();
        }
        if (doneToolbar) {
            doneToolbar->setRefreshDataBusy(false);
        }
        if (!succeeded) {
            TnUi::showWarning(QString::fromUtf8(u8"刷新数据"),
                              QString::fromUtf8(u8"WFS 服务信息刷新失败，请稍后重试。"));
            return;
        }
        if (isBaseDataMap) {
            self->scheduleProjectSchemaWarm(false, -1);
        } else {
            self->scheduleProjectSchemaWarm(true, self->m_currentCaseId);
        }
    });
    CaseWfsMap::forceRefreshWfsCapabilitiesCache();
}

void CasePanel::refreshBaseDataTreeFromProjectDetail()
{
    if (!m_baseDataView) {
        return;
    }
    if (m_baseMapWidget) {
        CaseWfsMap::removePanelWfsLayers(m_baseMapWidget);
    }

    const QList<BaseDataLayerGroupNodeItem> groups = buildBaseLayerGroupNodes();
    const QSet<QString> newTypenames = collectBaseDataTypenames(groups);
    const bool layerListChanged =
        m_hasBaseDataTypenamesSnapshot && (newTypenames != m_lastBaseDataTypenames);
    // Force only when typenames really changed. If Caps warm not yet succeeded, ensure (non-force)
    // so ProjectOpenReady does not restart an in-flight cold start and double the wait.
    const bool needCapsForceRefresh = layerListChanged;
    const bool needCapsEnsure = !layerListChanged && !CaseWfsMap::isCapabilitiesWarmSucceeded();

    m_baseDataView->setTreeData(groups, buildBaseToolNodes());
    m_lastBaseDataTypenames = newTypenames;
    m_hasBaseDataTypenamesSnapshot = true;

    QTimer::singleShot(0, this, [this, needCapsForceRefresh, needCapsEnsure]() {
        if (needCapsForceRefresh) {
            CaseWfsMap::refreshWfsCapabilitiesCache();
        } else if (needCapsEnsure) {
            CaseWfsMap::ensureCapabilitiesWarm();
        }
        scheduleProjectSchemaWarm(false, -1);
    });
}

void CasePanel::reloadBaseDataMenusFromApi(std::function<void(bool success)> onComplete)
{
    UIG().fetchCurrentProjectDetailQuiet([this, onComplete](bool success, const QString& errorMessage) {
        Q_UNUSED(errorMessage)
        if (!success) {
            if (onComplete) {
                onComplete(false);
            }
            return;
        }
        refreshBaseDataTreeFromProjectDetail();
        if (onComplete) {
            onComplete(true);
        }
    });
}

QList<BaseDataLayerGroupNodeItem> CasePanel::buildBaseLayerGroupNodes() const
{
    const QSharedPointer<ProjectDetailJson> detail = UIG().currentProjectDetail();
    if (detail.isNull()) {
        return {};
    }
    return buildBaseDataLayerGroups(detail->j_menu());
}

QList<BaseDataToolNodeItem> CasePanel::buildBaseToolNodes() const
{
    QList<BaseDataToolNodeItem> toolItems;
    const QSharedPointer<ProjectDetailJson> detail = UIG().currentProjectDetail();
    if (detail.isNull()) {
        return toolItems;
    }

    for (const ProjectToolJson& tool : detail->j_tools()) {
        const QString name = tool.j_name().trimmed();
        if (name.isEmpty()) {
            continue;
        }
        BaseDataToolNodeItem node;
        node.displayName = name;
        node.userData.insert(baseDataUserDataKey(BaseDataUserDataField::Type), tool.j_type());
        node.userData.insert(baseDataUserDataKey(BaseDataUserDataField::Url), tool.j_url());
        toolItems.append(node);
    }

    return toolItems;
}

bool CasePanel::isSchemeDetailViewActive() const
{
    return m_schemeWorkspaceStack && m_schemeWorkspaceStack->currentWidget() == m_detailPanel;
}

int CasePanel::schemeWorkspaceCurrentIndex() const
{
    return m_schemeWorkspaceStack ? m_schemeWorkspaceStack->currentIndex() : -1;
}

void CasePanel::resetSchemeWorkspaceNavigation(const char* reason)
{
    qDebug() << "[CasePanelNav] resetSchemeWorkspaceNavigation"
             << (reason ? reason : "unspecified")
             << "fromIndex=" << schemeWorkspaceCurrentIndex();
    m_savedSchemeWorkspaceIndex = 0;
    if (m_schemeWorkspaceStack) {
        m_schemeWorkspaceStack->setCurrentIndex(0);
    }
}

void CasePanel::onSchemeListReturnRequested()
{
    resetSchemeWorkspaceNavigation("userReturn");
}

void CasePanel::refreshProjectCasesAndBaseData(bool clearDetailWfs)
{
    if (clearDetailWfs) {
        clearDetailPanelWfsLayers();
    }
    refreshCasesFromGlobalState();
    refreshBaseDataTreeFromProjectDetail();
}

void CasePanel::refreshProjectCasesInPlace()
{
    qDebug() << "[CasePanelNav] refreshProjectCasesInPlace inDetail=" << isSchemeDetailViewActive()
             << "stackIndex=" << schemeWorkspaceCurrentIndex()
             << "currentCaseId=" << m_currentCaseId;
    refreshProjectCasesAndBaseData(!isSchemeDetailViewActive());
}

void CasePanel::applyProjectOpenState(int stateValue, const QString& errorMessage)
{
    qDebug() << "[CasePanelNav] applyProjectOpenState state=" << stateValue
             << "inDetail=" << isSchemeDetailViewActive()
             << "stackIndex=" << schemeWorkspaceCurrentIndex();

    if (stateValue == UIGlobalState::ProjectOpenReady) {
        const bool inDetail = isSchemeDetailViewActive();
        refreshProjectCasesAndBaseData(!inDetail);
        if (!inDetail) {
            resetSchemeWorkspaceNavigation("ProjectOpenReady");
        }
        return;
    }

    if (stateValue == UIGlobalState::ProjectOpenPartialReady) {
        const bool inDetail = isSchemeDetailViewActive();
        refreshProjectCasesAndBaseData(!inDetail);
        if (!inDetail) {
            resetSchemeWorkspaceNavigation("ProjectOpenPartialReady");
        }
        const QString message = errorMessage.trimmed();
        if (!message.isEmpty()) {
            TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
        }
        return;
    }

    if (stateValue == UIGlobalState::ProjectOpenLoading) {
        return;
    }

    if (stateValue == UIGlobalState::ProjectOpenError) {
        resetSchemeWorkspaceNavigation("ProjectOpenError");
        if (m_baseMapWidget) {
            CaseWfsMap::removePanelWfsLayers(m_baseMapWidget);
        }
        setCaseList({});
        if (m_baseDataView) {
            m_baseDataView->clearTreeData();
        }
        const QString message = errorMessage.trimmed();
        if (!message.isEmpty()) {
            TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
        }
        return;
    }
}

void CasePanel::onUiEventNotify(int uiEvent, const QVariantList& params)
{
    if (m_projectId <= 0 || params.isEmpty()) {
        return;
    }

    if (uiEvent != UIGlobalState::ProjectOpenStateChanged
        && uiEvent != UIGlobalState::CurrentProjectCasesChanged) {
        return;
    }

    const QVariantMap payload = params.first().toMap();
    const int projectId = payload.value(QStringLiteral("projectId")).toInt();
    if (projectId != m_projectId) {
        return;
    }

    const int stateValue = payload.value(QStringLiteral("state")).toInt();
    const QString errorMessage = payload.value(QStringLiteral("error")).toString();

    qDebug() << "[CasePanelNav] onUiEventNotify event=" << uiEvent
             << "state=" << stateValue
             << "projectId=" << projectId
             << "stackIndex=" << schemeWorkspaceCurrentIndex();

    if (uiEvent == UIGlobalState::ProjectOpenStateChanged) {
        applyProjectOpenState(stateValue, errorMessage);
        return;
    }

    if (stateValue == UIGlobalState::ProjectOpenReady
        || stateValue == UIGlobalState::ProjectOpenPartialReady) {
        refreshProjectCasesInPlace();
        if (stateValue == UIGlobalState::ProjectOpenPartialReady) {
            const QString message = errorMessage.trimmed();
            if (!message.isEmpty()) {
                TnUi::showWarning(QString::fromUtf8(u8"提示"), message);
            }
        }
        return;
    }

    if (stateValue == UIGlobalState::ProjectOpenError) {
        applyProjectOpenState(stateValue, errorMessage);
    }
}

void CasePanel::openCaseById(int caseId)
{
    for (const CaseItem& item : m_caseList) {
        if (item.id == caseId) {
            m_currentCaseId = caseId;
            m_detailPanel->setCaseInfo(item);
            if (TNMapWidget* map = m_detailPanel->standardMapWidget()) {
                map->setStyleContext(m_projectId, caseId);
            }
            m_detailPanel->setToolbarViewMode(CaseMapToolbarWidget::ViewMode::Standard);
            CaseWfsMap::clearLoadQueueIdleCallback();
            CaseWfsMap::removePanelWfsLayers(m_detailPanel->leftCompareMapWidget());
            CaseWfsMap::removePanelWfsLayers(m_detailPanel->rightCompareMapWidget());
            // Prefer schema warm for this case before any checked-layer sync creates providers.
            scheduleProjectSchemaWarm(true, caseId);
            syncSchemeDetailWfsFromCaseTree();
            resetSchemeDetailLegendToDefault();
            if (m_schemeWorkspaceStack) {
                m_schemeWorkspaceStack->setCurrentWidget(m_detailPanel);
                m_savedSchemeWorkspaceIndex = m_schemeWorkspaceStack->currentIndex();
            }
            if (m_caseTreeView) {
                m_caseTreeView->focusCaseNode(caseId);
            }
            return;
        }
    }
}

CompareCaseItem CasePanel::toCompareItem(const CaseItem& item) const
{
    CompareCaseItem compareItem;
    compareItem.id = item.id;
    compareItem.name = item.name;
    compareItem.projectName = m_projectName;
    compareItem.createTime = item.createTime;
    return compareItem;
}

int CasePanel::findBaseCaseId() const
{
    for (const CaseItem& item : m_caseList) {
        if (item.isBase == 1) {
            return item.id;
        }
    }
    return -1;
}

bool CasePanel::ensureBaseCaseDetailForCompare(int baseCaseId)
{
    if (baseCaseId <= 0 || !m_detailPanel) {
        return false;
    }

    if (!isSchemeDetailViewActive() || m_currentCaseId != baseCaseId) {
        openCaseById(baseCaseId);
    }

    if (CaseMapToolbarWidget* toolbar = m_detailPanel->schemeToolbarWidget()) {
        if (toolbar->viewMode() == CaseMapToolbarWidget::ViewMode::Compare) {
            m_detailPanel->setToolbarViewMode(CaseMapToolbarWidget::ViewMode::Standard);
        }
    }

    return m_detailPanel->standardMapWidget() != nullptr;
}

void CasePanel::onBaseDataToolNodeClicked(const QString& toolName, const QVariantMap& userData)
{
    const QString typeText = userData.value(baseDataUserDataKey(BaseDataUserDataField::Type)).toString();
    const QString urlText = userData.value(baseDataUserDataKey(BaseDataUserDataField::Url)).toString().trimmed();
    const BaseDataToolType toolType = baseDataToolTypeFromString(typeText);
    const QString throttleKey = QStringLiteral("%1|%2|%3").arg(typeText, toolName, urlText);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (throttleKey == m_lastToolOpenKey && (nowMs - m_lastToolOpenAtMs) < kToolDoubleClickThrottleMs) {
        return;
    }
    m_lastToolOpenKey = throttleKey;
    m_lastToolOpenAtMs = nowMs;

    switch (toolType) {
        case BaseDataToolType::Url:
            openWebViewUrlOrWarn(this, toolName, urlText);
            return;
        case BaseDataToolType::OpenPop:
            runBaseDataPopGeneration(toolName);
            return;
        case BaseDataToolType::OpenOd:
            runBaseDataOdGeneration(toolName);
            return;
        case BaseDataToolType::OpenDeduce:
            runBaseDataOdEstimation(toolName);
            return;
        case BaseDataToolType::OpenHtml:
            runBaseDataNetworkAnalysis(toolName);
            return;
        case BaseDataToolType::OpenArea:
            runBaseDataCommunityGeneration(toolName);
            return;
        case BaseDataToolType::Open:
            if (urlText == QStringLiteral("analysis")) {
                runBaseDataNetworkAnalysis(toolName);
            }
            return;
        default:
            return;
    }
}

void CasePanel::runBaseDataOdGeneration(const QString& dialogTitle)
{
    const QString title = dialogTitle.trimmed().isEmpty()
        ? QString::fromUtf8(u8"OD数据生成")
        : dialogTitle.trimmed();
    if (m_projectId <= 0) {
        TnUi::showWarning(title, QString::fromUtf8(u8"当前项目无效。"));
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(title, QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    QString odMode;
    if (!CaseOdTypeDialog::execSelect(&odMode, nullptr)) {
        return;
    }

    QJsonObject odModeJson;
    odModeJson.insert(QStringLiteral("od_mode"), odMode);
    const QString odModeJsonString =
        QString::fromUtf8(QJsonDocument(odModeJson).toJson(QJsonDocument::Compact));

    auto* loading = new LoadingDialog(title + QString::fromUtf8(u8"中…"), nullptr);
    loading->showNonBlocking();

    QPointer<CasePanel> self(this);
    HttpApis::grpcRunAlg(token,
                         m_projectId,
                         0,
                         QStringLiteral("tool"),
                         QStringLiteral("trip_model_all"),
                         odModeJsonString,
                         [self, loading, title](bool suc,
                                                const QSharedPointer<GrpcRunAlgResultJson>& res,
                                                const QString& errMsg) {
                             if (loading) {
                                 loading->closeDialog();
                                 loading->deleteLater();
                             }
                             if (!self) {
                                 return;
                             }
                             const QString failText = QString::fromUtf8(u8"算法运行失败");
                             if (!suc || res.isNull()) {
                                 TnUi::showWarning(title, errMsg.trimmed().isEmpty() ? failText : errMsg.trimmed());
                                 return;
                             }

                             const QJsonObject dataObj = res->jsonObject();
                             const int bizCode = dataObj.value(QStringLiteral("code")).toInt(0);
                             if (bizCode != 1) {
                                 const QString bizMessage =
                                     dataObj.value(QStringLiteral("message")).toString().trimmed();
                                 TnUi::showWarning(title, bizMessage.isEmpty() ? failText : bizMessage);
                                 return;
                             }

                             self->ensureOtherOdLayerLoadedAndStyled([self, title](bool ok, const QString& err) {
                                 if (!self) {
                                     return;
                                 }
                                 if (!ok) {
                                     TnUi::showWarning(title, err);
                                     return;
                                 }
                                 TnUi::showInformation(title,
                                     QString::fromUtf8(u8"OD 数据已生成，地图已加载并应用渲染样式。"));
                             });
                         });
}

void CasePanel::runBaseDataPopGeneration(const QString& dialogTitle)
{
    const QString title = dialogTitle.trimmed().isEmpty()
        ? QString::fromUtf8(u8"生成人口数据")
        : dialogTitle.trimmed();
    if (m_projectId <= 0) {
        TnUi::showWarning(title, QString::fromUtf8(u8"当前项目无效。"));
        return;
    }
    if (!m_baseDataView || !m_baseMapWidget) {
        TnUi::showWarning(title, QString::fromUtf8(u8"基础数据视图未就绪。"));
        return;
    }

    QVariantMap communityUserData;
    if (!m_baseDataView->findLayerUserDataByValAndGroup(kRoadCommunityLayerVal,
                                                        kRoadLayerGroupValue,
                                                        &communityUserData)) {
        TnUi::showWarning(title,
                          QString::fromUtf8(u8"未找到道路组下的 road_community 图层，请确认项目菜单配置。"));
        return;
    }

    const QString typeName =
        communityUserData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
    const QString displayName =
        communityUserData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString().trimmed();
    if (typeName.isEmpty()) {
        TnUi::showWarning(title,
                          QString::fromUtf8(u8"道路小区图层缺少 WFS 类型名（value）。"));
        return;
    }

    const QString rasterPath = CasePopZonalStatHelper::defaultPopRasterPath();
    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(title, QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    m_popGenTypeName = typeName;
    m_popGenDisplayName = displayName;
    m_popGenRasterPath = rasterPath;
    m_popGenToken = token;
    m_popGenTitle = title;

    QPointer<CasePanel> self(this);
    CaseWfsMap::prepareBatchWfsLayerLoad();
    CaseWfsMap::setLoadQueueIdleCallback([self]() {
        if (!self) {
            return;
        }
        QTimer::singleShot(0, self, &CasePanel::openPopGenerationDialog);
    });

    if (!m_baseDataView->setLayerCheckedByTypename(typeName, true)) {
        CaseWfsMap::clearLoadQueueIdleCallback();
        TnUi::showWarning(title,
                          QString::fromUtf8(u8"无法勾选道路小区图层。"));
    }
}

void CasePanel::openPopGenerationDialog()
{
    if (!m_baseMapWidget) {
        return;
    }

    const QString uriStr = CaseWfsMap::buildWfsDataSourceUri(m_popGenTypeName);
    auto* communityLayer = qobject_cast<QgsVectorLayer*>(m_baseMapWidget->getLayer(uriStr));
    if (!communityLayer || !communityLayer->isValid()) {
        TnUi::showWarning(m_popGenTitle,
                          QString::fromUtf8(u8"道路小区图层加载失败，请重试。"));
        return;
    }

    CasePopGenerationDialog dialog(communityLayer,
                                   m_popGenDisplayName,
                                   m_popGenTypeName,
                                   m_popGenRasterPath,
                                   m_projectId,
                                   m_popGenToken,
                                   nullptr);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_popGenSuccessMessage = dialog.successMessage().trimmed();
    QTimer::singleShot(0, this, &CasePanel::finishPopGenerationSuccess);
}

void CasePanel::finishPopGenerationSuccess()
{
    const QString infoMessage = m_popGenSuccessMessage.trimmed();
    TnUi::showInformation(m_popGenTitle,
                          infoMessage.isEmpty() ? QString::fromUtf8(u8"操作成功") : infoMessage);

    const QString typeName = m_popGenTypeName;
    QPointer<CasePanel> self(this);
    refreshBaseDataLayerTreeInPlace([self, typeName](bool /*synced*/) {
        CaseWfsMap::refreshWfsCapabilitiesCache();
        if (!self || !self->m_baseMapWidget) {
            return;
        }
        const QString uri = CaseWfsMap::buildWfsDataSourceUri(typeName);
        if (auto* layer = qobject_cast<QgsVectorLayer*>(self->m_baseMapWidget->getLayer(uri))) {
            layer->reload();
        }
    });
}

void CasePanel::runBaseDataCommunityGeneration(const QString& dialogTitle)
{
    const QString title = dialogTitle.trimmed().isEmpty()
        ? QString::fromUtf8(u8"生成交通小区")
        : dialogTitle.trimmed();
    if (m_projectId <= 0) {
        TnUi::showWarning(title, QString::fromUtf8(u8"当前项目无效。"));
        return;
    }
    if (!m_baseDataView || !m_baseMapWidget) {
        TnUi::showWarning(title, QString::fromUtf8(u8"基础数据视图未就绪。"));
        return;
    }

    QVariantMap roadWayUserData;
    if (!m_baseDataView->findLayerUserDataByValAndGroup(kRoadWayLayerVal,
                                                        kRoadLayerGroupValue,
                                                        &roadWayUserData)) {
        TnUi::showWarning(title,
                          QString::fromUtf8(u8"未找到道路组下的 road_way 图层，请确认项目菜单配置。"));
        return;
    }

    const QString roadWayTypeName =
        roadWayUserData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
    const QString roadWayDisplayName =
        roadWayUserData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString().trimmed();
    if (roadWayTypeName.isEmpty()) {
        TnUi::showWarning(title,
                          QString::fromUtf8(u8"road_way 图层缺少 WFS 类型名（value）。"));
        return;
    }

    QString communityTypeName;
    QString communityDisplayName = QStringLiteral("road_community");
    QVariantMap communityUserData;
    if (m_baseDataView->findLayerUserDataByValAndGroup(kRoadCommunityLayerVal,
                                                       kRoadLayerGroupValue,
                                                       &communityUserData)) {
        communityTypeName =
            communityUserData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
        const QString displayName =
            communityUserData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString().trimmed();
        if (!displayName.isEmpty()) {
            communityDisplayName = displayName;
        }
    }

    const QString rasterPath = CasePopZonalStatHelper::defaultPopRasterPath();
    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(title, QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    m_communityGenRoadWayTypeName = roadWayTypeName;
    m_communityGenRoadWayDisplayName = roadWayDisplayName;
    m_communityGenCommunityTypeName = communityTypeName;
    m_communityGenTargetTableDisplay = communityDisplayName;
    m_communityGenRasterPath = rasterPath;
    m_communityGenToken = token;
    m_communityGenTitle = title;

    QPointer<CasePanel> self(this);
    CaseWfsMap::prepareBatchWfsLayerLoad();
    CaseWfsMap::setLoadQueueIdleCallback([self]() {
        if (!self) {
            return;
        }
        QTimer::singleShot(0, self, &CasePanel::openCommunityGenerationDialog);
    });

    if (!m_baseDataView->setLayerCheckedByTypename(roadWayTypeName, true)) {
        CaseWfsMap::clearLoadQueueIdleCallback();
        TnUi::showWarning(title,
                          QString::fromUtf8(u8"无法勾选 road_way 图层。"));
    }
}

void CasePanel::openCommunityGenerationDialog()
{
    if (!m_baseMapWidget) {
        return;
    }

    const QString uriStr = CaseWfsMap::buildWfsDataSourceUri(m_communityGenRoadWayTypeName);
    auto* roadWayLayer = qobject_cast<QgsVectorLayer*>(m_baseMapWidget->getLayer(uriStr));
    if (!roadWayLayer || !roadWayLayer->isValid()) {
        TnUi::showWarning(m_communityGenTitle,
                          QString::fromUtf8(u8"road_way 图层加载失败，请重试。"));
        return;
    }

    CaseCommunityGenerationDialog dialog(roadWayLayer,
                                         m_projectName,
                                         m_communityGenRoadWayDisplayName,
                                         m_communityGenRoadWayTypeName,
                                         m_communityGenTargetTableDisplay,
                                         m_communityGenRasterPath,
                                         m_projectId,
                                         m_communityGenToken,
                                         nullptr);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_communityGenSuccessMessage = dialog.successMessage().trimmed();
    QTimer::singleShot(0, this, &CasePanel::finishCommunityGenerationSuccess);
}

void CasePanel::finishCommunityGenerationSuccess()
{
    const QString infoMessage = m_communityGenSuccessMessage.trimmed();
    TnUi::showInformation(m_communityGenTitle,
                          infoMessage.isEmpty() ? QString::fromUtf8(u8"操作成功") : infoMessage);

    const QString communityTypeName = m_communityGenCommunityTypeName;
    QPointer<CasePanel> self(this);
    refreshBaseDataLayerTreeInPlace([self, communityTypeName](bool /*synced*/) {
        CaseWfsMap::refreshWfsCapabilitiesCache();
        if (!self || !self->m_baseMapWidget || communityTypeName.trimmed().isEmpty()) {
            return;
        }

        const QString uri = CaseWfsMap::buildWfsDataSourceUri(communityTypeName);
        if (auto* layer = qobject_cast<QgsVectorLayer*>(self->m_baseMapWidget->getLayer(uri))) {
            layer->reload();
            return;
        }

        if (self->m_baseDataView) {
            self->m_baseDataView->setLayerCheckedByTypename(communityTypeName, true);
        }
    });
}

void CasePanel::runBaseDataNetworkAnalysis(const QString& dialogTitle)
{
    const QString title = dialogTitle.trimmed().isEmpty()
        ? QString::fromUtf8(u8"基础数据分析")
        : dialogTitle.trimmed();
    if (m_projectId <= 0) {
        TnUi::showWarning(title, QString::fromUtf8(u8"当前项目无效。"));
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(title, QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    auto* loading = new LoadingDialog(title + QString::fromUtf8(u8"加载中…"), nullptr);
    loading->showNonBlocking();

    QPointer<CasePanel> self(this);
    HttpApis::caseBaseDataView(token,
                               m_projectId,
                               [loading, self, title](bool suc,
                                                      const QSharedPointer<CaseReportViewJson>& res,
                                                      const QString& errMsg) {
                                   if (loading) {
                                       loading->closeDialog();
                                       loading->deleteLater();
                                   }
                                   if (!suc || res.isNull()) {
                                       TnUi::showWarning(title,
                                           errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"加载失败") : errMsg.trimmed());
                                       return;
                                   }

                                   openHtmlReportWithFallback(self ? self->window() : nullptr,
                                                              title,
                                                              res->j_content().trimmed(),
                                                              QStringLiteral("base_data_analysis.html"));
                               });
}

void CasePanel::runBaseDataOdEstimation(const QString& dialogTitle)
{
    const QString title = dialogTitle.trimmed().isEmpty()
        ? QString::fromUtf8(u8"OD反推")
        : dialogTitle.trimmed();
    if (m_projectId <= 0) {
        TnUi::showWarning(title, QString::fromUtf8(u8"当前项目无效。"));
        return;
    }

    const QSharedPointer<LoginResponseJson> loginInfo = UIG().loginInfo();
    const QString token = loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
    if (token.isEmpty()) {
        TnUi::showWarning(title, QString::fromUtf8(u8"登录信息缺失，请重新登录后重试。"));
        return;
    }

    OdEstimationDialog dialog(m_projectId, token, this);
    if (!dialog.loadAlgorithm()) {
        return;
    }

    struct OdEstMapResult {
        bool runSucceeded = false;
        bool styleFinished = false;
        bool styleOk = false;
        QString styleError;
    };

    OdEstMapResult mapResult;
    QPointer<CasePanel> self(this);
    QEventLoop* styleWaitLoop = nullptr;

    connect(&dialog, &OdEstimationDialog::runSucceeded, self.data(), [self, &mapResult, &styleWaitLoop]() {
        mapResult.runSucceeded = true;
        if (!self) {
            return;
        }
        self->ensureOtherOdLayerLoadedAndStyled([&mapResult, &styleWaitLoop](bool ok, const QString& err) {
            mapResult.styleFinished = true;
            mapResult.styleOk = ok;
            mapResult.styleError = err;
            if (styleWaitLoop) {
                styleWaitLoop->quit();
            }
        });
    });

    dialog.exec();

    if (!mapResult.runSucceeded || !self) {
        return;
    }

    if (!mapResult.styleFinished) {
        QEventLoop loop;
        styleWaitLoop = &loop;
        loop.exec();
        styleWaitLoop = nullptr;
    }

    if (mapResult.styleOk) {
        TnUi::showInformation(title,
                              QString::fromUtf8(u8"OD反推完成，地图已加载并应用渲染样式。"));
    } else {
        const QString message = mapResult.styleError.trimmed().isEmpty()
            ? QString::fromUtf8(u8"地图加载或渲染样式应用失败。")
            : mapResult.styleError.trimmed();
        TnUi::showWarning(title, message);
    }
}

void CasePanel::ensureOtherOdLayerLoadedAndStyled(const std::function<void(bool success, const QString& error)>& done)
{
    const auto finish = [done](bool ok, const QString& err) {
        if (done) {
            done(ok, err);
        }
    };

    if (!m_baseDataView || !m_baseMapWidget) {
        finish(false, QString::fromUtf8(u8"基础数据视图未就绪。"));
        return;
    }

    QVariantMap odUserData;
    if (!m_baseDataView->findLayerUserDataByVal(kOtherOdLayerVal, &odUserData)) {
        finish(false, QString::fromUtf8(u8"未找到 other_od 图层，请确认项目菜单配置。"));
        return;
    }

    const QString typeName =
        odUserData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
    if (typeName.isEmpty()) {
        finish(false, QString::fromUtf8(u8"OD 图层缺少 WFS 类型名（value）。"));
        return;
    }

    BaseDataTableView* baseDataView = m_baseDataView;
    const auto triggerLoad = [baseDataView]() {
        return baseDataView->setLayerCheckedByVal(kOtherOdLayerVal, true, nullptr);
    };

    CaseOtherOdMapHelper::loadOrReloadAndApplyDemandStyle(m_baseMapWidget, typeName, triggerLoad, finish);
}

void CasePanel::applyOtherOdStyleAfterDesireline(TNMapWidget* map,
                                                 bool isBaseDataMap,
                                                 int caseId,
                                                 const std::function<void(bool success, const QString& error)>& done)
{
    const auto finish = [done](bool ok, const QString& err) {
        if (done) {
            done(ok, err);
        }
    };

    if (!map) {
        finish(false, QString::fromUtf8(u8"地图未就绪。"));
        return;
    }

    QString typeName;
    std::function<bool()> triggerLoad;

    if (isBaseDataMap) {
        if (!m_baseDataView) {
            finish(false, QString::fromUtf8(u8"基础数据视图未就绪。"));
            return;
        }

        QVariantMap odUserData;
        if (!m_baseDataView->findLayerUserDataByVal(kOtherOdLayerVal, &odUserData)) {
            finish(false, QString::fromUtf8(u8"未找到 other_od 图层，请确认项目菜单配置。"));
            return;
        }

        typeName = odUserData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
        BaseDataTableView* baseDataView = m_baseDataView;
        triggerLoad = [baseDataView]() {
            return baseDataView->setLayerCheckedByVal(kOtherOdLayerVal, true, nullptr);
        };
    } else {
        if (!m_caseModel || caseId <= 0) {
            finish(false, QString::fromUtf8(u8"方案图层树未就绪。"));
            return;
        }

        QVariantMap odMeta;
        if (!m_caseModel->findLayerUserDataByVal(caseId, kOtherOdLayerVal, &odMeta)) {
            finish(false, QString::fromUtf8(u8"未找到 other_od 图层，请确认方案图层配置。"));
            return;
        }

        typeName = odMeta.value(QStringLiteral("typename")).toString().trimmed();
        CaseTreeModel* caseModel = m_caseModel;
        const int schemeCaseId = caseId;
        triggerLoad = [caseModel, schemeCaseId]() {
            return caseModel->setLayerCheckedByVal(schemeCaseId, kOtherOdLayerVal, true, nullptr);
        };
    }

    if (typeName.isEmpty()) {
        finish(false, QString::fromUtf8(u8"OD 图层缺少 WFS 类型名。"));
        return;
    }

    CaseOtherOdMapHelper::loadOrReloadAndApplyDemandStyle(map, typeName, triggerLoad, finish);
}

void CasePanel::clearDetailPanelWfsLayers()
{
    if (!m_detailPanel) {
        return;
    }
    CaseWfsMap::removePanelWfsLayers(m_detailPanel->standardMapWidget());
    CaseWfsMap::removePanelWfsLayers(m_detailPanel->leftCompareMapWidget());
    CaseWfsMap::removePanelWfsLayers(m_detailPanel->rightCompareMapWidget());
    if (!QCoreApplication::closingDown()) {
        resetSchemeDetailLegendToDefault();
    }
}

QModelIndex CasePanel::findCaseTreeIndex(int caseId) const
{
    if (!m_caseModel || caseId <= 0) {
        return QModelIndex();
    }
    const int rows = m_caseModel->rowCount(QModelIndex());
    for (int r = 0; r < rows; ++r) {
        const QModelIndex idx = m_caseModel->index(r, 0);
        if (!idx.isValid()) {
            continue;
        }
        if (idx.data(Qt::UserRole).toInt() == caseId) {
            return idx;
        }
    }
    return QModelIndex();
}

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
TNMapWidget* CasePanel::mapWidgetForSchemeCase(int caseId) const
{
    if (!m_detailPanel || caseId <= 0) {
        return nullptr;
    }

    const CaseMapToolbarWidget::ViewMode vm = m_detailPanel->toolbarViewMode();
    if (vm == CaseMapToolbarWidget::ViewMode::Compare) {
        if (caseId == m_detailPanel->leftCompareCaseId()) {
            return m_detailPanel->leftCompareMapWidget();
        }
        if (caseId == m_detailPanel->rightCompareCaseId()) {
            return m_detailPanel->rightCompareMapWidget();
        }
        return nullptr;
    }

    if (caseId == m_currentCaseId) {
        return m_detailPanel->standardMapWidget();
    }
    return nullptr;
}
#endif

void CasePanel::applyCaseTreeWfsToMap(TNMapWidget* map, int caseId, bool invalidatePending)
{
    if (!map || !m_caseModel || caseId <= 0) {
        return;
    }
    map->setStyleContext(m_projectId, caseId);

    if(invalidatePending)CaseWfsMap::invalidatePendingLoads();
    CaseWfsMap::removePanelWfsLayers(map);
    CaseWfsMap::prepareBatchWfsLayerLoad();

    const QModelIndex caseIdx = findCaseTreeIndex(caseId);
    if (!caseIdx.isValid()) {
        return;
    }

    const int topRows = m_caseModel->rowCount(caseIdx);
    for (int i = 0; i < topRows; ++i) {
        const QModelIndex topIdx = m_caseModel->index(i, 0, caseIdx);
        if (!topIdx.isValid()) {
            continue;
        }

        const QVariantMap topMeta = topIdx.data(Qt::UserRole).toMap();
        const bool isGroup = topMeta.value(QStringLiteral("nodeKind")).toString() == QStringLiteral("group");

        if (isGroup) {
            const int layerRows = m_caseModel->rowCount(topIdx);
            for (int j = 0; j < layerRows; ++j) {
                const QModelIndex layerIdx = m_caseModel->index(j, 0, topIdx);
                applyWfsForCaseTreeLayerIndex(map, layerIdx);
            }
        } else {
            applyWfsForCaseTreeLayerIndex(map, topIdx);
        }
    }
}

void CasePanel::applyWfsForCaseTreeLayerIndex(TNMapWidget* map, const QModelIndex& layerIdx)
{
    if (!map || !layerIdx.isValid() || !m_caseModel) {
        return;
    }

    const QVariantMap meta = layerIdx.data(Qt::UserRole).toMap();
    const QString typeName = meta.value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        return;
    }

    const int level = meta.value(QStringLiteral("level")).toInt();
    const QString displayName = layerIdx.data(Qt::DisplayRole).toString();
    const QString layerVal = meta.value(QStringLiteral("val")).toString().trimmed();
    const Qt::CheckState cs = static_cast<Qt::CheckState>(layerIdx.data(Qt::CheckStateRole).toInt());
    const bool visible = (cs == Qt::Checked);

    bool isBaseCase = false;
    QModelIndex caseIdx = layerIdx.parent();
    if (caseIdx.parent().isValid()) {
        caseIdx = caseIdx.parent();
    }
    CaseItem caseItem;
    if (caseIdx.isValid() && m_caseModel->getCaseData(caseIdx, caseItem)) {
        isBaseCase = caseItem.isBase == 1;
    }

    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    std::function<void()> rollback;
    if (visible) {
        rollback = [this, caseId, typeName]() {
            if (m_caseModel) {
                m_caseModel->setLayerCheckedByTypenameQuiet(caseId, typeName, false);
            }
        };
    }

    CaseWfsMap::applyWfsLayer(map, displayName, typeName, level, visible, layerVal, isBaseCase, rollback);
}

void CasePanel::syncSchemeDetailWfsFromCaseTree()
{
    if (!m_detailPanel || !m_caseModel) {
        return;
    }

    if (m_detailPanel->toolbarViewMode() == CaseMapToolbarWidget::ViewMode::Compare) {
        // Both maps belong to the same load generation. Invalidating for the right
        // map would cancel the left map's queued loads before they can run.
        CaseWfsMap::invalidatePendingLoads();
        applyCaseTreeWfsToMap(m_detailPanel->leftCompareMapWidget(), m_detailPanel->leftCompareCaseId(), false);
        applyCaseTreeWfsToMap(m_detailPanel->rightCompareMapWidget(), m_detailPanel->rightCompareCaseId(), false);
    } else {
        applyCaseTreeWfsToMap(m_detailPanel->standardMapWidget(), m_currentCaseId);
    }
    resetSchemeDetailLegendToDefault();
}

void CasePanel::onSchemeLayerActivated(int caseId, const QString& wfsTypeName)
{
    if (!m_detailPanel || !m_schemeWorkspaceStack) {
        return;
    }
    if (m_schemeWorkspaceStack->currentWidget() != m_detailPanel) {
        return;
    }
    if (m_detailPanel->toolbarViewMode() != CaseMapToolbarWidget::ViewMode::Standard) {
        return;
    }
    if (caseId != m_currentCaseId) {
        return;
    }

    TNMapWidget* map = m_detailPanel->standardMapWidget();
    if (!map) {
        return;
    }

    const QString trimmed = wfsTypeName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    map->selectLayerByUri(CaseWfsMap::buildWfsDataSourceUri(trimmed));
}

void CasePanel::onSchemeLayerDataViewRequested(int caseId, const QString& wfsTypeName)
{
    if (!m_detailPanel || !m_schemeWorkspaceStack) {
        return;
    }
    if (m_schemeWorkspaceStack->currentWidget() != m_detailPanel) {
        return;
    }
    if (m_detailPanel->toolbarViewMode() != CaseMapToolbarWidget::ViewMode::Standard) {
        return;
    }
    if (caseId != m_currentCaseId) {
        return;
    }

    TNMapWidget* map = m_detailPanel->standardMapWidget();
    CaseMapAttributeWidget* attributeWidget = m_detailPanel->standardAttributeWidget();
    if (!map || !attributeWidget) {
        return;
    }

    const QString trimmed = wfsTypeName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    m_detailPanel->switchToStandardDataView();

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(trimmed);
    if (!map->getLayer(uri)) {
        attributeWidget->showTip(QString::fromUtf8(u8"请先加载该图层至地图"));
        return;
    }

    map->selectLayerByUri(uri);
    attributeWidget->refreshFromCurrentLayer();
}

void CasePanel::onBaseDataLayerDataViewRequested(const QString& wfsTypeName, const QVariantMap& userData)
{
    Q_UNUSED(userData)

    if (!m_baseMapWidget || !m_baseAttributeWidget) {
        return;
    }

    const QString trimmed = wfsTypeName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    switchToBaseDataView();

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(trimmed);
    if (!m_baseMapWidget->getLayer(uri)) {
        m_baseAttributeWidget->showTip(QString::fromUtf8(u8"请先加载该图层至地图"));
        return;
    }

    m_baseMapWidget->selectLayerByUri(uri);
    m_baseAttributeWidget->refreshFromCurrentLayer();
}

void CasePanel::onCaseTreeLayerCheckChanged(const QModelIndex& index, Qt::CheckState state)
{
    if (!index.isValid() || !m_detailPanel || !m_schemeWorkspaceStack) {
        return;
    }
    if (m_schemeWorkspaceStack->currentWidget() != m_detailPanel) {
        return;
    }

    const QVariantMap meta = index.data(Qt::UserRole).toMap();
    const QString typeName = meta.value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        return;
    }

    QModelIndex caseIdx = index.parent();
    if (caseIdx.parent().isValid()) {
        caseIdx = caseIdx.parent();
    }
    if (!caseIdx.isValid()) {
        return;
    }

    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    const int level = meta.value(QStringLiteral("level")).toInt();
    const QString displayName = index.data(Qt::DisplayRole).toString();
    const QString layerVal = meta.value(QStringLiteral("val")).toString().trimmed();
    const bool visible = (state == Qt::Checked);

    bool isBaseCase = false;
    for (const CaseItem& caseItem : m_caseList) {
        if (caseItem.id == caseId) {
            isBaseCase = caseItem.isBase == 1;
            break;
        }
    }

    const CaseMapToolbarWidget::ViewMode vm = m_detailPanel->toolbarViewMode();
    std::function<void()> rollback;
    if (visible) {
        rollback = [this, caseId, typeName]() {
            if (m_caseModel) {
                m_caseModel->setLayerCheckedByTypenameQuiet(caseId, typeName, false);
            }
        };
    }

    if (vm == CaseMapToolbarWidget::ViewMode::Compare) {
        if (caseId == m_detailPanel->leftCompareCaseId()) {
            if (TNMapWidget* map = m_detailPanel->leftCompareMapWidget()) {
                map->setStyleContext(m_projectId, caseId);
            }
            CaseWfsMap::applyWfsLayer(m_detailPanel->leftCompareMapWidget(), displayName, typeName, level, visible,
                                      layerVal, isBaseCase, rollback);
        } else if (caseId == m_detailPanel->rightCompareCaseId()) {
            if (TNMapWidget* map = m_detailPanel->rightCompareMapWidget()) {
                map->setStyleContext(m_projectId, caseId);
            }
            CaseWfsMap::applyWfsLayer(m_detailPanel->rightCompareMapWidget(), displayName, typeName, level, visible,
                                      layerVal, isBaseCase, rollback);
        }
        return;
    }

    if (caseId == m_currentCaseId) {
        if (TNMapWidget* map = m_detailPanel->standardMapWidget()) {
            map->setStyleContext(m_projectId, caseId);
        }
        CaseWfsMap::applyWfsLayer(m_detailPanel->standardMapWidget(), displayName, typeName, level, visible, layerVal,
                                  isBaseCase, rollback);
        if (visible
            && (CaseWayLayer::isWayLayerVal(layerVal) || CaseWayLayer::isWayLayerTypeName(typeName))) {
            resetSchemeDetailLegendToDefault();
        }
    }
}

void CasePanel::onBaseDataLayerCheckChanged(const QString& layerName, bool checked, const QVariantMap& userData)
{
    const QString typeName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();

    if (!m_baseMapWidget) {
        if (checked && m_baseDataView && !typeName.isEmpty()) {
            m_baseDataView->setLayerCheckedByTypenameQuiet(typeName, false);
        }
        return;
    }
    if (typeName.isEmpty()) {
        return;
    }
    const int level = userData.value(baseDataUserDataKey(BaseDataUserDataField::Level)).toInt();
    const QString layerVal = userData.value(baseDataUserDataKey(BaseDataUserDataField::Val)).toString().trimmed();
    const QString display = layerName.trimmed().isEmpty() ? typeName : layerName.trimmed();
    m_baseMapWidget->setStyleContext(m_projectId, 0);
    std::function<void()> rollback;
    if (checked) {
        rollback = [this, typeName]() {
            if (!m_baseDataView) {
                return;
            }
            if (!m_baseDataView->setLayerCheckedByTypenameQuiet(typeName, false)) {
                qWarning() << QString::fromUtf8(u8"基础数据图层勾选回退失败：%1").arg(typeName);
            }
        };
    }
    CaseWfsMap::applyWfsLayer(m_baseMapWidget, display, typeName, level, checked, layerVal, true, rollback);
}

void CasePanel::onDetailCompareCasesChanged(int leftCaseId, int rightCaseId)
{
    Q_UNUSED(leftCaseId)
    Q_UNUSED(rightCaseId)
    syncSchemeDetailWfsFromCaseTree();
    if (m_compareRenderStyle != CompareRenderStyle::None) {
        applyCompareRenderStyleToBothMaps();
    }
}

void CasePanel::onCompareRenderStyleChanged(CompareRenderStyle style)
{
    m_compareRenderStyle = style;
    if (!m_detailPanel || m_detailPanel->toolbarViewMode() != CaseMapToolbarWidget::ViewMode::Compare) {
        return;
    }
    applyCompareRenderStyleToBothMaps();
}

void CasePanel::onDetailToolbarViewModeChanged(CaseMapToolbarWidget::ViewMode mode)
{
    if (!m_detailPanel) {
        return;
    }
    if (mode == CaseMapToolbarWidget::ViewMode::Standard) {
        m_compareRenderStyle = CompareRenderStyle::None;
        CaseWfsMap::removePanelWfsLayers(m_detailPanel->leftCompareMapWidget());
        CaseWfsMap::removePanelWfsLayers(m_detailPanel->rightCompareMapWidget());
        applyCaseTreeWfsToMap(m_detailPanel->standardMapWidget(), m_currentCaseId);
    } else {
        m_compareRenderStyle = CompareRenderStyle::None;
        CaseWfsMap::removePanelWfsLayers(m_detailPanel->standardMapWidget());
        syncSchemeDetailWfsFromCaseTree();
    }
}

void CasePanel::onSchemeMapToolTriggered(CaseMapToolbarWidget::ToolType toolType)
{
    if (toolType == CaseMapToolbarWidget::ToolType::RefreshData) {
        refreshWfsServiceInfo(false);
        return;
    }
    if (!m_mapToolController || !m_detailPanel || !m_detailPanel->standardMapWidget()) {
        return;
    }
    TNMapWidget* map = m_detailPanel->standardMapWidget();
    CaseMapToolContext ctx;
    ctx.projectId = m_projectId;
    ctx.caseId = m_currentCaseId;
    ctx.isBaseDataMap = false;
    ctx.isBaseCase = isCurrentCaseBase();
    ctx.exportNameContext = mapExportNameContext(m_currentCaseId);
    ctx.schemeLayerTree = m_caseTreeView;
    ctx.onFilterApplied = [this]() {
        if (m_detailPanel) {
            m_detailPanel->refreshStandardAttributeFromMap();
        }
    };
    ctx.onSchemeDataChanged = [this](std::function<void()> afterTreeSynced) {
        refreshCurrentCaseLayerTreeInPlace([this, afterTreeSynced](bool synced) {
            if (synced && afterTreeSynced) {
                afterTreeSynced();
            }
        });
    };
    ctx.loadSchemeAuxiliaryLayers = [this](TNMapWidget* schemeMap, int caseId,
                                           const QList<CaseAuxLayer::AuxLayerTarget>& targets) {
        loadAuxiliaryLayersOnSchemeMap(schemeMap, caseId, targets);
    };
    ctx.resolveTableFullName = [this, map](QgsVectorLayer* layer, QString* errorMessage) {
        return CaseMapLayerResolve::resolveTableFullName(m_caseTreeView, map, m_currentCaseId, layer, errorMessage);
    };
    const auto login = UIG().loginInfo();
    if (!login.isNull()) {
        ctx.authToken = login->j_token();
    }
    CaseMapToolbarWidget* schemeToolbar = m_detailPanel->schemeToolbarWidget();
    ctx.onGisAsyncBusyChanged = [schemeToolbar](bool busy) {
        if (schemeToolbar) {
            schemeToolbar->setGisAsyncToolsBusy(busy);
        }
    };
    const int schemeCaseId = m_currentCaseId;
    ctx.onDesirelineSucceeded = [this, map, schemeCaseId](
                                    const std::function<void(bool ok, const QString& err)>& onComplete) {
        applyOtherOdStyleAfterDesireline(map, false, schemeCaseId, onComplete);
    };
    ctx.prepareAddFeature = [this, schemeCaseId](TNMapWidget* schemeMap) {
        CaseWayFeatureAdd::prepareNodeLayerForWayAdd(schemeMap, m_caseTreeView, nullptr, schemeCaseId);
    };
    ctx.prefillFeatureAttributes = [map](QgsVectorLayer* layer, QgsFeature& feature, QStringList* readOnlyFieldNames) {
        QString warning;
        CaseWayFeatureAdd::prefillWayAttributes(map, layer, feature, &warning, readOnlyFieldNames);
        if (!warning.isEmpty()) {
            map->showMessage(warning, 4000);
        }
    };
    ctx.uncheckTraceTmpLayers = [this, schemeCaseId]() {
        if (!m_caseModel || schemeCaseId <= 0) {
            return;
        }
        const CaseLayerTreeSnapshot snapshot = m_caseModel->captureLayerCheckStates(schemeCaseId);
        for (auto it = snapshot.checkByTypename.constBegin(); it != snapshot.checkByTypename.constEnd(); ++it) {
            if (it.key().contains(QStringLiteral("trace_tmp"), Qt::CaseInsensitive)
                && it.value() == Qt::Checked) {
                m_caseModel->setLayerCheckedByTypenameQuiet(schemeCaseId, it.key(), false);
            }
        }
    };
    ctx.syncMapLegendFromGraduatedLayer = [this](QgsVectorLayer* layer, const QString& fieldName) {
        syncSchemeDetailLegendFromWayLayer(layer, fieldName);
    };
    const bool clearSymbol = toolType == CaseMapToolbarWidget::ToolType::ClearSymbol;
    m_mapToolController->runTool(map, toolType, ctx);
    if (clearSymbol) {
        resetSchemeDetailLegendToDefault();
    }
}

void CasePanel::onBaseMapToolTriggered(CaseMapToolbarWidget::ToolType toolType)
{
    if (toolType == CaseMapToolbarWidget::ToolType::RefreshData) {
        refreshWfsServiceInfo(true);
        return;
    }
    if (!m_mapToolController || !m_baseMapWidget) {
        return;
    }
    CaseMapToolContext ctx;
    ctx.projectId = m_projectId;
    ctx.caseId = 0;
    ctx.isBaseDataMap = true;
    ctx.exportNameContext = mapExportNameContext(0);
    ctx.onFilterApplied = [this]() {
        refreshBaseAttributeFromMap();
    };
    ctx.onBaseDataChanged = [this](std::function<void()> afterTreeSynced) {
        refreshBaseDataLayerTreeInPlace([this, afterTreeSynced](bool synced) {
            if (synced && afterTreeSynced) {
                afterTreeSynced();
            }
        });
    };
    ctx.loadBaseAuxiliaryLayers = [this](TNMapWidget* baseMap, const QList<CaseAuxLayer::AuxLayerTarget>& targets) {
        loadAuxiliaryLayersOnBaseMap(baseMap, targets);
    };
    ctx.resolveTableFullName = [this](QgsVectorLayer* layer, QString* errorMessage) {
        if (!m_baseDataView || !layer) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8(u8"图层树未就绪。");
            }
            return QString();
        }
        const QString wfsTypeName = CaseMapLayerResolve::vectorLayerWfsTypeName(layer).trimmed();
        const QVariantMap userData = m_baseDataView->findCheckedLayerUserDataByWfsTypeName(wfsTypeName);
        return CaseMapLayerResolve::resolveTableFullNameFromBaseData(wfsTypeName, userData, errorMessage);
    };
    const auto login = UIG().loginInfo();
    if (!login.isNull()) {
        ctx.authToken = login->j_token();
    }
    ctx.onGisAsyncBusyChanged = [this](bool busy) {
        if (m_baseMapToolbar) {
            m_baseMapToolbar->setGisAsyncToolsBusy(busy);
        }
    };
    ctx.onDesirelineSucceeded = [this](const std::function<void(bool ok, const QString& err)>& onComplete) {
        applyOtherOdStyleAfterDesireline(m_baseMapWidget, true, 0, onComplete);
    };
    ctx.prepareAddFeature = [this](TNMapWidget* baseMap) {
        CaseWayFeatureAdd::prepareNodeLayerForWayAdd(baseMap, nullptr, m_baseDataView, 0);
    };
    ctx.prefillFeatureAttributes = [this](QgsVectorLayer* layer, QgsFeature& feature, QStringList* readOnlyFieldNames) {
        QString warning;
        CaseWayFeatureAdd::prefillWayAttributes(m_baseMapWidget, layer, feature, &warning, readOnlyFieldNames);
        if (!warning.isEmpty()) {
            m_baseMapWidget->showMessage(warning, 4000);
        }
    };
    ctx.uncheckTraceTmpLayers = [this]() {
        if (!m_baseDataView) {
            return;
        }
        const BaseDataLayerTreeSnapshot snapshot = m_baseDataView->captureLayerCheckStates();
        for (auto it = snapshot.checkByTypename.constBegin(); it != snapshot.checkByTypename.constEnd(); ++it) {
            if (it.key().contains(QStringLiteral("trace_tmp"), Qt::CaseInsensitive)
                && it.value() == Qt::Checked) {
                m_baseDataView->setLayerCheckedByTypenameQuiet(it.key(), false);
            }
        }
    };
    ctx.syncMapLegendFromGraduatedLayer = [this](QgsVectorLayer* layer, const QString& fieldName) {
        syncBaseMapLegendFromLayer(layer, fieldName);
    };
    const bool clearSymbol = toolType == CaseMapToolbarWidget::ToolType::ClearSymbol;
    m_mapToolController->runTool(m_baseMapWidget, toolType, ctx);
    if (clearSymbol) {
        resetBaseMapLegendToDefault();
    }
}

bool CasePanel::currentCaseItem(CaseItem& out) const
{
    for (const CaseItem& item : m_caseList) {
        if (item.id == m_currentCaseId) {
            out = item;
            return true;
        }
    }
    return false;
}

CaseMapExportNameContext CasePanel::mapExportNameContext(int caseId) const
{
    CaseMapExportNameContext ctx;
    ctx.projectName = m_projectName.trimmed();
    if (ctx.projectName.isEmpty() && m_projectId > 0) {
        ctx.projectName = QString::fromUtf8(u8"项目%1").arg(m_projectId);
    }

    if (caseId > 0) {
        for (const CaseItem& item : m_caseList) {
            if (item.id == caseId) {
                ctx.caseName = item.name.trimmed();
                break;
            }
        }
    }
    return ctx;
}

bool CasePanel::isCurrentCaseBase() const
{
    CaseItem item;
    return currentCaseItem(item) && item.isBase == 1;
}

void CasePanel::onAnalysisResultViewed(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                       const QString& resultName)
{
    if (!isCurrentCaseBase() && resultKey != QLatin1String("traffic_flow")
        && resultKey != QLatin1String("saturation")) {
        return;
    }
    applyAnalysisResultOnMap(module, resultKey, resultName);
}

void CasePanel::onAnalysisModuleChanged(CaseWorkPanel::AnalysisModule module)
{
    Q_UNUSED(module)
    resetSchemeDetailLegendToDefault();
}

void CasePanel::onWorkStepRunFinished(const QString& stepKey)
{
    Q_UNUSED(stepKey)
    // 路网现状分析/方案评审运行成功后自动刷新数据（等效工具栏"刷新数据"按钮）
    refreshWfsServiceInfo(false);
}

void CasePanel::onAnalysisDataRequested(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                        const QString& tableName, const QString& resultName)
{
    Q_UNUSED(tableName)
    if (!isCurrentCaseBase() && resultKey != QLatin1String("traffic_flow")
        && resultKey != QLatin1String("saturation")) {
        return;
    }
    showAnalysisDataDialog(module, resultKey, resultName);
}

void CasePanel::onAiDataSelectRequested(CaseWorkPanel::AnalysisModule module, const QString& stepKey,
                                        bool activate)
{
    if (!m_mapToolController || !m_detailPanel) {
        return;
    }

    CaseWorkPanel* workPanel = m_detailPanel->workPanel();
    QPointer<CaseWorkPanel> workPanelPtr = workPanel;
    TNMapWidget* map = m_detailPanel->standardMapWidget();

    auto uncheckButton = [&]() {
        if (workPanelPtr) {
            workPanelPtr->setAiDataSelectChecked(stepKey, false);
        }
    };

    if (!activate) {
        if (map) {
            m_mapToolController->cancelAiWaySelect(map);
        }
        m_activeAiDataSelectStepKey.clear();
        return;
    }

    if (m_detailPanel->toolbarViewMode() != CaseMapToolbarWidget::ViewMode::Standard) {
        TnUi::showWarning(QString::fromUtf8(u8"数据选择"),
                          QString::fromUtf8(u8"请在标准视图下框选路段。"));
        uncheckButton();
        return;
    }
    if (!map) {
        TnUi::showWarning(QString::fromUtf8(u8"数据选择"), QString::fromUtf8(u8"地图未就绪。"));
        uncheckButton();
        return;
    }

    QString error;
    QgsVectorLayer* wayLayer = wayLayerOnStandardMap(module, &error);
    if (!wayLayer) {
        TnUi::showWarning(QString::fromUtf8(u8"数据选择"),
                          error.isEmpty() ? QString::fromUtf8(u8"未找到路段图层。") : error);
        uncheckButton();
        return;
    }

    if (!m_activeAiDataSelectStepKey.isEmpty() && m_activeAiDataSelectStepKey != stepKey) {
        m_mapToolController->cancelAiWaySelect(map);
    }

    m_activeAiDataSelectStepKey = stepKey;
    m_mapToolController->runAiWaySelect(map, wayLayer,
        [this, workPanelPtr, stepKey](const QStringList& linkIds) {
            m_activeAiDataSelectStepKey.clear();
            if (!workPanelPtr) {
                return;
            }
            workPanelPtr->finishAiDataSelect(stepKey, linkIds);
            if (!linkIds.isEmpty()) {
                TnUi::showInformation(QString::fromUtf8(u8"数据选择"),
                                      QString::fromUtf8(u8"已选中 %1 个路段。").arg(linkIds.size()));
            }
        });
}

void CasePanel::onTrafficMicroDiagnosisRequested()
{
    if (!m_detailPanel) {
        return;
    }

    CaseWorkPanel* workPanel = m_detailPanel->workPanel();
    if (!workPanel) {
        return;
    }

    if (m_detailPanel->toolbarViewMode() != CaseMapToolbarWidget::ViewMode::Standard) {
        return;
    }

    TNMapWidget* map = m_detailPanel->standardMapWidget();
    if (!map || !map->canvas()) {
        TnUi::showWarning(QString::fromUtf8(u8"中微观诊断"),
                          QString::fromUtf8(u8"地图未就绪。"));
        return;
    }

    QgsVectorLayer* roadWayLayer = nullptr;
    const QList<QgsMapLayer*> layers = map->canvas()->layers();
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
        roadWayLayer = vectorLayer;
        break;
    }

    if (!roadWayLayer) {
        TnUi::showWarning(QString::fromUtf8(u8"中微观诊断"),
                          QString::fromUtf8(u8"请先加载道路路段图层并选择要素。"));
        return;
    }

    const int linkFieldIndex = roadWayLayer->fields().indexOf(QStringLiteral("link_id"));
    if (linkFieldIndex < 0) {
        TnUi::showWarning(QString::fromUtf8(u8"中微观诊断"),
                          QString::fromUtf8(u8"图层缺少 link_id 字段。"));
        return;
    }

    QStringList linkIds;
    const QgsFeatureIds featureIds = roadWayLayer->selectedFeatureIds();
    linkIds.reserve(featureIds.size());
    for (QgsFeatureId fid : featureIds) {
        const QgsFeature feature = roadWayLayer->getFeature(fid);
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
        TnUi::showWarning(QString::fromUtf8(u8"中微观诊断"),
                          QString::fromUtf8(u8"未获取到有效的路段 link_id。"));
        return;
    }

    workPanel->startTrafficMicroDiagnosis(linkIds.join(QLatin1Char(',')));
}

QgsVectorLayer* CasePanel::wayLayerOnStandardMap(CaseWorkPanel::AnalysisModule module, QString* errorOut,
                                                 QString* wfsUriOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    if (wfsUriOut) {
        wfsUriOut->clear();
    }

    if (!m_detailPanel || !m_caseModel || m_currentCaseId <= 0) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"方案详情未就绪。");
        }
        return nullptr;
    }
    if (m_detailPanel->toolbarViewMode() != CaseMapToolbarWidget::ViewMode::Standard) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"请在标准视图下查看分析数据。");
        }
        return nullptr;
    }

    TNMapWidget* map = m_detailPanel->standardMapWidget();
    if (!map) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"地图未就绪。");
        }
        return nullptr;
    }

    const QString layerVal = wayLayerValForModule(module);
    QVariantMap meta;
    if (!m_caseModel->setLayerCheckedByVal(m_currentCaseId, layerVal, true, &meta)) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"未找到图层 %1，请确认方案菜单配置。").arg(layerVal);
        }
        return nullptr;
    }

    const QString typeName = meta.value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层缺少 WFS 类型名。");
        }
        return nullptr;
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(typeName);
    if (wfsUriOut) {
        *wfsUriOut = uri;
    }

    auto* vectorLayer = qobject_cast<QgsVectorLayer*>(map->getLayer(uri));
    if (!vectorLayer || !vectorLayer->isValid()) {
        return nullptr;
    }
    return vectorLayer;
}

void CasePanel::autoSaveAnalysisMapCanvas(TNMapWidget* map, const QString& /*resultName*/)
{
    if (!m_mapToolController || !map || !map->canvas()) {
        qWarning() << QString(u8"自动保存画布跳过：地图或导出控制器未就绪。");
        return;
    }

    const QString imagesDir = QDir(applicationBaseDir()).filePath(QStringLiteral("images"));
    if (!QDir().mkpath(imagesDir)) {
        qWarning() << QString(u8"自动保存画布失败：无法创建 images 目录：%1").arg(imagesDir);
        return;
    }

    const QString fileName =
        buildDefaultExportFileName(mapExportNameContext(m_currentCaseId), QStringLiteral("png"));
    const QString outputPath = QDir(imagesDir).filePath(fileName);

    CaseMapExportOptions options;
    options.extentMode = CaseMapExportOptions::ExtentMode::CurrentView;
    options.format = CaseMapExportOptions::Format::Png;
    options.dpi = 96;
    options.includeBasemap = true;
    options.outputPath = outputPath;

    m_mapToolController->exportMapWithOptions(map, options, MapExportFeedback::Auto);
}

void CasePanel::resetSchemeDetailLegendToDefault()
{
    if (!m_detailPanel) {
        return;
    }
    CaseMapLegend::applyDefault(m_detailPanel->standardLegendWidget());
}

void CasePanel::resetBaseMapLegendToDefault()
{
    CaseMapLegend::applyDefault(m_baseLegendWidget);
}

void CasePanel::syncBaseMapLegendFromLayer(QgsVectorLayer* layer, const QString& fieldName)
{
    if (!m_baseLegendWidget || !layer) {
        return;
    }

    QPointer<CaseMapLegendWidget> legendGuard(m_baseLegendWidget);
    QPointer<QgsVectorLayer> layerGuard(layer);
    QPointer<TNMapWidget> mapGuard(m_baseMapWidget);
    const auto trySync = [legendGuard, layerGuard, mapGuard, fieldName]() -> bool {
        if (!legendGuard || !layerGuard) {
            return false;
        }
        auto* graduated = dynamic_cast<QgsGraduatedSymbolRenderer*>(layerGuard->renderer());
        if (!graduated || graduated->classAttribute() != fieldName) {
            return false;
        }
        CaseMapLegend::showCustom(legendGuard,
                                  CaseMapLegend::itemsFromGraduatedRenderer(graduated, fieldName),
                                  CaseMapLegend::graduatedLegendTitle(fieldName));
        if (mapGuard) {
            mapGuard->refreshBottomLeftOverlay();
        }
        return true;
    };

    if (trySync()) {
        return;
    }
    QTimer::singleShot(0, layer, [trySync]() { trySync(); });
    QTimer::singleShot(200, layer, [trySync]() { trySync(); });
}

void CasePanel::syncLeftCompareLegendFromWayLayer(QgsVectorLayer* layer, WayLayerAnalysisStyle style)
{
    if (!m_detailPanel || !layer) {
        return;
    }

    const QString fieldName = CaseWayLayerStyle::graduatedFieldName(style);
    if (fieldName.isEmpty()) {
        return;
    }

    QPointer<CaseMapLegendWidget> legendGuard(m_detailPanel->leftCompareLegendWidget());
    QPointer<QgsVectorLayer> layerGuard(layer);
    QPointer<TNMapWidget> mapGuard(m_detailPanel->leftCompareMapWidget());
    const auto trySync = [legendGuard, layerGuard, mapGuard, fieldName]() -> bool {
        if (!legendGuard || !layerGuard) {
            return false;
        }
        auto* graduated = dynamic_cast<QgsGraduatedSymbolRenderer*>(layerGuard->renderer());
        if (!graduated || graduated->classAttribute() != fieldName) {
            return false;
        }
        CaseMapLegend::showCustom(legendGuard,
                                  CaseMapLegend::itemsFromGraduatedRenderer(graduated, fieldName),
                                  CaseMapLegend::graduatedLegendTitle(fieldName));
        if (mapGuard) {
            mapGuard->refreshBottomLeftOverlay();
        }
        return true;
    };

    if (trySync()) {
        return;
    }
    QTimer::singleShot(0, layer, [trySync]() { trySync(); });
    QTimer::singleShot(200, layer, [trySync]() { trySync(); });
}

void CasePanel::applyCompareRenderStyleToBothMaps()
{
    if (!m_detailPanel || !m_caseModel) {
        return;
    }

    TNMapWidget* leftMap = m_detailPanel->leftCompareMapWidget();
    TNMapWidget* rightMap = m_detailPanel->rightCompareMapWidget();
    const int leftCaseId = m_detailPanel->leftCompareCaseId();
    const int rightCaseId = m_detailPanel->rightCompareCaseId();
    const CompareRenderStyle style = m_compareRenderStyle;

    if (rightMap && rightCaseId > 0) {
        CaseCompareRender::applyToMap(rightMap, rightCaseId, m_caseModel, m_caseList, style, nullptr);
    }

    if (!leftMap || leftCaseId <= 0) {
        return;
    }

    QPointer<CasePanel> self(this);
    CaseCompareRender::applyToMap(leftMap, leftCaseId, m_caseModel, m_caseList, style,
                                  [self, style](QgsVectorLayer* layer, bool applied) {
                                      if (!self || !self->m_detailPanel || !applied) {
                                          return;
                                      }
                                      CaseMapLegendWidget* legend = self->m_detailPanel->leftCompareLegendWidget();
                                      if (!legend) {
                                          return;
                                      }
                                      if (style == CompareRenderStyle::None) {
                                          CaseMapLegend::applyDefault(legend);
                                          return;
                                      }
                                      WayLayerAnalysisStyle wayStyle = WayLayerAnalysisStyle::None;
                                      if (style == CompareRenderStyle::TrafficFlow) {
                                          wayStyle = WayLayerAnalysisStyle::TrafficFlow;
                                      } else if (style == CompareRenderStyle::Saturation) {
                                          wayStyle = WayLayerAnalysisStyle::TrafficSaturation;
                                      }
                                      self->syncLeftCompareLegendFromWayLayer(layer, wayStyle);
                                  });
}

void CasePanel::syncSchemeDetailLegendFromWayLayer(QgsVectorLayer* layer, const QString& fieldName)
{
    if (!m_detailPanel || !layer) {
        return;
    }

    QPointer<CaseMapLegendWidget> legendGuard(m_detailPanel->standardLegendWidget());
    QPointer<QgsVectorLayer> layerGuard(layer);
    QPointer<TNMapWidget> mapGuard(m_detailPanel->standardMapWidget());
    const auto trySync = [legendGuard, layerGuard, mapGuard, fieldName]() -> bool {
        if (!legendGuard || !layerGuard) {
            return false;
        }
        auto* graduated = dynamic_cast<QgsGraduatedSymbolRenderer*>(layerGuard->renderer());
        if (!graduated || graduated->classAttribute() != fieldName) {
            return false;
        }
        CaseMapLegend::showCustom(legendGuard,
                                  CaseMapLegend::itemsFromGraduatedRenderer(graduated, fieldName),
                                  CaseMapLegend::graduatedLegendTitle(fieldName));
        if (mapGuard) {
            mapGuard->refreshBottomLeftOverlay();
        }
        return true;
    };

    if (trySync()) {
        return;
    }
    QTimer::singleShot(0, layer, [trySync]() { trySync(); });
    QTimer::singleShot(200, layer, [trySync]() { trySync(); });
}

void CasePanel::drainPendingAnalysisResultView()
{
    if (!m_hasPendingAnalysisView) {
        return;
    }
    const CaseWorkPanel::AnalysisModule module = m_pendingAnalysisModule;
    const QString resultKey = m_pendingAnalysisResultKey;
    const QString resultName = m_pendingAnalysisResultName;
    m_hasPendingAnalysisView = false;
    QTimer::singleShot(0, this, [this, module, resultKey, resultName]() {
        applyAnalysisResultOnMap(module, resultKey, resultName);
    });
}

void CasePanel::applyAnalysisResultOnMap(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                         const QString& resultName)
{
    if (m_analysisStyleApplying) {
        m_pendingAnalysisModule = module;
        m_pendingAnalysisResultKey = resultKey;
        m_pendingAnalysisResultName = resultName;
        m_hasPendingAnalysisView = true;
        return;
    }

    QString error;
    QString uri;
    wayLayerOnStandardMap(module, &error, &uri);
    if (!error.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"路网分析"), error);
        return;
    }
    if (uri.isEmpty()) {
        return;
    }

    ++m_analysisApplyGeneration;
    const int generation = m_analysisApplyGeneration;

    TNMapWidget* map = m_detailPanel->standardMapWidget();
    QPointer<CasePanel> self(this);
    const bool shouldAutoSave = resultKey == QStringLiteral("traffic_flow")
                                || resultKey == QStringLiteral("saturation");
    auto applied = std::make_shared<bool>(false);
    auto autoSaved = std::make_shared<bool>(false);
    auto busy = std::make_shared<bool>(false);

    const WayLayerAnalysisStyle wayStyle = CaseWayLayerStyle::fromResultKey(resultKey);
    const auto applyOnce = [self, map, uri, wayStyle, resultName,
                            shouldAutoSave, applied, autoSaved, busy, generation]() -> bool {
        if (!self || !map) {
            return false;
        }
        if (generation != self->m_analysisApplyGeneration) {
            return true;
        }
        if (*applied) {
            return true;
        }
        if (*busy) {
            return false;
        }
        auto* vectorLayer = qobject_cast<QgsVectorLayer*>(map->getLayer(uri));
        if (!vectorLayer || !vectorLayer->isValid()) {
            return false;
        }

        *busy = true;
        self->m_analysisStyleApplying = true;
        struct ApplyGuard {
            std::function<void()> cleanup;
            ~ApplyGuard()
            {
                if (cleanup) {
                    cleanup();
                }
            }
        } applyGuard{[self, busy, applied]() {
            if (busy) {
                *busy = false;
            }
            if (!self) {
                return;
            }
            self->m_analysisStyleApplying = false;
            if (self->m_hasPendingAnalysisView) {
                if (applied) {
                    *applied = true;
                }
                self->drainPendingAnalysisResultView();
            }
        }};

        CaseWayLayer::DefaultTypeFilterSuspendGuard typeFilterGuard;

        self->resetSchemeDetailLegendToDefault();
        CaseWfsMap::resetPanelWfsDefaultSymbols(map);
        if (generation != self->m_analysisApplyGeneration) {
            CaseWfsMap::clearLoadQueueIdleCallback();
            return true;
        }
        if (self->m_hasPendingAnalysisView) {
            CaseWfsMap::clearLoadQueueIdleCallback();
            return true;
        }

        vectorLayer = qobject_cast<QgsVectorLayer*>(map->getLayer(uri));
        if (!vectorLayer) {
            return false;
        }

        if (!CaseWayLayerStyle::applyAnalysisStyle(vectorLayer, wayStyle, QStringLiteral("RdYlGn"), false, map)) {
            return false;
        }
        if (self->m_hasPendingAnalysisView) {
            CaseWfsMap::clearLoadQueueIdleCallback();
            return true;
        }

        self->syncSchemeDetailLegendFromWayLayer(vectorLayer, CaseWayLayerStyle::graduatedFieldName(wayStyle));

        if (map->canvas()) {
            map->canvas()->refresh();
        }
        *applied = true;
        CaseWfsMap::clearLoadQueueIdleCallback();

        if (shouldAutoSave && !*autoSaved) {
            *autoSaved = true;
            QTimer::singleShot(100, self, [self, map, resultName]() {
                if (self && map) {
                    self->autoSaveAnalysisMapCanvas(map, resultName);
                }
            });
        }
        return true;
    };

    if (applyOnce()) {
        return;
    }

    const int retryDelays[] = {150, 400, 800, 1500};
    for (int delay : retryDelays) {
        QTimer::singleShot(delay, this, [applyOnce]() { applyOnce(); });
    }

    CaseWfsMap::setLoadQueueIdleCallback([applyOnce]() { applyOnce(); });
}

void CasePanel::showAnalysisDataDialog(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                       const QString& resultName)
{
    const auto openDialog = [this, resultKey, resultName](QgsVectorLayer* vectorLayer) {
        if (!vectorLayer) {
            return;
        }

        DataBrowserDialog dialog(this);
        const bool isFlow = (resultKey == QStringLiteral("traffic_flow")
                             || resultKey == QStringLiteral("traffic_flow_diag"));
        if (isFlow) {
            dialog.setTitleText(resultName.isEmpty() ? QString::fromUtf8(u8"路网流量分布") : resultName);
            dialog.setVisibleFields(
                {QStringLiteral("link_id"), QStringLiteral("name"), QStringLiteral("volume"), QStringLiteral("capacity"),
                 QStringLiteral("v_c"), QStringLiteral("link_flow_diff"), QStringLiteral("saturation_diff")});
        } else {
            dialog.setTitleText(resultName.isEmpty() ? QString::fromUtf8(u8"交通饱和度") : resultName);
            dialog.setVisibleFields({QStringLiteral("link_id"), QStringLiteral("name"), QStringLiteral("volume"),
                                     QStringLiteral("capacity"), QStringLiteral("v_c")});
        }

        dialog.loadVectorLayer(vectorLayer);
        dialog.moveToRightTop(this);
        dialog.exec();
    };

    QString error;
    QString uri;
    if (QgsVectorLayer* vectorLayer = wayLayerOnStandardMap(module, &error, &uri)) {
        openDialog(vectorLayer);
        return;
    }

    if (!error.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"分析数据"), error);
        return;
    }

    if (uri.isEmpty()) {
        return;
    }

    TNMapWidget* map = m_detailPanel->standardMapWidget();
    QPointer<CasePanel> self(this);
    const auto tryOpen = [self, map, uri, openDialog]() {
        if (!self || !map) {
            return;
        }
        auto* layer = qobject_cast<QgsVectorLayer*>(map->getLayer(uri));
        if (layer && layer->isValid()) {
            openDialog(layer);
        }
    };
    QTimer::singleShot(150, this, tryOpen);
    QTimer::singleShot(400, this, tryOpen);
}

