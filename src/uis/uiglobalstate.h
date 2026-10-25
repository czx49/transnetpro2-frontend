/*****************************************************************//**
 * \file   uiglobalstate.h
 * \brief  界面全局状态管理
 * \details 管理登录态、版本信息和页面切换事件
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef UIGLOBALSTATE_H
#define UIGLOBALSTATE_H

#include "httpapijson.h"
#include "singleton.h"

#include <functional>

#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVariantList>

template <class T>
class Singleton;

class UIGlobalState : public QObject {
    Q_OBJECT

public:
    struct BaseMenuGroups {
        QList<MenuJson> roadMenus;
        QList<MenuJson> busMenus;
        QList<MenuJson> otherMenus;
    };

    enum ProjectListState {
        ProjectListIdle,
        ProjectListLoading,
        ProjectListReady,
        ProjectListError
    };

    enum ProjectOpenState {
        ProjectOpenIdle,
        ProjectOpenLoading,
        ProjectOpenReady,
        ProjectOpenPartialReady,
        ProjectOpenError
    };

    enum PageType {
        PageEmpty,
        PageLogin,
        PageMain
    };

    enum UIEvent {
        LoginStateChanged,
        CurrentPageChanged,
        VersionInfoChanged,
        ProjectListStateChanged,
        ProjectOpenStateChanged,
        ProjectListDataChanged,
        ProjectCreated,
        CurrentProjectChanged,
        CurrentProjectCasesChanged
    };

public:
    explicit UIGlobalState(QObject* parent = nullptr);
    ~UIGlobalState() override = default;

    void setLoginInfo(const QSharedPointer<LoginResponseJson>& loginInfo);
    QSharedPointer<LoginResponseJson> loginInfo() const;
    bool isLogin() const;

    void setVersionInfo(const QSharedPointer<VersionResponseJson>& versionInfo);
    QSharedPointer<VersionResponseJson> versionInfo() const;

    void setCurrentPage(PageType currentPage);
    PageType currentPage() const;

    void loadProjectPage(int page, int pageSize);
    void createProject(const QString& projectName,
                       const QString& projectDesc,
                       int provinceId,
                       const QString& province,
                       int cityId,
                       const QString& city);
    void editProject(int id,
                     const QString& projectName,
                     const QString& projectDesc,
                     int provinceId,
                     const QString& province,
                     int cityId,
                     const QString& city);
    void deleteProject(int id);
    void openProject(int projectId);
    void reloadCurrentProjectCases();
    void reloadCurrentProjectDetail();
    void fetchCurrentProjectCasesQuiet(
        const std::function<void(bool success, const QList<CaseJson>& cases, const QString& errorMessage)>& callback);
    void fetchCurrentProjectDetailQuiet(
        const std::function<void(bool success, const QString& errorMessage)>& callback);

    ProjectListState projectListState() const;
    ProjectOpenState projectOpenState() const;
    bool isProjectOpening() const;
    QString projectListError() const;
    QString projectOpenError() const;
    QList<ProjectItemJson> projectItems() const;
    QSharedPointer<ProjectDetailJson> currentProjectDetail() const;
    QList<CaseJson> currentProjectCases() const;
    BaseMenuGroups currentProjectBaseMenuGroups() const;
    int currentProjectId() const;

signals:
    void uiEventNotify(int uiEvent, const QVariantList& params);

private:
    friend class Singleton<UIGlobalState>;

    void setProjectListState(ProjectListState state, const QString& errorMessage = QString());
    void setProjectOpenState(ProjectOpenState state, const QString& errorMessage = QString());
    static BaseMenuGroups classifyProjectMenus(const QList<MenuJson>& menus);

private:
    bool mIsLogin;
    PageType mCurrentPage;
    QSharedPointer<LoginResponseJson> mLoginInfo;
    QSharedPointer<VersionResponseJson> mVersionInfo;
    QList<ProjectItemJson> mProjectItems;
    QSharedPointer<ProjectDetailJson> mCurrentProjectDetail;
    QList<CaseJson> mCurrentProjectCases;
    BaseMenuGroups mCurrentProjectBaseMenus;
    int mCurrentProjectId;
    ProjectListState mProjectListState;
    ProjectOpenState mProjectOpenState;
    QString mProjectListError;
    QString mProjectOpenError;
    int mProjectListRequestSeq;
    int mProjectOpenRequestSeq;
};

#define UIG() Singleton<UIGlobalState>::instance()

#endif // UIGLOBALSTATE_H
