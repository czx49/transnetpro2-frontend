/*****************************************************************//**
 * \file   homepanel.cpp
 * \brief  首页面板实现
 * \details 首页主视图、项目管理与帮助信息的实现
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "homepanel.h"

#include "appsession.h"
#include "httpapijson.h"
#include "uiglobalstate.h"
#include "webview2dialog.h"

#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QSharedPointer>
#include <QHBoxLayout>
#include <QLabel>
#include "../publicctrls/messagedialog.h"
#include <QMouseEvent>
#include <QPixmap>
#include <QTimer>
#include <QVariantMap>

#include <functional>

// TNP_HOME_HIDE_FEATURE_CARDS (2026-05-19)
// 需求：暂时隐藏首页五张业务功能卡片（城市规划/交通规划/交通管控/交通政策/公共交通）。
// 恢复步骤：
//   1) initUi()：取消注释 contentLayout->addWidget(createFeatureCardArea());
//   2) applyBaseConfigFromSession()：取消注释「功能卡片标题」同步块；
//   3) 将 kQuickStartScroll* 常量恢复为注释中的旧值（96 / 260）；
//   4) 移除 middleLayout 的 stretch=1 与 setAlignment(AlignTop)，恢复 contentLayout->addStretch()。

namespace HomePanelConsts {
constexpr int kQuickStartRowMinHeight = 36;
constexpr int kQuickStartRowSpacing = 2;
constexpr int kQuickStartScrollMinHeight = 140; // 原 96（隐藏功能卡片后拔高列表）
constexpr int kQuickStartEmptyScrollHeight = 108;
} // namespace HomePanelConsts

namespace {
QString defaultHeroSiteName()
{
    return QString(u8"城市综合交通网络预测与管理");
}

QString defaultHeroSubLine()
{
    return QString(u8"欢迎使用");
}

QString defaultHeroDescription()
{
    return QString(
        u8"提供面向不同业务功能的“一体式”城市综合交通集成分析平台");
}

QString defaultCardTitle(FeatureCardType type)
{
    switch (type) {
    case FeatureCardType::Planning:
        return QString(u8"城市规划");
    case FeatureCardType::DataManage:
        return QString(u8"交通规划");
    case FeatureCardType::Evaluation:
        return QString(u8"交通管控");
    case FeatureCardType::Simulation:
        return QString(u8"交通政策");
    case FeatureCardType::Publish:
        return QString(u8"公共交通");
    default:
        return QString(u8"未知模块");
    }
}

int featureCardTitId(FeatureCardType type)
{
    switch (type) {
    case FeatureCardType::Planning:
        return 1;
    case FeatureCardType::DataManage:
        return 2;
    case FeatureCardType::Evaluation:
        return 3;
    case FeatureCardType::Simulation:
        return 4;
    case FeatureCardType::Publish:
        return 5;
    default:
        return 0;
    }
}

const QList<QString>& defaultContactLines()
{
    static const QList<QString> lines = {
        QString(u8"张明、李华、王强"),
        QString(u8"contact@gis-traffic.com"),
        QString(u8"support@gis-traffic.com"),
        QString(u8"400-888-9999"),
        QString(u8"北京市海淀区中关村大街1号")
    };
    return lines;
}

ProjectItem makeProjectItem(int id,
                            const QString& name,
                            const QString& description,
                            const QDateTime& createTime,
                            int caseCount,
                            const QString& status,
                            const QString& path = QString())
{
    ProjectItem item;
    item.id = id;
    item.name = name;
    item.description = description;
    item.createTime = createTime;
    item.caseCount = caseCount;
    item.status = status;
    item.path = path;
    return item;
}

void clearLayout(QLayout* layout)
{
    if (!layout) {
        return;
    }

    while (QLayoutItem* child = layout->takeAt(0)) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        if (child->layout()) {
            clearLayout(child->layout());
        }
        delete child;
    }
}

/** 仅清空「快速开始」列表布局：不递归子 layout，避免遗漏或重复处理 */
void clearQuickStartListLayout(QLayout* layout)
{
    if (!layout) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
}

/**
 * \brief 最近项目一行（不用 QPushButton 嵌套 layout，避免 Windows 下高度塌成 0）
 */
QString resolveQuickStartRegionText(const RecentProjectEntry& entry, ProjectItemModel* projectModel)
{
    QString province = entry.provinceName.trimmed();
    QString city = entry.cityName.trimmed();
    if (province.isEmpty() && city.isEmpty() && entry.projectId > 0 && projectModel) {
        ProjectItem item;
        if (projectModel->projectById(entry.projectId, item)) {
            province = item.provinceName.trimmed();
            city = item.cityName.trimmed();
        }
    }
    return formatProjectRegionText(province, city);
}

class QuickStartRowWidget final : public QWidget {
public:
    QuickStartRowWidget(const RecentProjectEntry& entry,
                        const QString& regionText,
                        std::function<void(const RecentProjectEntry&)> onActivate,
                        QWidget* parent)
        : QWidget(parent)
        , m_entry(entry)
        , m_onActivate(std::move(onActivate))
    {
        setObjectName(QStringLiteral("quickStartRow"));
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(HomePanelConsts::kQuickStartRowMinHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setStyleSheet(
            "#quickStartRow {"
            "   background-color: transparent;"
            "   border-radius: 10px;"
            "}"
            "#quickStartRow:hover {"
            "   background-color: rgba(29, 71, 137, 0.28);"
            "}"
        );

        auto* rowLayout = new QHBoxLayout(this);
        rowLayout->setContentsMargins(18, 7, 18, 7);
        rowLayout->setSpacing(18);

        auto* nameLabel = new QLabel(m_entry.name, this);
        nameLabel->setStyleSheet(
            "QLabel {"
            "   color: rgba(235, 240, 248, 0.92);"
            "   font-size: 15px;"
            "   font-weight: 500;"
            "}"
        );

        auto* regionLabel = new QLabel(regionText, this);
        regionLabel->setMinimumWidth(120);
        regionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        regionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        regionLabel->setStyleSheet(
            "QLabel {"
            "   color: rgba(171, 188, 214, 0.78);"
            "   font-size: 14px;"
            "}"
        );

        auto* timeLabel = new QLabel(m_entry.lastOpened.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")), this);
        timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        timeLabel->setStyleSheet(
            "QLabel {"
            "   color: rgba(171, 188, 214, 0.78);"
            "   font-size: 14px;"
            "}"
        );

        rowLayout->addWidget(nameLabel, 1);
        rowLayout->addWidget(regionLabel);
        rowLayout->addWidget(timeLabel);
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_onActivate) {
            m_onActivate(m_entry);
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    RecentProjectEntry m_entry;
    std::function<void(const RecentProjectEntry&)> m_onActivate;
};

QPixmap scaledPixmapForBox(const QString& iconPath, const QSize& boxSize, qreal fillRatio)
{
    const QPixmap pixmap(iconPath);
    if (pixmap.isNull()) {
        return pixmap;
    }

    const int iconSize = qMax(1, qRound(qMin(boxSize.width(), boxSize.height()) * fillRatio));
    return pixmap.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
} // namespace

HomePanel::HomePanel(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_scrollArea(nullptr)
    , m_planningCard(nullptr)
    , m_dataManageCard(nullptr)
    , m_evaluationCard(nullptr)
    , m_simulationCard(nullptr)
    , m_publishCard(nullptr)
    , m_projectModel(new ProjectItemModel(this))
    , m_newProjectDialog(new NewProjectDialog(this))
    , m_myProjectsDialog(new MyProjectsDialog(this))
    , m_myProjectsButton(nullptr)
    , m_quickStartListFrame(nullptr)
    , m_quickStartListScrollArea(nullptr)
    , m_quickStartListScrollContent(nullptr)
    , m_quickStartListLayout(nullptr)
    , m_heroMainTitleLabel(nullptr)
    , m_heroSubTitleLabel(nullptr)
    , m_heroDescLabel(nullptr)
    , m_helpDocsGridLayout(nullptr)
    , m_waitingForCreatePrompt(false)
{
    m_recentHistory.load();
    initUi();
    refreshQuickStartList();

    connect(&AppSession::instance(), &AppSession::baseConfigChanged, this, &HomePanel::applyBaseConfigFromSession);
    connect(&UIG(), &UIGlobalState::uiEventNotify, this, &HomePanel::onUiEventNotify);

    m_myProjectsDialog->setModel(m_projectModel);
    connect(m_myProjectsDialog, &MyProjectsDialog::projectActivated, this, &HomePanel::onProjectActivated);
    connect(m_myProjectsDialog, &MyProjectsDialog::newProjectRequested, this, &HomePanel::onNewProjectRequestedFromDialog);
    connect(m_myProjectsDialog, &MyProjectsDialog::projectEditRequested, this, &HomePanel::onProjectEditRequested);
    connect(m_myProjectsDialog, &MyProjectsDialog::projectDeleteRequested, this, &HomePanel::onProjectDeleteRequested);

    refreshProjectListFromGlobalState();
    UIG().loadProjectPage(1, 50);
}

HomePanel::~HomePanel()
{
}

ProjectItemModel* HomePanel::projectModel() const
{
    return m_projectModel;
}

void HomePanel::recordLatestOpenedProject(int projectId)
{
    ProjectItem item;
    if (!m_projectModel->projectById(projectId, item)) {
        return;
    }
    m_recentHistory.upsertFromProjectItem(item);
    refreshQuickStartList();
}

void HomePanel::recordLatestOpenedProject(const ProjectItem& item)
{
    m_recentHistory.upsertFromProjectItem(item);
    refreshQuickStartList();
}

void HomePanel::saveRecentProjects()
{
    m_recentHistory.save();
}

void HomePanel::refreshQuickStartFromHistory()
{
    refreshQuickStartList();
}

void HomePanel::triggerNewProject()
{
    onNewProjectClicked();
}

void HomePanel::triggerOpenProject()
{
    onOpenProjectClicked();
}

void HomePanel::applyBaseConfigFromSession()
{
    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();

    if (m_heroMainTitleLabel) {
        m_heroMainTitleLabel->setText(cfg && !cfg->j_site_name().isEmpty() ? cfg->j_site_name()
                                                                          : defaultHeroSiteName());
    }

    if (m_heroSubTitleLabel) {
        if (cfg && !cfg->j_tit1().isEmpty()) {
            m_heroSubTitleLabel->setText(cfg->j_tit1());
        } else {
            m_heroSubTitleLabel->setText(defaultHeroSubLine());
        }
    }

    if (m_heroDescLabel) {
        m_heroDescLabel->setText(cfg && !cfg->j_tit2().isEmpty() ? cfg->j_tit2() : defaultHeroDescription());
    }

    // TNP_HOME_HIDE_FEATURE_CARDS — 功能卡片标题同步（恢复卡片显示时取消下方注释）
    /*
    QHash<int, QString> idToTitle;
    if (cfg) {
        const QList<BaseConfigTitleJson> titlist = cfg->j_titlist();
        for (const BaseConfigTitleJson& entry : titlist) {
            idToTitle.insert(entry.j_id(), entry.j_title());
        }
    }

    const QList<FeatureCardWidget*> cards = {
        m_planningCard, m_dataManageCard, m_evaluationCard, m_simulationCard, m_publishCard
    };
    for (FeatureCardWidget* card : cards) {
        if (!card) {
            continue;
        }
        const int tid = featureCardTitId(card->cardType());
        const QString fromApi = idToTitle.value(tid);
        card->setDisplayTitle(fromApi.isEmpty() ? defaultCardTitle(card->cardType()) : fromApi);
    }
    */

    if (m_contactTextLabels.size() >= 5) {
        const QList<QString>& fallbacks = defaultContactLines();
        const auto setContactLine = [&](int index, const QString& apiValue) {
            QLabel* lab = m_contactTextLabels.at(index);
            if (!lab) {
                return;
            }
            lab->setText(apiValue.isEmpty() ? fallbacks.at(index) : apiValue);
        };

        if (cfg) {
            setContactLine(0, cfg->j_contact());
            setContactLine(1, cfg->j_email1());
            setContactLine(2, cfg->j_email2());
            setContactLine(3, cfg->j_phone());
            setContactLine(4, cfg->j_address());
        } else {
            for (int i = 0; i < 5; ++i) {
                setContactLine(i, QString());
            }
        }
    }

    rebuildHelpDocButtons();
}

void HomePanel::onFeatureCardClicked(FeatureCardType type)
{
    const QList<FeatureCardWidget*> cards = {
        m_planningCard, m_dataManageCard, m_evaluationCard, m_simulationCard, m_publishCard
    };

    for (FeatureCardWidget* card : cards) {
        if (card) {
            card->setSelected(card->cardType() == type);
        }
    }

    emit featureCardClicked(type);
}

void HomePanel::onMyProjectsClicked()
{
    m_myProjectsDialog->refresh();
    m_myProjectsDialog->exec();
}

void HomePanel::onNewProjectClicked()
{
    m_newProjectDialog->reset();
    if (m_newProjectDialog->exec() != QDialog::Accepted) {
        return;
    }

    const QString projectName = m_newProjectDialog->projectName().trimmed();
    if (projectName.isEmpty()) {
        return;
    }
    m_waitingForCreatePrompt = true;
    UIG().createProject(projectName,
                        u8"新建交通网络预测项目",
                        m_newProjectDialog->provinceId(),
                        m_newProjectDialog->provinceName(),
                        m_newProjectDialog->cityId(),
                        m_newProjectDialog->cityName());
}

void HomePanel::onOpenProjectClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(this, u8"选择项目目录", QString());
    if (dir.isEmpty()) {
        return;
    }

    const int projectId = findOrCreateProjectForPath(dir, QString());
    openProject(projectId);
}

void HomePanel::onTeachingCasesClicked()
{
    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();
    const QString url = cfg ? cfg->j_study_url() : QString();
    openWebViewUrlOrWarn(this, QString::fromUtf8(u8"教学案例"), url);
}

void HomePanel::onProjectActivated(int projectId)
{
    openProject(projectId);
}

void HomePanel::onNewProjectRequestedFromDialog()
{
    QTimer::singleShot(0, this, &HomePanel::onNewProjectClicked);
}

void HomePanel::onProjectEditRequested(int projectId)
{
    ProjectItem item;
    if (!m_projectModel->projectById(projectId, item)) {
        return;
    }

    m_newProjectDialog->reset();
    m_newProjectDialog->prepareForEdit(item);
    if (m_newProjectDialog->exec() != QDialog::Accepted) {
        return;
    }

    m_pendingProjectListAction = QStringLiteral("edit");
    m_pendingProjectId = projectId;
    UIG().editProject(projectId,
                      m_newProjectDialog->projectName(),
                      item.description,
                      m_newProjectDialog->provinceId(),
                      m_newProjectDialog->provinceName(),
                      m_newProjectDialog->cityId(),
                      m_newProjectDialog->cityName());
}

void HomePanel::onProjectDeleteRequested(int projectId)
{
    m_pendingProjectListAction = QStringLiteral("delete");
    m_pendingProjectId = projectId;
    UIG().deleteProject(projectId);
}

void HomePanel::onUiEventNotify(int uiEvent, const QVariantList& params)
{
    switch (uiEvent) {
    case UIGlobalState::ProjectListDataChanged:
        refreshProjectListFromGlobalState();
        if (m_myProjectsDialog && m_myProjectsDialog->isVisible()) {
            m_myProjectsDialog->refresh();
        }
        if (m_waitingForCreatePrompt && UIG().projectListState() == UIGlobalState::ProjectListError) {
            m_waitingForCreatePrompt = false;
        }
        if (!m_pendingProjectListAction.isEmpty()) {
            if (UIG().projectListState() == UIGlobalState::ProjectListError) {
                TnUi::showWarning(u8"提示", UIG().projectListError());
            } else if (m_pendingProjectListAction == QStringLiteral("delete") && m_pendingProjectId > 0) {
                if (m_recentHistory.removeByProjectId(m_pendingProjectId)) {
                    saveRecentProjects();
                    refreshQuickStartList();
                }
            }
            m_pendingProjectListAction.clear();
            m_pendingProjectId = 0;
        }
        break;
    case UIGlobalState::ProjectCreated: {
        if (!m_waitingForCreatePrompt) {
            break;
        }
        m_waitingForCreatePrompt = false;

        if (params.isEmpty()) {
            break;
        }
        const QVariantMap payload = params.first().toMap();
        const int createdProjectId = payload.value(QStringLiteral("projectId")).toInt();
        if (createdProjectId <= 0) {
            TnUi::showInformation(u8"创建成功", u8"创建成功但未返回项目ID，请在列表中打开");
            break;
        }
        if (TnUi::confirmYesNo(u8"创建成功", u8"项目已创建，是否立即打开该项目？")) {
            openProject(createdProjectId);
        }
        break;
    }
    default:
        break;
    }
}

void HomePanel::initUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("homePanel"));
    setStyleSheet(
        "#homePanel {"
        "   background-color: #030d20;"
        "}"
        "#homePanel QScrollArea {"
        "   border: none;"
        "   background: transparent;"
        "}"
        "#homePanel QScrollBar:vertical {"
        "   width: 10px;"
        "   margin: 0;"
        "   background: transparent;"
        "}"
        "#homePanel QScrollBar::handle:vertical {"
        "   background: rgba(63, 135, 236, 0.35);"
        "   border-radius: 5px;"
        "   min-height: 28px;"
        "}"
    );

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);

    QWidget* contentWidget = new QWidget(m_scrollArea);
    contentWidget->setObjectName(QStringLiteral("homeContent"));
    contentWidget->setStyleSheet(
        "#homeContent {"
        "   background: qradialgradient(cx:0.52, cy:0.14, radius:1.12,"
        "       fx:0.52, fy:0.10,"
        "       stop:0 rgba(18, 64, 138, 0.30),"
        "       stop:0.32 rgba(7, 26, 62, 0.55),"
        "       stop:0.62 rgba(3, 14, 34, 0.96),"
        "       stop:1 #020917);"
        "}"
    );

    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(64, 18, 64, 30);
    contentLayout->setSpacing(18);

    contentLayout->addWidget(createTitleArea());
    // TNP_HOME_HIDE_FEATURE_CARDS
    // contentLayout->addWidget(createFeatureCardArea());

    QHBoxLayout* middleLayout = new QHBoxLayout();
    middleLayout->setSpacing(20);
    middleLayout->setAlignment(Qt::AlignTop);
    middleLayout->addWidget(createQuickStartModule(), 2);
    middleLayout->addWidget(createProjectManagementModule(), 1, Qt::AlignTop);
    contentLayout->addLayout(middleLayout, 1);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(20);
    bottomLayout->addWidget(createHelpModule(), 1);
    bottomLayout->addWidget(createContactModule(), 1);
    contentLayout->addLayout(bottomLayout);

    m_scrollArea->setWidget(contentWidget);
    m_mainLayout->addWidget(m_scrollArea);

    applyBaseConfigFromSession();
}

void HomePanel::refreshProjectListFromGlobalState()
{
    const QList<ProjectItemJson> rawItems = UIG().projectItems();
    QList<ProjectItem> items;
    items.reserve(rawItems.size());

    for (const ProjectItemJson& raw : rawItems) {
        ProjectItem item;
        item.id = raw.j_id();
        item.name = raw.j_name();
        item.description = raw.j_desc();
        item.createTime = QDateTime::fromString(raw.j_created_at(), Qt::ISODate);
        if (!item.createTime.isValid()) {
            item.createTime = QDateTime::currentDateTime();
        }
        item.caseCount = raw.j_num();
        item.status = raw.j_status() == 1 ? QString(u8"进行中") : QString(u8"已完成");
        item.path.clear();
        item.provinceId = raw.j_province_id();
        item.provinceName = raw.j_province().trimmed();
        item.cityId = raw.j_city_id();
        item.cityName = raw.j_city().trimmed();
        items.append(item);
    }

    m_projectModel->setDatas(items);
    if (m_recentHistory.enrichFromProjectItems(items)) {
        saveRecentProjects();
    }
    refreshQuickStartList();
}

void HomePanel::refreshQuickStartList()
{
    if (!m_quickStartListLayout || !m_quickStartListFrame || !m_quickStartListScrollArea
        || !m_quickStartListScrollContent) {
        return;
    }

    clearQuickStartListLayout(m_quickStartListLayout);

    const QList<RecentProjectEntry> entries = m_recentHistory.recentItems();
    const int maxCount = qMin(entries.size(), RecentProjectHistory::maxDisplayedEntries());

    m_quickStartListScrollArea->setMinimumHeight(entries.isEmpty() ? HomePanelConsts::kQuickStartEmptyScrollHeight
                                                                   : HomePanelConsts::kQuickStartScrollMinHeight);

    QWidget* const rowParent = m_quickStartListScrollContent;
    for (int i = 0; i < maxCount; ++i) {
        const RecentProjectEntry entry = entries.at(i);
        const QString regionText = resolveQuickStartRegionText(entry, m_projectModel);
        auto* row =
            new QuickStartRowWidget(entry,
                                     regionText,
                                     [this](const RecentProjectEntry& e) {
                                         openRecentEntry(e);
                                     },
                                     rowParent);
        m_quickStartListLayout->addWidget(row);
    }

    if (entries.isEmpty()) {
        auto* emptyLabel =
            new QLabel(u8"暂无最近打开的项目，请在右侧「项目管理」中新建或打开项目。", rowParent);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setMinimumHeight(72);
        emptyLabel->setStyleSheet(
            "QLabel {"
            "   color: rgba(171, 188, 214, 0.78);"
            "   padding: 22px 8px;"
            "   font-size: 14px;"
            "}"
        );
        m_quickStartListLayout->addWidget(emptyLabel);
    }

    m_quickStartListLayout->addStretch();

    m_quickStartListScrollContent->adjustSize();
    m_quickStartListScrollArea->updateGeometry();
    m_quickStartListFrame->updateGeometry();
    if (m_scrollArea && m_scrollArea->widget()) {
        m_scrollArea->widget()->updateGeometry();
    }
}

void HomePanel::createProject(const QString& name, const QString& description, const QString& path, bool refreshQuickStartUi)
{
    const ProjectItem item = makeProjectItem(m_projectModel->nextProjectId(),
                                             name,
                                             description.isEmpty() ? u8"新建交通网络预测项目" : description,
                                             QDateTime::currentDateTime(),
                                             0,
                                             u8"草稿",
                                             path);

    m_projectModel->addProject(item);
    if (refreshQuickStartUi) {
        refreshQuickStartList();
    }
}

int HomePanel::findOrCreateProjectForPath(const QString& dir, const QString& fallbackName)
{
    const QString cleanDir = QDir::cleanPath(QFileInfo(dir).absoluteFilePath());
    const QList<ProjectItem> items = m_projectModel->datas();
    for (const ProjectItem& item : items) {
        if (!item.path.isEmpty()) {
            const QString itemPath = QDir::cleanPath(QFileInfo(item.path).absoluteFilePath());
            if (itemPath == cleanDir) {
                return item.id;
            }
        }
    }

    const QFileInfo info(dir);
    const QString name = fallbackName.isEmpty()
                             ? (info.fileName().isEmpty() ? QString(u8"未命名项目") : info.fileName())
                             : fallbackName;
    createProject(name, u8"从本地目录导入的项目", dir, false);
    return m_projectModel->datas().constLast().id;
}

void HomePanel::removeRecentEntryAndPersist(const RecentProjectEntry& entry)
{
    if (!m_recentHistory.removeEntry(entry)) {
        return;
    }
    saveRecentProjects();
    refreshQuickStartList();
}

void HomePanel::openRecentEntry(const RecentProjectEntry& entry)
{
    if (!entry.path.isEmpty()) {
        const QString cleanPath = QDir::cleanPath(QFileInfo(entry.path).absoluteFilePath());
        if (!QDir(cleanPath).exists()) {
            removeRecentEntryAndPersist(entry);
            TnUi::showInformation(u8"提示", u8"项目目录已不存在，已从最近打开列表中移除。");
            return;
        }
        const int projectId = findOrCreateProjectForPath(entry.path, entry.name);
        openProject(projectId);
        return;
    }

    ProjectItem tmp;
    if (m_projectModel->projectById(entry.projectId, tmp)) {
        openProject(entry.projectId);
        return;
    }

    removeRecentEntryAndPersist(entry);
    TnUi::showInformation(u8"提示", u8"该项目已不存在于当前项目列表中，已从最近打开列表中移除。");
}

void HomePanel::openProject(int projectId)
{
    if (projectId <= 0) {
        return;
    }
    if (UIG().isProjectOpening()) {
        return;
    }

    ProjectItem item;
    if (m_projectModel->projectById(projectId, item)) {
        m_recentHistory.upsertFromProjectItem(item);
    }
    emit projectOpened(projectId);
    refreshQuickStartList();
}

QWidget* HomePanel::createTitleArea()
{
    QFrame* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("heroFrame"));
    frame->setMinimumHeight(236);
    frame->setStyleSheet(
        "#heroFrame {"
        "   background: transparent;"
        "   border: none;"
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(36, 26, 36, 24);
    layout->setSpacing(12);

    m_heroMainTitleLabel = new QLabel(defaultHeroSiteName(), frame);
    m_heroMainTitleLabel->setAlignment(Qt::AlignCenter);
    m_heroMainTitleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 30px;"
        "   font-weight: 700;"
        "}"
    );

    m_heroSubTitleLabel = new QLabel(defaultHeroSubLine(), frame);
    m_heroSubTitleLabel->setAlignment(Qt::AlignCenter);
    m_heroSubTitleLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(255, 255, 255, 0.95);"
        "   font-size: 16px;"
        "   font-weight: 600;"
        "}"
    );

    m_heroDescLabel = new QLabel(defaultHeroDescription(), frame);
    m_heroDescLabel->setAlignment(Qt::AlignCenter);
    m_heroDescLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(194, 210, 236, 0.78);"
        "   font-size: 15px;"
        "   line-height: 1.6;"
        "}"
    );

    layout->addStretch();
    layout->addWidget(m_heroMainTitleLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_heroSubTitleLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_heroDescLabel, 0, Qt::AlignHCenter);
    layout->addStretch();

    return frame;
}

QWidget* HomePanel::createFeatureCardArea()
{
    QWidget* container = new QWidget(this);
    container->setObjectName(QStringLiteral("featureCardArea"));
    container->setStyleSheet(
        "#featureCardArea {"
        "   background: transparent;"
        "}"
    );
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(18, 4, 18, 8);
    layout->setSpacing(150);
    layout->setAlignment(Qt::AlignCenter);

    m_planningCard = new FeatureCardWidget(FeatureCardType::Planning, container);
    m_dataManageCard = new FeatureCardWidget(FeatureCardType::DataManage, container);
    m_evaluationCard = new FeatureCardWidget(FeatureCardType::Evaluation, container);
    m_simulationCard = new FeatureCardWidget(FeatureCardType::Simulation, container);
    m_publishCard = new FeatureCardWidget(FeatureCardType::Publish, container);

    const QList<FeatureCardWidget*> cards = {
        m_planningCard, m_dataManageCard, m_evaluationCard, m_simulationCard, m_publishCard
    };

    for (FeatureCardWidget* card : cards) {
        connect(card, &FeatureCardWidget::cardClicked, this, &HomePanel::onFeatureCardClicked);
        layout->addWidget(card);
    }

    m_planningCard->setSelected(true);

    return container;
}

QWidget* HomePanel::createQuickStartModule()
{
    QFrame* frame = createSectionFrame();
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(frame->layout());

    QWidget* headerWidget = new QWidget(frame);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    QLabel* titleLabel = new QLabel(u8"快速开始", headerWidget);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );

    m_myProjectsButton = new QPushButton(u8"我的项目", headerWidget);
    m_myProjectsButton->setCursor(Qt::PointingHandCursor);
    m_myProjectsButton->setStyleSheet(
        "QPushButton {"
        "   min-width: 88px;"
        "   min-height: 32px;"
        "   padding: 0 16px;"
        "   border-radius: 16px;"
        "   border: none;"
        "   background-color: #1e86ff;"
        "   color: white;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3197ff;"
        "}"
    );
    connect(m_myProjectsButton, &QPushButton::clicked, this, &HomePanel::onMyProjectsClicked);

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_myProjectsButton);
    layout->addWidget(headerWidget);

    QFrame* listFrame = new QFrame(frame);
    m_quickStartListFrame = listFrame;
    listFrame->setObjectName(QStringLiteral("quickStartListFrame"));
    listFrame->setStyleSheet(
        "#quickStartListFrame {"
        "   border-radius: 12px;"
        "   border: 1px solid rgba(32, 72, 132, 0.42);"
        "   background-color: rgba(2, 12, 30, 0.85);"
        "}"
    );

    auto* listOuterLayout = new QVBoxLayout(listFrame);
    listOuterLayout->setContentsMargins(0, 0, 0, 0);
    listOuterLayout->setSpacing(0);

    auto* listScroll = new QScrollArea(listFrame);
    m_quickStartListScrollArea = listScroll;
    listScroll->setObjectName(QStringLiteral("quickStartListScroll"));
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    listScroll->setWidgetResizable(true);
    listScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    listScroll->setMinimumHeight(HomePanelConsts::kQuickStartScrollMinHeight);
    listScroll->setStyleSheet(
        "QScrollArea#quickStartListScroll {"
        "   border: none;"
        "   background: transparent;"
        "}"
        "QScrollArea#quickStartListScroll > QWidget > QWidget {"
        "   background: transparent;"
        "}"
        "QScrollArea#quickStartListScroll QScrollBar:vertical {"
        "   width: 10px;"
        "   margin: 0;"
        "   background: transparent;"
        "}"
        "QScrollArea#quickStartListScroll QScrollBar::handle:vertical {"
        "   background: rgba(63, 135, 236, 0.35);"
        "   border-radius: 5px;"
        "   min-height: 28px;"
        "}"
    );

    auto* scrollContent = new QWidget();
    m_quickStartListScrollContent = scrollContent;
    scrollContent->setObjectName(QStringLiteral("quickStartScrollContent"));
    scrollContent->setAttribute(Qt::WA_StyledBackground, false);

    m_quickStartListLayout = new QVBoxLayout(scrollContent);
    m_quickStartListLayout->setContentsMargins(16, 12, 16, 12);
    m_quickStartListLayout->setSpacing(HomePanelConsts::kQuickStartRowSpacing);
    m_quickStartListLayout->setAlignment(Qt::AlignTop);
    scrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    listScroll->setWidget(scrollContent);
    listOuterLayout->addWidget(listScroll, 1);

    layout->addWidget(listFrame, 1);

    return frame;
}

QWidget* HomePanel::createProjectManagementModule()
{
    QFrame* frame = createSectionFrame();
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(frame->layout());

    QLabel* titleLabel = new QLabel(u8"项目管理", frame);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );
    layout->addWidget(titleLabel);

    QHBoxLayout* actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(18);

    QPushButton* newButton = createActionButton(QStringLiteral(":/icons/icons/add_project@2x.png"), u8"新建项目", QStringLiteral("newProjectButton"));
    QPushButton* openButton = createActionButton(QStringLiteral(":/icons/icons/folder_open@2x.png"), u8"打开项目", QStringLiteral("openProjectButton"));
    QPushButton* teachingButton = createActionButton(QStringLiteral(":/icons/icons/teaching_mode@2x.png"), u8"教学案例", QStringLiteral("teachingCaseButton"));

    connect(newButton, &QPushButton::clicked, this, &HomePanel::onNewProjectClicked);
    connect(openButton, &QPushButton::clicked, this, &HomePanel::onOpenProjectClicked);
    connect(teachingButton, &QPushButton::clicked, this, &HomePanel::onTeachingCasesClicked);

    actionsLayout->addWidget(newButton, 1);
    actionsLayout->addWidget(openButton, 1);
    actionsLayout->addWidget(teachingButton, 1);
    layout->addLayout(actionsLayout);
    layout->addStretch();

    return frame;
}

QWidget* HomePanel::createHelpModule()
{
    QFrame* frame = createSectionFrame();
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(frame->layout());

    QLabel* titleLabel = new QLabel(u8"帮助文档", frame);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );
    layout->addWidget(titleLabel);

    m_helpDocsGridLayout = new QGridLayout();
    m_helpDocsGridLayout->setContentsMargins(6, 8, 6, 0);
    m_helpDocsGridLayout->setHorizontalSpacing(24);
    m_helpDocsGridLayout->setVerticalSpacing(16);

    layout->addLayout(m_helpDocsGridLayout);
    layout->addStretch();

    rebuildHelpDocButtons();

    return frame;
}

void HomePanel::rebuildHelpDocButtons()
{
    if (!m_helpDocsGridLayout) {
        return;
    }

    while (QLayoutItem* item = m_helpDocsGridLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    struct HelpEntry {
        QString title;
        QString url;
    };

    QVector<HelpEntry> entries;
    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();
    if (cfg) {
        const QList<BaseConfigHelpItemJson> list = cfg->j_help_list();
        entries.reserve(list.size());
        for (const BaseConfigHelpItemJson& h : list) {
            entries.append({h.j_title(), h.j_url()});
        }
    }

    if (entries.isEmpty()) {
        entries = {{QString::fromUtf8(u8"方案配置"), QString()},
                   {QString::fromUtf8(u8"数据库建立"), QString()},
                   {QString::fromUtf8(u8"基本原理"), QString()},
                   {QString::fromUtf8(u8"图形编辑"), QString()}};
    }

    static const QStringList kFallbackIcons = {
        QStringLiteral(":/icons/icons/settings@2x.png"),
        QStringLiteral(":/icons/icons/database@2x.png"),
        QStringLiteral(":/icons/icons/document@2x.png"),
        QStringLiteral(":/icons/icons/view_1@2x.png")
    };

    for (int i = 0; i < entries.size(); ++i) {
        const QString iconPath = kFallbackIcons.at(i % kFallbackIcons.size());
        const QString btnTitle = entries.at(i).title;
        const QString btnUrl = entries.at(i).url;
        QPushButton* button =
            createActionButton(iconPath, btnTitle.isEmpty() ? QString::fromUtf8(u8"帮助") : btnTitle, QStringLiteral("helpButton"));
        connect(button, &QPushButton::clicked, this, [this, btnTitle, btnUrl]() {
            const QString titleBase = btnTitle.isEmpty() ? QString::fromUtf8(u8"帮助文档") : btnTitle;
            openWebViewUrlOrWarn(this, titleBase, btnUrl);
        });
        m_helpDocsGridLayout->addWidget(button, i / 4, i % 4);
    }
}

QWidget* HomePanel::createContactModule()
{
    QFrame* frame = createSectionFrame();
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(frame->layout());

    QLabel* titleLabel = new QLabel(u8"联系我们", frame);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: 700;"
        "}"
    );
    layout->addWidget(titleLabel);

    QGridLayout* contactLayout = new QGridLayout();
    contactLayout->setContentsMargins(0, 8, 0, 0);
    contactLayout->setHorizontalSpacing(30);
    contactLayout->setVerticalSpacing(18);

    struct ContactItem {
        QString icon;
        int row;
        int column;
    };

    const QList<ContactItem> contactItems = {
        {QStringLiteral(":/icons/icons/user@2x.png"), 0, 0},
        {QStringLiteral(":/icons/icons/email@2x.png"), 0, 1},
        {QStringLiteral(":/icons/icons/email_2@2x.png"), 0, 2},
        {QStringLiteral(":/icons/icons/phone@2x.png"), 1, 0},
        {QStringLiteral(":/icons/icons/location@2x.png"), 1, 1}
    };

    m_contactTextLabels.clear();
    const QList<QString>& contactLines = defaultContactLines();

    for (int i = 0; i < contactItems.size(); ++i) {
        const ContactItem& item = contactItems.at(i);
        QWidget* cell = new QWidget(frame);
        QHBoxLayout* cellLayout = new QHBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(10);

        QLabel* iconLabel = new QLabel(cell);
        iconLabel->setFixedSize(18, 18);
        iconLabel->setPixmap(scaledPixmapForBox(item.icon, iconLabel->size(), 1.0));

        QLabel* textLabel = new QLabel(contactLines.at(i), cell);
        textLabel->setStyleSheet(
            "QLabel {"
            "   color: rgba(221, 232, 248, 0.88);"
            "   font-size: 14px;"
            "}"
        );

        m_contactTextLabels.append(textLabel);

        cellLayout->addWidget(iconLabel, 0, Qt::AlignTop);
        cellLayout->addWidget(textLabel, 1);
        contactLayout->addWidget(cell, item.row, item.column);
    }

    layout->addLayout(contactLayout);
    layout->addStretch();

    return frame;
}

QPushButton* HomePanel::createActionButton(const QString& iconPath, const QString& title, const QString& objectName)
{
    QPushButton* button = new QPushButton(this);
    button->setObjectName(objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumSize(108, 112);
    button->setStyleSheet(
        "QPushButton {"
        "   border-radius: 16px;"
        "   border: 1px solid rgba(43, 107, 190, 0.55);"
        "   background-color: rgba(6, 22, 49, 0.70);"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(11, 35, 77, 0.84);"
        "   border-color: rgba(56, 160, 255, 0.85);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(13, 44, 94, 0.95);"
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(button);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);

    QLabel* iconBox = new QLabel(button);
    iconBox->setFixedSize(56, 56);
    iconBox->setAlignment(Qt::AlignCenter);
    iconBox->setStyleSheet(
        "QLabel {"
        "   border-radius: 14px;"
        "   background-color: rgba(30, 134, 255, 0.12);"
        "   border: 1px dashed rgba(50, 142, 255, 0.58);"
        "}"
    );
    iconBox->setPixmap(scaledPixmapForBox(iconPath, iconBox->size(), 0.84));

    QLabel* titleLabel = new QLabel(title, button);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(235, 243, 255, 0.96);"
        "   font-size: 14px;"
        "   font-weight: 600;"
        "   background: transparent;"
        "}"
    );

    layout->addStretch();
    layout->addWidget(iconBox, 0, Qt::AlignCenter);
    layout->addWidget(titleLabel);
    layout->addStretch();

    return button;
}

QFrame* HomePanel::createSectionFrame()
{
    QFrame* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("sectionFrame"));
    frame->setStyleSheet(
        "#sectionFrame {"
        "   border-radius: 18px;"
        "   border: 1px solid rgba(34, 91, 168, 0.45);"
        "   background-color: rgba(7, 23, 54, 0.84);"
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);
    return frame;
}
