/*****************************************************************//**
 * \file   homepanel.h
 * \brief  首页面板
 * \details 根据设计图实现欢迎区、功能卡片、项目入口与帮助信息
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef HOMEPANEL_H
#define HOMEPANEL_H

#include "../homectrls/featurecardwidget.h"
#include "../homectrls/myprojectsdialog.h"
#include "../homectrls/newprojectdialog.h"
#include "../homectrls/projectitemmodel.h"
#include "../homectrls/recentprojecthistory.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

class QGridLayout;

class HomePanel : public QWidget {
    Q_OBJECT

public:
    explicit HomePanel(QWidget* parent = nullptr);
    ~HomePanel() override;

    ProjectItemModel* projectModel() const;

    void recordLatestOpenedProject(int projectId);
    void recordLatestOpenedProject(const ProjectItem& item);
    void saveRecentProjects();

    /** 外部在切换回起始页等时机刷新「快速开始」列表（例如此前面板隐藏时未完成重绘） */
    void refreshQuickStartFromHistory();

    /** 主窗口菜单「新建项目」，行为与首页同名按钮一致 */
    void triggerNewProject();
    /** 主窗口菜单「打开项目」，行为与首页同名按钮一致 */
    void triggerOpenProject();

signals:
    void featureCardClicked(FeatureCardType type);
    void projectOpened(int projectId);

private slots:
    void onFeatureCardClicked(FeatureCardType type);
    void onMyProjectsClicked();
    void onNewProjectClicked();
    void onOpenProjectClicked();
    void onTeachingCasesClicked();
    void onProjectActivated(int projectId);
    void onNewProjectRequestedFromDialog();
    void onProjectEditRequested(int projectId);
    void onProjectDeleteRequested(int projectId);
    void onUiEventNotify(int uiEvent, const QVariantList& params);

private:
    void initUi();
    void applyBaseConfigFromSession();
    void refreshProjectListFromGlobalState();
    void refreshQuickStartList();
    void createProject(const QString& name,
                       const QString& description = QString(),
                       const QString& path = QString(),
                       bool refreshQuickStartUi = true);
    void openProject(int projectId);
    int findOrCreateProjectForPath(const QString& dir, const QString& fallbackName);
    void openRecentEntry(const RecentProjectEntry& entry);
    void removeRecentEntryAndPersist(const RecentProjectEntry& entry);
    QWidget* createTitleArea();
    QWidget* createFeatureCardArea();
    QWidget* createQuickStartModule();
    QWidget* createProjectManagementModule();
    QWidget* createHelpModule();
    QWidget* createContactModule();
    QPushButton* createActionButton(const QString& iconPath, const QString& title, const QString& objectName);
    QFrame* createSectionFrame();
    void rebuildHelpDocButtons();

private:
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;

    FeatureCardWidget* m_planningCard;
    FeatureCardWidget* m_dataManageCard;
    FeatureCardWidget* m_evaluationCard;
    FeatureCardWidget* m_simulationCard;
    FeatureCardWidget* m_publishCard;

    ProjectItemModel* m_projectModel;
    NewProjectDialog* m_newProjectDialog;
    MyProjectsDialog* m_myProjectsDialog;

    QPushButton* m_myProjectsButton;
    QFrame* m_quickStartListFrame;
    QScrollArea* m_quickStartListScrollArea;
    QWidget* m_quickStartListScrollContent;
    QVBoxLayout* m_quickStartListLayout;

    QLabel* m_heroMainTitleLabel;
    QLabel* m_heroSubTitleLabel;
    QLabel* m_heroDescLabel;

    QVector<QLabel*> m_contactTextLabels;

    QGridLayout* m_helpDocsGridLayout;

    RecentProjectHistory m_recentHistory;
    bool m_waitingForCreatePrompt;
    QString m_pendingProjectListAction;
    int m_pendingProjectId = 0;
};

#endif // HOMEPANEL_H
