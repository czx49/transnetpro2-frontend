/*****************************************************************//**
 * \file   mainwindow.cpp
 * \brief  主窗口实现
 * \details 负责首页和项目工作台的动态标签切换
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "mainwindow.h"

#include "appsession.h"
#include "casepanel.h"
#include "framelessdialog.h"
#include "homepanel.h"
#include "httpapijson.h"
#include "loadingdialog.h"
#include "uiglobalstate.h"
#include "webview2dialog.h"

#include "../casectrls/casewfsmapcontroller.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFrame>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include "../publicctrls/messagedialog.h"
#include <QPointer>
#include <QPushButton>
#include <QSharedPointer>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVariantMap>
#include <QVBoxLayout>

namespace {
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


class NavTabBar : public QTabBar
{
public:
    explicit NavTabBar(QWidget* parent = nullptr) : QTabBar(parent) {}
protected:
    QSize tabSizeHint(int index) const override
    {
        QSize sz = QTabBar::tabSizeHint(index);
        sz.rwidth() += 30; // 标签整体加宽30像素，文字区域扩容
        return sz;
    }
};


MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_navBar(nullptr)
    , m_menuButton(nullptr)
    , m_mainMenu(nullptr)
    , m_actionNewProject(nullptr)
    , m_actionOpenProject(nullptr)
    , m_actionManual(nullptr)
    , m_actionAbout(nullptr)
    , m_titleLabel(nullptr)
    , m_tabBar(nullptr)
    , m_panelStack(new QStackedWidget(this))
    , m_homePanel(nullptr)
    , m_projectOpenLoadingDelayTimer(new QTimer(this))
    , m_openingProjectId(0)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("mainWindow"));
    setStyleSheet(
        "#mainWindow {"
        "   background-color: #020a18;"
        "}"
    );

    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initNavigationBar();
    createPanels();

    m_projectOpenLoadingDelayTimer->setSingleShot(true);
    connect(m_projectOpenLoadingDelayTimer, &QTimer::timeout, this, &MainWindow::showProjectOpenLoading);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        flushRecentProjects();
        CaseWfsMap::prepareForShutdown();
        for (CasePanel* panel : m_projectPanels) {
            if (panel) {
                panel->prepareForClose();
            }
        }
    });

    connect(&AppSession::instance(), &AppSession::baseConfigChanged, this, &MainWindow::applyWindowTitleFromBaseConfig);
    applyWindowTitleFromBaseConfig();

    setMinimumWidth(960);
    setMinimumHeight(0);
}

MainWindow::~MainWindow()
{
}

void MainWindow::applyWindowTitleFromBaseConfig()
{
    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();
    const QString title = (cfg && !cfg->j_site_name().isEmpty())
        ? cfg->j_tit1()
        : QString::fromUtf8(u8"城市道路交通网络预测与分析平台");
    setWindowTitle(title);
    if (QWidget* top = window()) {
        top->setWindowTitle(title);
    }
}

void MainWindow::flushRecentProjects()
{
    if (m_homePanel) {
        m_homePanel->saveRecentProjects();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    flushRecentProjects();
    event->accept();
    QApplication::quit();
}

void MainWindow::openProjectFromHome(int projectId)
{
    if (!m_homePanel || projectId <= 0) {
        return;
    }

    if (UIG().isProjectOpening()) {
        return;
    }

    const int existingTab = findTabByProjectId(projectId);
    if (existingTab >= 0) {
        m_homePanel->recordLatestOpenedProject(projectId);
        activateProjectTab(projectId);
        return;
    }

    m_openingProjectId = projectId;
    m_homePanel->recordLatestOpenedProject(projectId);
    m_projectOpenLoadingDelayTimer->start(300);
    UIG().openProject(projectId);
}

int MainWindow::addTabItem(int projectId, const QString& tabName)
{
    const int tabIndex = m_tabBar->addTab(tabName);

    QWidget* btnWrap = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(btnWrap);
    layout->setContentsMargins(0, 0, 20, 0);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QPushButton* closeBtn = new QPushButton();
    const int btnSize = 16;
    closeBtn->setFixedSize(btnSize, btnSize);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            border: none;
            background: transparent;
            image: url(:/icons/icons/close_1.png);
        }
        QPushButton:hover {
            image: url(:/icons/icons/close.png);
        }
    )");
    closeBtn->setIconSize(QSize(btnSize, btnSize));

    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, [this, projectId]() {
        closeProjectTab(projectId);
    });

    m_tabBar->setTabButton(tabIndex, QTabBar::RightSide, btnWrap);
    return tabIndex;
}

void MainWindow::showTabContent(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= m_tabBar->count()) {
        return;
    }

    const int projectId = m_tabBar->tabData(tabIndex).toInt();
    if (projectId == 0) {
        m_panelStack->setCurrentWidget(m_homePanel);
        if (m_homePanel) {
            m_homePanel->refreshQuickStartFromHistory();
        }
        return;
    }

    CasePanel* panel = m_projectPanels.value(projectId, nullptr);
    if (panel) {
        m_panelStack->setCurrentWidget(panel);
    }
}

void MainWindow::closeProjectTab(int projectId)
{
    if (projectId <= 0) {
        return;
    }

    const int tabIndex = findTabByProjectId(projectId);
    if (tabIndex <= 0) {
        return;
    }

    CasePanel* panel = m_projectPanels.take(projectId);
    m_tabBar->removeTab(tabIndex);

    if (panel) {
        panel->prepareForClose();
        m_panelStack->removeWidget(panel);
        delete panel;
    }

    m_tabBar->setCurrentIndex(0);
}

void MainWindow::onUiEventNotify(int uiEvent, const QVariantList& params)
{
    if (uiEvent != UIGlobalState::ProjectOpenStateChanged || params.isEmpty()) {
        return;
    }

    const QVariantMap payload = params.first().toMap();
    const int stateValue = payload.value(QStringLiteral("state")).toInt();
    const int projectId = payload.value(QStringLiteral("projectId")).toInt();
    const QString errorMessage = payload.value(QStringLiteral("error")).toString();
    if (projectId <= 0) {
        return;
    }

    if (m_openingProjectId > 0 && projectId != m_openingProjectId) {
        return;
    }

    switch (stateValue) {
    case UIGlobalState::ProjectOpenLoading:
        break;
    case UIGlobalState::ProjectOpenReady:
    case UIGlobalState::ProjectOpenPartialReady: {
        const QString projectName = resolveProjectName(projectId);
        int tabIndex = findTabByProjectId(projectId);
        if (tabIndex < 0) {
            CasePanel* panel = createProjectPanel(projectId, projectName);
            m_projectPanels.insert(projectId, panel);
            m_panelStack->addWidget(panel);

            tabIndex = addTabItem(projectId, projectName);
            m_tabBar->setTabData(tabIndex, projectId);
            m_tabBar->setTabsClosable(false);
        } else if (!projectName.isEmpty()) {
            m_tabBar->setTabText(tabIndex, projectName);
            CasePanel* panel = m_projectPanels.value(projectId, nullptr);
            if (panel) {
                panel->setProjectName(projectName);
            }
        }

        m_tabBar->setCurrentIndex(tabIndex);
        showTabContent(tabIndex);
        hideProjectOpenLoading();
        break;
    }
    case UIGlobalState::ProjectOpenError:
        hideProjectOpenLoading();
        if (!errorMessage.isEmpty()) {
            TnUi::showWarning(QString::fromUtf8(u8"提示"), errorMessage);
        }
        break;
    default:
        break;
    }
}

void MainWindow::showProjectOpenLoading()
{
    if (!UIG().isProjectOpening()) {
        return;
    }
    if (m_projectOpenLoading) {
        return;
    }

    auto* loading = new LoadingDialog(QString::fromUtf8(u8"正在打开项目…"), nullptr);
    m_projectOpenLoading = loading;
    loading->showNonBlocking();
}

void MainWindow::hideProjectOpenLoading()
{
    if (m_projectOpenLoadingDelayTimer) {
        m_projectOpenLoadingDelayTimer->stop();
    }
    if (m_projectOpenLoading) {
        m_projectOpenLoading->closeDialog();
        m_projectOpenLoading.clear();
    }
    m_openingProjectId = 0;
}

void MainWindow::activateProjectTab(int projectId)
{
    const int tabIndex = findTabByProjectId(projectId);
    if (tabIndex < 0) {
        return;
    }

    m_tabBar->setCurrentIndex(tabIndex);
    showTabContent(tabIndex);
}

void MainWindow::onTabChanged(int index)
{
    showTabContent(index);
}

void MainWindow::onTabCloseRequested(int index)
{
    if (index <= 0 || index >= m_tabBar->count()) {
        return;
    }

    closeProjectTab(m_tabBar->tabData(index).toInt());
}

void MainWindow::initNavigationBar()
{
    m_navBar = new QFrame(this);
    m_navBar->setObjectName(QStringLiteral("navBar"));
    m_navBar->setStyleSheet(
        "#navBar {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "       stop:0 #040b1b, stop:0.6 #07132b, stop:1 #041127);"
        "   border-bottom: 1px solid rgba(40, 82, 145, 0.45);"
        "}"
    );

    QVBoxLayout* navLayout = new QVBoxLayout(m_navBar);
    navLayout->setContentsMargins(18, 8, 18, 0);
    navLayout->setSpacing(8);

    QWidget* brandRow = new QWidget(m_navBar);
    QHBoxLayout* brandLayout = new QHBoxLayout(brandRow);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(10);

    m_menuButton = new QToolButton(brandRow);
    m_menuButton->setFixedSize(22, 22);
    m_menuButton->setIconSize(QSize(14, 14));
    m_menuButton->setIcon(
        QIcon(scaledPixmapForBox(QStringLiteral(":/icons/icons/navigation@2x.png"), m_menuButton->iconSize(), 1.0)));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    m_menuButton->setAutoRaise(true);
    m_menuButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_menuButton->setCursor(Qt::PointingHandCursor);
    m_menuButton->setStyleSheet(
        "QToolButton {"
        "   border: none;"
        "   background: transparent;"
        "   padding: 0;"
        "}"
        "QToolButton::menu-indicator {"
        "   image: none;"
        "}");

    m_mainMenu = new QMenu(m_menuButton);
    m_mainMenu->setStyleSheet(
        "QMenu {"
        "   background-color: #1e52b4;"
        "   border: 1px solid rgba(255, 255, 255, 0.18);"
        "   border-radius: 10px;"
        "   padding: 8px 6px;"
        "}"
        "QMenu::item {"
        "   padding: 10px 36px 10px 14px;"
        "   color: rgba(255, 255, 255, 0.95);"
        "   border-radius: 6px;"
        "   font-size: 14px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: rgba(255, 255, 255, 0.14);"
        "}");

    const QSize menuIconBox(18, 18);
    m_actionNewProject =
        m_mainMenu->addAction(QIcon(scaledPixmapForBox(QStringLiteral(":/icons/icons/add_project@2x.png"), menuIconBox, 1.0)),
                              QString::fromUtf8(u8"新建项目"));
    m_actionOpenProject =
        m_mainMenu->addAction(QIcon(scaledPixmapForBox(QStringLiteral(":/icons/icons/folder_open@2x.png"), menuIconBox, 1.0)),
                              QString::fromUtf8(u8"打开项目"));
    m_actionManual =
        m_mainMenu->addAction(QIcon(scaledPixmapForBox(QStringLiteral(":/icons/icons/document@2x.png"), menuIconBox, 1.0)),
                              QString::fromUtf8(u8"使用手册"));
    m_actionAbout =
        m_mainMenu->addAction(QIcon(scaledPixmapForBox(QStringLiteral(":/icons/icons/navigation@2x.png"), menuIconBox, 1.0)),
                              QString::fromUtf8(u8"关于"));

    m_menuButton->setMenu(m_mainMenu);

    connect(m_actionManual, &QAction::triggered, this, &MainWindow::openUserManualFromConfig);
    connect(m_actionAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);

    QLabel* logoLabel = new QLabel(brandRow);
    logoLabel->setFixedSize(22, 22);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setPixmap(QIcon(QStringLiteral(":/icons/icon.ico")).pixmap(22, 22));

    m_titleLabel = new QLabel(u8"城市道路交通网络预测与分析平台", brandRow);
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 14px;"
        "   font-weight: 700;"
        "}"
    );

    brandLayout->addWidget(m_menuButton);
    brandLayout->addWidget(logoLabel);
    brandLayout->addWidget(m_titleLabel);
    brandLayout->addStretch();

    QPushButton* aiPlanButton = new QPushButton(QString::fromUtf8(u8"AI规划助手"), brandRow);
    aiPlanButton->setCursor(Qt::PointingHandCursor);
    aiPlanButton->setFixedHeight(28);
    aiPlanButton->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 14px;"
        "   background-color: #1f84ff;"
        "   color: white;"
        "   font-size: 13px;"
        "   font-weight: 700;"
        "   padding: 0 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3b94ff;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #1874e6;"
        "}"
    );
    connect(aiPlanButton, &QPushButton::clicked, this, &MainWindow::openAiPlanningAssistant);
    brandLayout->addWidget(aiPlanButton);
    navLayout->addWidget(brandRow);

    m_tabBar = new NavTabBar(m_navBar);
    m_tabBar->setMovable(false);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setDrawBase(false);
    m_tabBar->setTabsClosable(false);

    m_tabBar->setStyleSheet(R"(
    QTabBar::tab {
        min-width: 108px;
        max-width: 300px;
        min-height: 42px;
        margin-right: 10px;
        /* 从64缩减到48，释放大量文字空间 */
        padding: 0 10px 0 14px;
        text-align: left;
        color: rgba(181, 203, 233, 0.86);
        background-color: transparent;
        border-top-left-radius: 10px;
        border-top-right-radius: 10px;
        font-size: 16px;
        font-weight: 600;
    }
    QTabBar::tab:selected {
        color: #0683f5;
        background-color: rgba(255, 255, 255, 0.06);
    }
    QTabBar::tab:hover:!selected {
        color: white;
        background-color: rgba(255, 255, 255, 0.025);
    }
    )");

    connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    navLayout->addWidget(m_tabBar);

    m_mainLayout->addWidget(m_navBar);
}

void MainWindow::createPanels()
{
    m_homePanel = new HomePanel(this);
    wireMainMenuActions();
    connect(m_homePanel, &HomePanel::projectOpened, this, &MainWindow::openProjectFromHome);
    connect(&UIG(), &UIGlobalState::uiEventNotify, this, &MainWindow::onUiEventNotify);

    m_panelStack->setFrameShape(QFrame::NoFrame);
    m_panelStack->addWidget(m_homePanel);
    m_mainLayout->addWidget(m_panelStack, 1);

    const int homeTabIndex = m_tabBar->addTab(u8"起始页");
    m_tabBar->setTabData(homeTabIndex, 0);
    //m_tabBar->tabButton(homeTabIndex, QTabBar::RightSide)->hide();
    m_tabBar->setCurrentIndex(homeTabIndex);
}

void MainWindow::wireMainMenuActions()
{
    if (!m_homePanel || !m_actionNewProject || !m_actionOpenProject) {
        return;
    }

    connect(m_actionNewProject, &QAction::triggered, m_homePanel, &HomePanel::triggerNewProject, Qt::UniqueConnection);
    connect(m_actionOpenProject, &QAction::triggered, m_homePanel, &HomePanel::triggerOpenProject, Qt::UniqueConnection);
}

void MainWindow::openUserManualFromConfig()
{
    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();
    const QString url = cfg ? cfg->j_manual_url() : QString();
    openWebViewUrlOrWarn(this, QString::fromUtf8(u8"使用手册"), url);
}

void MainWindow::openAiPlanningAssistant()
{
    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();
    const QString url = cfg ? cfg->j_ai_url() : QString();
    openWebViewUrlOrWarn(this, QString::fromUtf8(u8"AI规划助手"), url);
}

void MainWindow::showAboutDialog()
{
    FramelessDialog dialog(nullptr);
    dialog.setTitleText(QString::fromUtf8(u8"关于"));
    dialog.setMinimizeButtonVisible(false);
    dialog.setMaximizeButtonVisible(false);
    dialog.resize(440, 300);
    dialog.setStyleSheet(Phase2Ui::dialogShellStyleSheet());

    QVBoxLayout* layout = dialog.contentLayout();
    layout->setContentsMargins(24, 16, 24, 20);
    layout->setSpacing(14);

    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();
    const QString siteName =
        cfg && !cfg->j_site_name().isEmpty()
            ? cfg->j_site_name()
            : QString::fromUtf8(u8"城市道路交通网络预测与分析平台");

    const QSharedPointer<VersionResponseJson> verInfo = AppSession::instance().versionInfo();
    const QString verText =
        verInfo && !verInfo->j_version().isEmpty() ? verInfo->j_version() : QString::fromUtf8(u8"—");

    auto* nameLabel = new QLabel(siteName, &dialog);
    nameLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffffff; font-size: 17px; font-weight: 700; background: transparent; }"));

    auto* versionLabel = new QLabel(QString::fromUtf8(u8"版本：%1").arg(verText), &dialog);
    versionLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #cbd5e1; font-size: 14px; background: transparent; }"));

    auto* descLabel = new QLabel(QString::fromUtf8(u8"TransNetPro2 桌面客户端"), &dialog);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #cbd5e1; font-size: 13px; background: transparent; }"));

    layout->addWidget(nameLabel);
    layout->addWidget(versionLabel);
    layout->addWidget(descLabel);
    layout->addStretch();

    auto* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), &dialog);
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    QObject::connect(okBtn, &QPushButton::clicked, &dialog, &FramelessDialog::accept);
    layout->addWidget(okBtn, 0, Qt::AlignHCenter);

    dialog.exec();
}

int MainWindow::findTabByProjectId(int projectId) const
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toInt() == projectId) {
            return i;
        }
    }

    return -1;
}

CasePanel* MainWindow::createProjectPanel(int projectId, const QString& projectName)
{
    CasePanel* panel = new CasePanel(this);
    panel->setProjectId(projectId);
    panel->setProjectName(projectName);
    return panel;
}

QString MainWindow::resolveProjectName(int projectId) const
{
    const QSharedPointer<ProjectDetailJson> detail = UIG().currentProjectDetail();
    if (!detail.isNull()) {
        const ProjectItemJson info = detail->j_info();
        if (info.j_id() == projectId && !info.j_name().isEmpty()) {
            return info.j_name();
        }
    }

    if (m_homePanel && m_homePanel->projectModel()) {
        ProjectItem project;
        if (m_homePanel->projectModel()->projectById(projectId, project) && !project.name.isEmpty()) {
            return project.name;
        }
    }

    return QString::fromUtf8(u8"项目%1").arg(projectId);
}
