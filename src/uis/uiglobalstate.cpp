/*****************************************************************//**
 * \file   uiglobalstate.cpp
 * \brief  界面全局状态管理实现
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "uiglobalstate.h"
#include "httpapis.h"

#include <memory>

#include <QVariantMap>

namespace {
QVariantList singlePayload(const QVariantMap& payload)
{
    return { payload };
}

QVariantMap projectListPayload(UIGlobalState::ProjectListState state,
                               const QString& error,
                               int currentProjectId,
                               int count,
                               int page,
                               int pageSize)
{
    return {
        { "state", static_cast<int>(state) },
        { "error", error },
        { "projectId", currentProjectId },
        { "count", count },
        { "page", page },
        { "pageSize", pageSize }
    };
}

QVariantMap projectOpenPayload(UIGlobalState::ProjectOpenState state,
                               const QString& error,
                               int projectId,
                               int count)
{
    return {
        { "state", static_cast<int>(state) },
        { "error", error },
        { "projectId", projectId },
        { "count", count }
    };
}

QString authToken(const QSharedPointer<LoginResponseJson>& loginInfo)
{
    return loginInfo.isNull() ? QString() : loginInfo->j_token().trimmed();
}

int createdProjectIdFromResponse(const QSharedPointer<PropertyJson>& response)
{
    if (response.isNull()) {
        return 0;
    }

    const QVariantMap data = response->value<QVariantMap>();
    if (data.contains(QStringLiteral("project_id"))) {
        return data.value(QStringLiteral("project_id")).toInt();
    }

    if (data.contains(QStringLiteral("id"))) {
        return data.value(QStringLiteral("id")).toInt();
    }

    return 0;
}

bool containsAnyKeyword(const QString& text, const QStringList& keywords)
{
    const QString lowerText = text.toLower();
    for (const QString& keyword : keywords) {
        if (lowerText.contains(keyword.toLower())) {
            return true;
        }
    }
    return false;
}

QList<ProjectItemJson> toProjectItems(const QSharedPointer<PagedDataJson>& pagedData)
{
    QList<ProjectItemJson> items;
    if (pagedData.isNull()) {
        return items;
    }

    const QList<QJsonObject> rawItems = pagedData->j_data();
    items.reserve(rawItems.count());
    for (const QJsonObject& raw : rawItems) {
        items.append(ProjectItemJson(raw));
    }
    return items;
}

QList<CaseJson> toCaseItems(const QSharedPointer<PagedDataJson>& pagedData)
{
    QList<CaseJson> items;
    if (pagedData.isNull()) {
        return items;
    }

    const QList<QJsonObject> rawItems = pagedData->j_data();
    items.reserve(rawItems.count());
    for (const QJsonObject& raw : rawItems) {
        items.append(CaseJson(raw));
    }
    return items;
}

QList<CaseJson> mergeCasesWithBaseScheme(const QSharedPointer<ProjectDetailJson>& detail,
                                         int projectId,
                                         const QList<CaseJson>& apiCases)
{
    Q_UNUSED(detail)
    Q_UNUSED(projectId)
    QList<CaseJson> merged;
    for (const CaseJson& item : apiCases) {
        if (item.j_id() <= 0) {
            continue;
        }
        merged.append(item);
    }
    return merged;
}
}

UIGlobalState::UIGlobalState(QObject* parent)
    : QObject(parent)
    , mIsLogin(false)
    , mCurrentPage(PageEmpty)
    , mCurrentProjectId(0)
    , mProjectListState(ProjectListIdle)
    , mProjectOpenState(ProjectOpenIdle)
    , mProjectListRequestSeq(0)
    , mProjectOpenRequestSeq(0)
{
}

void UIGlobalState::setLoginInfo(const QSharedPointer<LoginResponseJson>& loginInfo)
{
    const bool oldLoginState = mIsLogin;
    mLoginInfo = loginInfo;
    mIsLogin = !mLoginInfo.isNull();

    if (oldLoginState != mIsLogin || oldLoginState) {
        emit uiEventNotify(LoginStateChanged, {});
    }
}

QSharedPointer<LoginResponseJson> UIGlobalState::loginInfo() const
{
    return mLoginInfo;
}

bool UIGlobalState::isLogin() const
{
    return mIsLogin;
}

void UIGlobalState::setVersionInfo(const QSharedPointer<VersionResponseJson>& versionInfo)
{
    mVersionInfo = versionInfo;
    emit uiEventNotify(VersionInfoChanged, {});
}

QSharedPointer<VersionResponseJson> UIGlobalState::versionInfo() const
{
    return mVersionInfo;
}

void UIGlobalState::setCurrentPage(PageType currentPage)
{
    if (mCurrentPage == currentPage) {
        return;
    }

    mCurrentPage = currentPage;
    emit uiEventNotify(CurrentPageChanged, { static_cast<int>(mCurrentPage) });
}

UIGlobalState::PageType UIGlobalState::currentPage() const
{
    return mCurrentPage;
}

void UIGlobalState::loadProjectPage(int page, int pageSize)
{
    const int safePage = qMax(1, page);
    const int safePageSize = pageSize > 0 ? pageSize : DEFAULT_PAGE_LIMIT;
    const QString token = authToken(loginInfo());
    const int requestSeq = ++mProjectListRequestSeq;

    if (token.isEmpty()) {
        mProjectItems.clear();
        setProjectListState(ProjectListError, u8"登录信息缺失或 token 为空");
        emit uiEventNotify(ProjectListDataChanged, singlePayload(projectListPayload(
                                                     mProjectListState,
                                                     mProjectListError,
                                                     mCurrentProjectId,
                                                     mProjectItems.count(),
                                                     safePage,
                                                     safePageSize)));
        return;
    }

    setProjectListState(ProjectListLoading);
    HttpApis::getProjectList(token, safePage, safePageSize, QString(), [this, safePage, safePageSize, requestSeq](bool suc,
                                                                                                                    const QSharedPointer<PagedDataJson>& response,
                                                                                                                    const QString& errMsg) {
        if (requestSeq != mProjectListRequestSeq) {
            return;
        }

        if (!suc || response.isNull()) {
            mProjectItems.clear();
            setProjectListState(ProjectListError, errMsg.isEmpty() ? QString(u8"获取项目列表失败") : errMsg);
        } else {
            mProjectItems = toProjectItems(response);
            setProjectListState(ProjectListReady);
        }

        emit uiEventNotify(ProjectListDataChanged, singlePayload(projectListPayload(
                                                     mProjectListState,
                                                     mProjectListError,
                                                     mCurrentProjectId,
                                                     mProjectItems.count(),
                                                     safePage,
                                                     safePageSize)));
    });
}

void UIGlobalState::createProject(const QString& projectName,
                                  const QString& projectDesc,
                                  int provinceId,
                                  const QString& province,
                                  int cityId,
                                  const QString& city)
{
    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        setProjectListState(ProjectListError, u8"登录信息缺失或 token 为空");
        emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                         { "state", static_cast<int>(mProjectListState) },
                                                         { "error", mProjectListError },
                                                         { "projectId", mCurrentProjectId },
                                                         { "count", mProjectItems.count() },
                                                         { "page", 1 },
                                                         { "pageSize", DEFAULT_PAGE_LIMIT },
                                                         { "projectName", projectName },
                                                         { "projectDesc", projectDesc }
                                                     }));
        return;
    }

    setProjectListState(ProjectListLoading);
    HttpApis::createProject(token,
                            projectName,
                            projectDesc,
                            provinceId,
                            province,
                            cityId,
                            city,
                            [this, projectName, projectDesc](bool suc,
                                                             const QSharedPointer<PropertyJson>& response,
                                                             const QString& errMsg) {
        if (!suc) {
            setProjectListState(ProjectListError, errMsg.isEmpty() ? QString(u8"创建项目失败") : errMsg);
            emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                             { "state", static_cast<int>(mProjectListState) },
                                                             { "error", mProjectListError },
                                                             { "projectId", mCurrentProjectId },
                                                             { "count", mProjectItems.count() },
                                                             { "page", 1 },
                                                             { "pageSize", DEFAULT_PAGE_LIMIT },
                                                             { "projectName", projectName },
                                                             { "projectDesc", projectDesc }
                                                         }));
            return;
        }

        const int createdProjectId = createdProjectIdFromResponse(response);
        const QString createReason =
            createdProjectId > 0 ? QString() : QString(u8"创建成功但接口未返回项目ID");

        emit uiEventNotify(ProjectCreated, singlePayload({
                                           { "state", static_cast<int>(mProjectListState) },
                                           { "error", createReason },
                                           { "reason", createReason },
                                           { "projectId", createdProjectId },
                                           { "count", mProjectItems.count() },
                                           { "projectName", projectName },
                                           { "projectDesc", projectDesc }
                                       }));
        loadProjectPage(1, DEFAULT_PAGE_LIMIT);
    });
}

void UIGlobalState::editProject(int id,
                                const QString& projectName,
                                const QString& projectDesc,
                                int provinceId,
                                const QString& province,
                                int cityId,
                                const QString& city)
{
    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        setProjectListState(ProjectListError, u8"登录信息缺失或 token 为空");
        emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                         { "state", static_cast<int>(mProjectListState) },
                                                         { "error", mProjectListError },
                                                         { "action", QStringLiteral("edit") },
                                                         { "projectId", id },
                                                         { "count", mProjectItems.count() },
                                                         { "page", 1 },
                                                         { "pageSize", DEFAULT_PAGE_LIMIT }
                                                     }));
        return;
    }

    if (id <= 0) {
        setProjectListState(ProjectListError, u8"无效的项目ID");
        emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                         { "state", static_cast<int>(mProjectListState) },
                                                         { "error", mProjectListError },
                                                         { "action", QStringLiteral("edit") },
                                                         { "projectId", id },
                                                         { "count", mProjectItems.count() },
                                                         { "page", 1 },
                                                         { "pageSize", DEFAULT_PAGE_LIMIT }
                                                     }));
        return;
    }

    setProjectListState(ProjectListLoading);
    HttpApis::editProject(token,
                          id,
                          projectName,
                          projectDesc,
                          provinceId,
                          province,
                          cityId,
                          city,
                          [this, id](bool suc, const QSharedPointer<ResponseJson>& /*response*/, const QString& errMsg) {
        if (!suc) {
            setProjectListState(ProjectListError, errMsg.isEmpty() ? QString(u8"编辑项目失败") : errMsg);
            emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                             { "state", static_cast<int>(mProjectListState) },
                                                             { "error", mProjectListError },
                                                             { "action", QStringLiteral("edit") },
                                                             { "projectId", id },
                                                             { "count", mProjectItems.count() },
                                                             { "page", 1 },
                                                             { "pageSize", DEFAULT_PAGE_LIMIT }
                                                         }));
            return;
        }

        loadProjectPage(1, DEFAULT_PAGE_LIMIT);
        emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                         { "state", static_cast<int>(ProjectListReady) },
                                                         { "error", QString() },
                                                         { "action", QStringLiteral("edit") },
                                                         { "projectId", id },
                                                         { "count", mProjectItems.count() },
                                                         { "page", 1 },
                                                         { "pageSize", DEFAULT_PAGE_LIMIT }
                                                     }));
    });
}

void UIGlobalState::deleteProject(int id)
{
    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        setProjectListState(ProjectListError, u8"登录信息缺失或 token 为空");
        emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                         { "state", static_cast<int>(mProjectListState) },
                                                         { "error", mProjectListError },
                                                         { "action", QStringLiteral("delete") },
                                                         { "projectId", id },
                                                         { "count", mProjectItems.count() },
                                                         { "page", 1 },
                                                         { "pageSize", DEFAULT_PAGE_LIMIT }
                                                     }));
        return;
    }

    if (id <= 0) {
        setProjectListState(ProjectListError, u8"无效的项目ID");
        emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                         { "state", static_cast<int>(mProjectListState) },
                                                         { "error", mProjectListError },
                                                         { "action", QStringLiteral("delete") },
                                                         { "projectId", id },
                                                         { "count", mProjectItems.count() },
                                                         { "page", 1 },
                                                         { "pageSize", DEFAULT_PAGE_LIMIT }
                                                     }));
        return;
    }

    setProjectListState(ProjectListLoading);
    HttpApis::deleteProject(token, id, [this, id](bool suc, const QSharedPointer<ResponseJson>& /*response*/, const QString& errMsg) {
        if (!suc) {
            setProjectListState(ProjectListError, errMsg.isEmpty() ? QString(u8"删除项目失败") : errMsg);
            emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                             { "state", static_cast<int>(mProjectListState) },
                                                             { "error", mProjectListError },
                                                             { "action", QStringLiteral("delete") },
                                                             { "projectId", id },
                                                             { "count", mProjectItems.count() },
                                                             { "page", 1 },
                                                             { "pageSize", DEFAULT_PAGE_LIMIT }
                                                         }));
            return;
        }

        if (mCurrentProjectId == id) {
            mCurrentProjectId = 0;
            mCurrentProjectDetail.clear();
            mCurrentProjectCases.clear();
            mCurrentProjectBaseMenus = BaseMenuGroups {};
            setProjectOpenState(ProjectOpenIdle);
        }

        loadProjectPage(1, DEFAULT_PAGE_LIMIT);
        emit uiEventNotify(ProjectListDataChanged, singlePayload({
                                                         { "state", static_cast<int>(ProjectListReady) },
                                                         { "error", QString() },
                                                         { "action", QStringLiteral("delete") },
                                                         { "projectId", id },
                                                         { "count", mProjectItems.count() },
                                                         { "page", 1 },
                                                         { "pageSize", DEFAULT_PAGE_LIMIT }
                                                     }));
    });
}

void UIGlobalState::openProject(int projectId)
{
    if (mProjectOpenState == ProjectOpenLoading) {
        return;
    }

    if (projectId <= 0) {
        ++mProjectOpenRequestSeq;
        setProjectOpenState(ProjectOpenError, u8"无效的项目ID");
        mCurrentProjectBaseMenus = BaseMenuGroups {};
        emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                   mProjectOpenState,
                                                   mProjectOpenError,
                                                   mCurrentProjectId,
                                                   mCurrentProjectCases.count())));
        emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                        mProjectOpenState,
                                                        mProjectOpenError,
                                                        mCurrentProjectId,
                                                        mCurrentProjectCases.count())));
        return;
    }

    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        ++mProjectOpenRequestSeq;
        setProjectOpenState(ProjectOpenError, u8"登录信息缺失或 token 为空");
        mCurrentProjectBaseMenus = BaseMenuGroups {};
        emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                   mProjectOpenState,
                                                   mProjectOpenError,
                                                   projectId,
                                                   mCurrentProjectCases.count())));
        emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                        mProjectOpenState,
                                                        mProjectOpenError,
                                                        projectId,
                                                        mCurrentProjectCases.count())));
        return;
    }

    const int requestSeq = ++mProjectOpenRequestSeq;
    mCurrentProjectId = projectId;
    mCurrentProjectDetail.clear();
    mCurrentProjectCases.clear();
    mCurrentProjectBaseMenus = BaseMenuGroups {};
    setProjectOpenState(ProjectOpenLoading);
    emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                               mProjectOpenState,
                                               mProjectOpenError,
                                               mCurrentProjectId,
                                               mCurrentProjectCases.count())));
    emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                    mProjectOpenState,
                                                    mProjectOpenError,
                                                    mCurrentProjectId,
                                                    mCurrentProjectCases.count())));

    struct OpenProjectContext {
        bool detailDone = false;
        bool casesDone = false;
        bool detailOk = false;
        bool casesOk = false;
        QSharedPointer<ProjectDetailJson> detail;
        QSharedPointer<PagedDataJson> caseResponse;
        QString detailErr;
        QString casesErr;
    };

    const auto ctx = std::make_shared<OpenProjectContext>();
    const auto tryFinishOpen = [this, projectId, requestSeq, ctx]() {
        if (!ctx->detailDone || !ctx->casesDone) {
            return;
        }
        if (requestSeq != mProjectOpenRequestSeq || projectId != mCurrentProjectId) {
            return;
        }

        if (!ctx->detailOk || ctx->detail.isNull()) {
            mCurrentProjectDetail.clear();
            mCurrentProjectCases.clear();
            mCurrentProjectBaseMenus = BaseMenuGroups {};
            setProjectOpenState(ProjectOpenError,
                                ctx->detailErr.isEmpty() ? QString(u8"获取项目详情失败") : ctx->detailErr);
            emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                           mProjectOpenState,
                                                           mProjectOpenError,
                                                           projectId,
                                                           mCurrentProjectCases.count())));
            emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                                mProjectOpenState,
                                                                mProjectOpenError,
                                                                projectId,
                                                                mCurrentProjectCases.count())));
            return;
        }

        mCurrentProjectDetail = ctx->detail;
        mCurrentProjectBaseMenus = classifyProjectMenus(ctx->detail->j_menu());

        if (!ctx->casesOk || ctx->caseResponse.isNull()) {
            mCurrentProjectCases = mergeCasesWithBaseScheme(mCurrentProjectDetail, projectId, {});
            setProjectOpenState(ProjectOpenPartialReady,
                                ctx->casesErr.isEmpty() ? QString(u8"项目详情已就绪，但方案列表加载失败")
                                                        : ctx->casesErr);
        } else {
            mCurrentProjectCases = mergeCasesWithBaseScheme(mCurrentProjectDetail,
                                                            projectId,
                                                            toCaseItems(ctx->caseResponse));
            setProjectOpenState(ProjectOpenReady);
        }

        emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                       mProjectOpenState,
                                                       mProjectOpenError,
                                                       projectId,
                                                       mCurrentProjectCases.count())));
        emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                            mProjectOpenState,
                                                            mProjectOpenError,
                                                            projectId,
                                                            mCurrentProjectCases.count())));
    };

    HttpApis::projectDetail(token, projectId, [this, projectId, requestSeq, ctx, tryFinishOpen](
                                bool suc,
                                const QSharedPointer<ProjectDetailJson>& detail,
                                const QString& errMsg) {
        if (requestSeq != mProjectOpenRequestSeq || projectId != mCurrentProjectId) {
            return;
        }

        ctx->detailDone = true;
        ctx->detailOk = suc && !detail.isNull();
        ctx->detail = detail;
        ctx->detailErr = errMsg;
        tryFinishOpen();
    });

    HttpApis::getCaseList(token, projectId, 1, DEFAULT_PAGE_LIMIT, [this, projectId, requestSeq, ctx, tryFinishOpen](
                              bool caseSuc,
                              const QSharedPointer<PagedDataJson>& caseResponse,
                              const QString& caseErrMsg) {
        if (requestSeq != mProjectOpenRequestSeq || projectId != mCurrentProjectId) {
            return;
        }

        ctx->casesDone = true;
        ctx->casesOk = caseSuc && !caseResponse.isNull();
        ctx->caseResponse = caseResponse;
        ctx->casesErr = caseErrMsg;
        tryFinishOpen();
    });
}

void UIGlobalState::reloadCurrentProjectCases()
{
    if (mCurrentProjectId <= 0) {
        ++mProjectOpenRequestSeq;
        setProjectOpenState(ProjectOpenError, u8"当前未选择项目");
        emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                   mProjectOpenState,
                                                   mProjectOpenError,
                                                   mCurrentProjectId,
                                                   mCurrentProjectCases.count())));
        emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                        mProjectOpenState,
                                                        mProjectOpenError,
                                                        mCurrentProjectId,
                                                        mCurrentProjectCases.count())));
        return;
    }

    const int projectId = mCurrentProjectId;
    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        ++mProjectOpenRequestSeq;
        setProjectOpenState(ProjectOpenError, u8"登录信息缺失或 token 为空");
        emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                   mProjectOpenState,
                                                   mProjectOpenError,
                                                   projectId,
                                                   mCurrentProjectCases.count())));
        emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                        mProjectOpenState,
                                                        mProjectOpenError,
                                                        projectId,
                                                        mCurrentProjectCases.count())));
        return;
    }

    const int requestSeq = ++mProjectOpenRequestSeq;
    setProjectOpenState(ProjectOpenLoading);
    emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                               mProjectOpenState,
                                               mProjectOpenError,
                                               projectId,
                                               mCurrentProjectCases.count())));
    HttpApis::getCaseList(token, projectId, 1, DEFAULT_PAGE_LIMIT, [this, projectId, requestSeq](bool suc,
                                                                                                   const QSharedPointer<PagedDataJson>& response,
                                                                                                   const QString& errMsg) {
        if (requestSeq != mProjectOpenRequestSeq || projectId != mCurrentProjectId) {
            return;
        }

        if (!suc || response.isNull()) {
            mCurrentProjectCases = mergeCasesWithBaseScheme(mCurrentProjectDetail, projectId, {});
            if (mCurrentProjectDetail.isNull()) {
                setProjectOpenState(ProjectOpenError, errMsg.isEmpty() ? QString(u8"刷新方案列表失败") : errMsg);
            } else {
                setProjectOpenState(ProjectOpenPartialReady, errMsg.isEmpty() ? QString(u8"方案列表刷新失败") : errMsg);
            }
        } else {
            mCurrentProjectCases = mergeCasesWithBaseScheme(mCurrentProjectDetail, projectId, toCaseItems(response));
            setProjectOpenState(ProjectOpenReady);
        }

        emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                   mProjectOpenState,
                                                   mProjectOpenError,
                                                   projectId,
                                                   mCurrentProjectCases.count())));
        emit uiEventNotify(CurrentProjectCasesChanged, singlePayload(projectOpenPayload(
                                                        mProjectOpenState,
                                                        mProjectOpenError,
                                                        projectId,
                                                        mCurrentProjectCases.count())));
    });
}

void UIGlobalState::fetchCurrentProjectCasesQuiet(
    const std::function<void(bool, const QList<CaseJson>&, const QString&)>& callback)
{
    if (!callback) {
        return;
    }

    if (mCurrentProjectId <= 0) {
        callback(false, {}, QString::fromUtf8(u8"当前未选择项目"));
        return;
    }

    const int projectId = mCurrentProjectId;
    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        callback(false, {}, QString::fromUtf8(u8"登录信息缺失或 token 为空"));
        return;
    }

    HttpApis::getCaseList(token, projectId, 1, DEFAULT_PAGE_LIMIT,
        [this, projectId, callback](bool suc, const QSharedPointer<PagedDataJson>& response, const QString& errMsg) {
            if (projectId != mCurrentProjectId) {
                return;
            }
            if (!suc || response.isNull()) {
                callback(false, {}, errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"刷新方案列表失败") : errMsg.trimmed());
                return;
            }

            mCurrentProjectCases = mergeCasesWithBaseScheme(mCurrentProjectDetail, projectId, toCaseItems(response));
            callback(true, mCurrentProjectCases, QString());
        });
}

void UIGlobalState::fetchCurrentProjectDetailQuiet(
    const std::function<void(bool, const QString&)>& callback)
{
    if (!callback) {
        return;
    }

    if (mCurrentProjectId <= 0) {
        callback(false, QString::fromUtf8(u8"当前未选择项目"));
        return;
    }

    const int projectId = mCurrentProjectId;
    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        callback(false, QString::fromUtf8(u8"登录信息缺失或 token 为空"));
        return;
    }

    const int requestSeq = ++mProjectOpenRequestSeq;
    HttpApis::projectDetail(token, projectId, [this, projectId, requestSeq, callback](
            bool suc, const QSharedPointer<ProjectDetailJson>& detail, const QString& errMsg) {
        if (requestSeq != mProjectOpenRequestSeq || projectId != mCurrentProjectId) {
            return;
        }
        if (!suc || detail.isNull()) {
            const QString message =
                errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"刷新项目详情失败") : errMsg.trimmed();
            callback(false, message);
            return;
        }

        mCurrentProjectDetail = detail;
        mCurrentProjectBaseMenus = classifyProjectMenus(detail->j_menu());
        callback(true, QString());
    });
}

void UIGlobalState::reloadCurrentProjectDetail()
{
    if (mCurrentProjectId <= 0) {
        return;
    }

    const int projectId = mCurrentProjectId;
    const QString token = authToken(loginInfo());
    if (token.isEmpty()) {
        return;
    }

    const int requestSeq = ++mProjectOpenRequestSeq;
    HttpApis::projectDetail(token, projectId, [this, projectId, requestSeq](bool suc,
                                                                            const QSharedPointer<ProjectDetailJson>& detail,
                                                                            const QString& errMsg) {
        Q_UNUSED(errMsg)
        if (requestSeq != mProjectOpenRequestSeq || projectId != mCurrentProjectId) {
            return;
        }
        if (!suc || detail.isNull()) {
            return;
        }

        mCurrentProjectDetail = detail;
        mCurrentProjectBaseMenus = classifyProjectMenus(detail->j_menu());
        emit uiEventNotify(CurrentProjectChanged, singlePayload(projectOpenPayload(
                                                   mProjectOpenState,
                                                   mProjectOpenError,
                                                   projectId,
                                                   mCurrentProjectCases.count())));
    });
}

UIGlobalState::ProjectListState UIGlobalState::projectListState() const
{
    return mProjectListState;
}

UIGlobalState::ProjectOpenState UIGlobalState::projectOpenState() const
{
    return mProjectOpenState;
}

bool UIGlobalState::isProjectOpening() const
{
    return mProjectOpenState == ProjectOpenLoading;
}

QString UIGlobalState::projectListError() const
{
    return mProjectListError;
}

QString UIGlobalState::projectOpenError() const
{
    return mProjectOpenError;
}

QList<ProjectItemJson> UIGlobalState::projectItems() const
{
    return mProjectItems;
}

QSharedPointer<ProjectDetailJson> UIGlobalState::currentProjectDetail() const
{
    return mCurrentProjectDetail;
}

QList<CaseJson> UIGlobalState::currentProjectCases() const
{
    return mCurrentProjectCases;
}

UIGlobalState::BaseMenuGroups UIGlobalState::currentProjectBaseMenuGroups() const
{
    return mCurrentProjectBaseMenus;
}

int UIGlobalState::currentProjectId() const
{
    return mCurrentProjectId;
}

void UIGlobalState::setProjectListState(ProjectListState state, const QString& errorMessage)
{
    const bool stateChanged = (mProjectListState != state);
    const bool errorChanged = (mProjectListError != errorMessage);

    mProjectListState = state;
    mProjectListError = errorMessage;

    if (stateChanged || errorChanged) {
        emit uiEventNotify(ProjectListStateChanged, singlePayload({
                                                      { "state", static_cast<int>(mProjectListState) },
                                                      { "error", mProjectListError },
                                                      { "projectId", mCurrentProjectId },
                                                      { "count", mProjectItems.count() }
                                                  }));
    }
}

void UIGlobalState::setProjectOpenState(ProjectOpenState state, const QString& errorMessage)
{
    const bool stateChanged = (mProjectOpenState != state);
    const bool errorChanged = (mProjectOpenError != errorMessage);

    mProjectOpenState = state;
    mProjectOpenError = errorMessage;

    if (stateChanged || errorChanged) {
        emit uiEventNotify(ProjectOpenStateChanged, singlePayload({
                                                      { "state", static_cast<int>(mProjectOpenState) },
                                                      { "error", mProjectOpenError },
                                                      { "projectId", mCurrentProjectId },
                                                      { "count", mCurrentProjectCases.count() }
                                                  }));
    }
}

UIGlobalState::BaseMenuGroups UIGlobalState::classifyProjectMenus(const QList<MenuJson>& menus)
{
    BaseMenuGroups groups;
    const QStringList roadKeywords = {QStringLiteral("road"), QString::fromUtf8(u8"道路")};
    const QStringList busKeywords = {QStringLiteral("bus"), QString::fromUtf8(u8"公交")};
    const QStringList slowKeywords = {QStringLiteral("slow"), QString::fromUtf8(u8"慢行")};

    for (const MenuJson& menu : menus) {
        const QString key = menu.j_value();
        const QString name = menu.j_name();
        if (containsAnyKeyword(key, roadKeywords) || containsAnyKeyword(name, roadKeywords)) {
            groups.roadMenus.append(menu);
            continue;
        }
        if (containsAnyKeyword(key, busKeywords) || containsAnyKeyword(name, busKeywords)) {
            groups.busMenus.append(menu);
            continue;
        }
        if (containsAnyKeyword(key, slowKeywords) || containsAnyKeyword(name, slowKeywords)) {
            groups.otherMenus.append(menu);
            continue;
        }
        groups.otherMenus.append(menu);
    }

    return groups;
}
