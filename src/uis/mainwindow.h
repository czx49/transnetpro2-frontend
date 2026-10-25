/*****************************************************************//**
 * \file   mainwindow.h
 * \brief  主窗口
 * \details 管理顶部品牌栏、首页标签与动态项目标签
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QPointer>
#include <QWidget>

class HomePanel;
class CasePanel;
class LoadingDialog;
class QAction;
class QLabel;
class QMenu;
class QStackedWidget;
class QTabBar;
class QTimer;
class QToolButton;
class QVBoxLayout;
class QFrame;
class NavTabBar;

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /** 将首页最近打开列表写入磁盘（退出应用前调用） */
    void flushRecentProjects();
    /** 从 baseConfig 同步窗口标题到顶层 AppWindow */
    void applyWindowTitleFromBaseConfig();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openProjectFromHome(int projectId);
    void onUiEventNotify(int uiEvent, const QVariantList& params);
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void openUserManualFromConfig();
    void openAiPlanningAssistant();
    void showAboutDialog();

private:
    void initNavigationBar();
    void createPanels();
    void wireMainMenuActions();
    int findTabByProjectId(int projectId) const;
    CasePanel* createProjectPanel(int projectId, const QString& projectName);
    QString resolveProjectName(int projectId) const;
    int addTabItem(int projectId, const QString& tabName);
    void showTabContent(int tabIndex);
    void closeProjectTab(int projectId);
    void showProjectOpenLoading();
    void hideProjectOpenLoading();
    void activateProjectTab(int projectId);

private:
    QVBoxLayout* m_mainLayout;
    QFrame* m_navBar;
    QToolButton* m_menuButton;
    QMenu* m_mainMenu;
    QAction* m_actionNewProject;
    QAction* m_actionOpenProject;
    QAction* m_actionManual;
    QAction* m_actionAbout;
    QLabel* m_titleLabel;
    NavTabBar* m_tabBar;
    QStackedWidget* m_panelStack;
    HomePanel* m_homePanel;
    QHash<int, CasePanel*> m_projectPanels;
    QPointer<LoadingDialog> m_projectOpenLoading;
    QTimer* m_projectOpenLoadingDelayTimer;
    int m_openingProjectId;
};

#endif // MAINWINDOW_H
