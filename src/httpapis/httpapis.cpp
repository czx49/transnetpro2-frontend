#include "httpapis.h"

#include "httpapidefines.h"
#include "grpcaistreamclient.h"
#include "grpcstreamclient.h"
#include "qrequest2.h"

#include <QMetaType>
#include <QUrl>
#include <QUrlQuery>

class HttpApisPrivate {
public:
};

namespace {
template <class T>
void sendGet(const QUrl& url,
             const HttpApis::ResponseCallBack<T>& callback,
             const QString& token = QString(),
             int transferTimeoutMs = QRequest2::kDefaultTransferTimeoutMs)
{
    auto resultful_callback = [callback](bool suc, const QString& msg, const QByteArray& data) {
        HttpApis::handleResponse<T>(suc, msg, data, callback);
    };

    QVariantMap params;
    QRequest2::instance().get(url.toString(),
                              resultful_callback,
                              params,
                              HttpApis::defaultHeaders(),
                              HttpApis::defaultRawHeaders(token),
                              nullptr,
                              transferTimeoutMs);
}

template <class T>
void sendPost(const QString& url,
              const QVariantMap& params,
              const HttpApis::ResponseCallBack<T>& callback,
              const QString& token = QString(),
              int transferTimeoutMs = QRequest2::kDefaultTransferTimeoutMs)
{
    auto resultful_callback = [callback](bool suc, const QString& msg, const QByteArray& data) {
        HttpApis::handleResponse<T>(suc, msg, data, callback);
    };

    QRequest2::instance().post(url,
                               resultful_callback,
                               params,
                               HttpApis::defaultHeaders(),
                               HttpApis::defaultRawHeaders(token),
                               nullptr,
                               transferTimeoutMs);
}

template <class T>
void sendGetLongRunning(const QUrl& url,
                        const HttpApis::ResponseCallBack<T>& callback,
                        const QString& token = QString())
{
    sendGet<T>(url, callback, token, QRequest2::kLongRunningTransferTimeoutMs);
}

template <class T>
void sendPostLongRunning(const QString& url,
                         const QVariantMap& params,
                         const HttpApis::ResponseCallBack<T>& callback,
                         const QString& token = QString())
{
    sendPost<T>(url, params, callback, token, QRequest2::kLongRunningTransferTimeoutMs);
}

void sendGetRaw(const QUrl& url, const HttpApis::RawResponseCallback& callback, const QString& token = QString())
{
    auto wrapped = [callback](bool suc, const QString& msg, const QByteArray& data) {
        callback(suc, data, msg);
    };

    QVariantMap empty;
    QRequest2::instance().get(url.toString(), wrapped, empty, HttpApis::defaultHeaders(), HttpApis::defaultRawHeaders(token));
}
} // namespace

HttpApis::HttpApis()
    : d_p(new HttpApisPrivate)
{
}

HttpApis::~HttpApis()
{
    delete d_p;
    d_p = nullptr;
}

HttpApis& HttpApis::instance()
{
    static HttpApis ins;
    return ins;
}

void HttpApis::getVersion(const ResponseCallBack<VersionResponseJson>& callback)
{
    sendGet<VersionResponseJson>(QUrl(fullURL(API_VERSION_GET)), callback);
}

void HttpApis::login(const QString& username, const QString& password, const ResponseCallBack<LoginResponseJson>& callback)
{
    const QVariantMap params = {
        { "username", username },
        { "password", password }
    };

    auto resultful_callback = [callback](bool suc, const QString& msg, const QByteArray& data) {
        handleResponse<LoginResponseJson>(suc, msg, data, callback);
    };

    QRequest2::instance().post(fullURL(API_LOGIN), resultful_callback, params, defaultHeaders());
}

void HttpApis::getBaseConfig(const ResponseCallBack<BaseConfigJson>& callback)
{
    sendGet<BaseConfigJson>(QUrl(fullURL(API_CONFIG_BASE)), callback);
}

void HttpApis::getAiConfig(const ResponseCallBack<PropertyJson>& callback)
{
    sendGet<PropertyJson>(QUrl(fullURL(API_CONFIG_AICONFIG)), callback);
}

void HttpApis::getCommonQuestions(int isbase,
                                  const QString& type,
                                  const ResponseCallBack<QuestionListJson>& callback)
{
    QUrl url(fullURL(API_CONFIG_QUESTION));
    QUrlQuery query;
    query.addQueryItem("isbase", QString::number(isbase ? 1 : 0));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGet<QuestionListJson>(url, callback);
}

void HttpApis::getRegions(int province, const ResponseCallBack<RegionListJson>& callback)
{
    QUrl url(fullURL(API_CONFIG_REGIONS));
    QUrlQuery query;
    query.addQueryItem("province", QString::number(province));
    url.setQuery(query);

    sendGet<RegionListJson>(url, callback);
}

void HttpApis::getFieldChinese(const ResponseCallBack<PropertyJson>& callback)
{
    sendGet<PropertyJson>(QUrl(fullURL(API_CONFIG_FIELD_CHINESE)), callback);
}

void HttpApis::getProjectList(const QString& token,
                              int pageindex,
                              int limit,
                              const QString& name,
                              const ResponseCallBack<PagedDataJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_PAGE));
    QUrlQuery query;
    query.addQueryItem("page", QString::number(qMax(1, pageindex)));
    query.addQueryItem("limit", QString::number(limit > 0 ? limit : DEFAULT_PAGE_LIMIT));
    if (!name.isEmpty()) {
        query.addQueryItem("name", name);
    }
    url.setQuery(query);

    sendGet<PagedDataJson>(url, callback, token);
}

void HttpApis::createProject(const QString& token,
                             const QString& name,
                             const QString& desc,
                             int provinceId,
                             const QString& province,
                             int cityId,
                             const QString& city,
                             const ResponseCallBack<PropertyJson>& callback)
{
    const QVariantMap params = {
        { "name", name },
        { "desc", desc },
        { "province_id", provinceId },
        { "province", province },
        { "city_id", cityId },
        { "city", city }
    };

    sendPost<PropertyJson>(fullURL(API_PROJECT_ADD), params, callback, token);
}

void HttpApis::editProject(const QString& token,
                           int id,
                           const QString& name,
                           const QString& desc,
                           int provinceId,
                           const QString& province,
                           int cityId,
                           const QString& city,
                           const ResponseCallBack<ResponseJson>& callback)
{
    const QVariantMap params = {
        { "id", id },
        { "name", name },
        { "desc", desc },
        { "province_id", provinceId },
        { "province", province },
        { "city_id", cityId },
        { "city", city }
    };

    sendPost<ResponseJson>(fullURL(API_PROJECT_EDIT), params, callback, token);
}

void HttpApis::projectDetail(const QString& token, int id, const ResponseCallBack<ProjectDetailJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_DETAIL));
    QUrlQuery query;
    query.addQueryItem("id", QString::number(id));
    url.setQuery(query);

    sendGet<ProjectDetailJson>(url, callback, token);
}

void HttpApis::deleteProject(const QString& token, int id, const ResponseCallBack<ResponseJson>& callback)
{
    const QVariantMap params = {
        { "id", id }
    };

    sendPost<ResponseJson>(fullURL(API_PROJECT_DEL), params, callback, token);
}

void HttpApis::clearProjectTable(const QString& token,
                                 int project_id,
                                 const QString& table,
                                 const ResponseCallBack<ResponseJson>& callback)
{
    const QVariantMap params = {
        { "project_id", project_id },
        { "table", table }
    };

    sendPost<ResponseJson>(fullURL(API_PROJECT_CLRTABLE), params, callback, token);
}

void HttpApis::exportProjectTable(const QString& token,
                                  const QString& type,
                                  int project_id,
                                  int case_id,
                                  const QString& table,
                                  const ResponseCallBack<ProjectExportResult>& callback)
{
    QUrl url(fullURL(API_PROJECT_EXPORT));
    QUrlQuery query;
    query.addQueryItem("type", type);
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("table", table);
    url.setQuery(query);

    sendGetLongRunning<ProjectExportResult>(url, callback, token);
}

void HttpApis::exportCase(const QString& token,
                          int project_id,
                          int case_id,
                          const ResponseCallBack<DownLoadCaseJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_EXPORTCASE));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    url.setQuery(query);

    sendGetLongRunning<DownLoadCaseJson>(url, callback, token);
}

void HttpApis::projectUpdateCentroid(const QString& token,
                                     int projectId,
                                     int caseId,
                                     const QString& tableFullName,
                                     const ResponseCallBack<UpdateCentroidJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_UPDATE_CENTROID));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(projectId));
    query.addQueryItem("case_id", QString::number(caseId));
    query.addQueryItem("tablefullname", tableFullName);
    url.setQuery(query);

    sendGet<UpdateCentroidJson>(url, callback, token);
}

void HttpApis::clipLayer(const QString& token,
                         int project_id,
                         int case_id,
                         const QString& community_table,
                         const QString& way_table,
                         const QString& clip_name,
                         const ResponseCallBack<ResponseJson>& callback)
{
    QVariantMap params;
    params["project_id"] = project_id;
    params["case_id"] = case_id;
    params["community_table"] = community_table;
    params["way_table"] = way_table;
    params["clip_name"] = clip_name;

    sendPostLongRunning<ResponseJson>(fullURL(API_PROJECT_CLIP), params, callback, token);
}

void HttpApis::clipPart(const QString& token,
                        int project_id,
                        int case_id,
                        const QString& way_table,
                        const QString& clip_name,
                        const QString& wkt,
                        const ResponseCallBack<ResponseJson>& callback)
{
    QVariantMap params;
    params["project_id"] = project_id;
    params["case_id"] = case_id;
    params["way_table"] = way_table;
    params["clip_name"] = clip_name;
    params["wkt"] = wkt;

    sendPostLongRunning<ResponseJson>(fullURL(API_PROJECT_CLIP_PART), params, callback, token);
}

void HttpApis::deleteClipLayer(const QString& token,
                               int project_id,
                               int case_id,
                               const QString& tablefullname,
                               const ResponseCallBack<ResponseJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_CLIP_DEL));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("tablefullname", tablefullname);
    url.setQuery(query);

    sendGet<ResponseJson>(url, callback, token);
}

void HttpApis::projectBaseAnalysis(const QString& token,
                                   int project_id,
                                   const ResponseCallBack<PropertyJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_BASE_ANALYSIS));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    url.setQuery(query);

    sendGet<PropertyJson>(url, callback, token);
}

void HttpApis::projectPopData(const QString& token,
                              int project_id,
                              const QVariantList& dataList,
                              const ResponseCallBack<ProjectPopDataResultJson>& callback)
{
    QVariantMap params;
    params[QStringLiteral("project_id")] = project_id;
    params[QStringLiteral("data")] = dataList;

    sendPostLongRunning<ProjectPopDataResultJson>(fullURL(API_PROJECT_POPDATA), params, callback, token);
}

void HttpApis::projectGenCommunity(const QString& token,
                                   int project_id,
                                   const QVariantList& dataList,
                                   const ResponseCallBack<ProjectGenCommunityResultJson>& callback)
{
    QVariantMap params;
    params[QStringLiteral("project_id")] = project_id;
    params[QStringLiteral("data")] = dataList;

    sendPostLongRunning<ProjectGenCommunityResultJson>(fullURL(API_PROJECT_GEN_COMMUNITY), params, callback, token);
}

void HttpApis::checkProjectObs(const QString& token,
                               int project_id,
                               const ResponseCallBack<ProjectCheckObsResultJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_CHECK_OBS));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    url.setQuery(query);

    sendGet<ProjectCheckObsResultJson>(url, callback, token);
}

void HttpApis::selectDataAll(const QString& token,
                             const QString& tablefullname,
                             const ResponseCallBack<SelectDataJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_SELECT_DATA_ALL));
    QUrlQuery query;
    query.addQueryItem("tablefullname", tablefullname);
    url.setQuery(query);

    sendGet<SelectDataJson>(url, callback, token);
}

void HttpApis::selectDataPage(const QString& token,
                              const QString& tablefullname,
                              int page,
                              int limit,
                              const ResponseCallBack<SelectDataPageJson>& callback)
{
    QUrl url(fullURL(API_PROJECT_SELECT_DATA_PAGE));
    QUrlQuery query;
    query.addQueryItem("tablefullname", tablefullname);
    query.addQueryItem("page", QString::number(page > 0 ? page : 1));
    query.addQueryItem("limit", QString::number(limit > 0 ? limit : DEFAULT_PAGE_LIMIT));
    url.setQuery(query);

    sendGet<SelectDataPageJson>(url, callback, token);
}

void HttpApis::uploadCSV(const QString& token,
                         const QString& type,
                         int project_id,
                         int case_id,
                         const QString& table,
                         const QString& filePath,
                         const ResponseCallBack<ResponseJson>& callback)
{
    QUrl url(fullURL(API_FILES_DRCSV));
    QUrlQuery query;
    query.addQueryItem("type", type);
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("table", table);
    url.setQuery(query);

    auto resultful_callback = [callback](bool suc, const QString& msg, const QByteArray& data) {
        handleResponse<ResponseJson>(suc, msg, data, callback);
    };

    QVariantMap params;
    QRequest2::instance().postFile(
        filePath,
        url.toString(),
        resultful_callback,
        params,
        QString(),
        QRequest2::KnownHeaderMap(),
        defaultRawHeaders(token),
        nullptr,
        QRequest2::kLongRunningTransferTimeoutMs);
}

void HttpApis::uploadShpZip(const QString& token,
                            const QString& type,
                            int project_id,
                            int case_id,
                            const QString& table,
                            const QString& filePath,
                            const ResponseCallBack<ResponseJson>& callback)
{
    QUrl url(fullURL(API_FILES_DRSHPZIP));
    QUrlQuery query;
    query.addQueryItem("type", type);
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("table", table);
    url.setQuery(query);

    auto resultful_callback = [callback](bool suc, const QString& msg, const QByteArray& data) {
        handleResponse<ResponseJson>(suc, msg, data, callback);
    };

    QVariantMap params;
    QRequest2::instance().postFile(
        filePath,
        url.toString(),
        resultful_callback,
        params,
        QString(),
        QRequest2::KnownHeaderMap(),
        defaultRawHeaders(token),
        nullptr,
        QRequest2::kLongRunningTransferTimeoutMs);
}

void HttpApis::changePassword(const QString& token,
                              const QString& oldPassword,
                              const QString& newPassword,
                              const ResponseCallBack<ResponseJson>& callback)
{
    const QVariantMap params = {
        { "oldPassword", oldPassword },
        { "newPassword", newPassword },
        { "confirmNewPassword", newPassword }
    };

    sendPost<ResponseJson>(fullURL(API_USER_CHANGE_PASSWORD), params, callback, token);
}

void HttpApis::getCaseListAll(const QString& token, const ResponseCallBack<CaseListJson>& callback)
{
    sendGet<CaseListJson>(QUrl(fullURL(API_CASE_LIST_ALL)), callback, token);
}

void HttpApis::getCaseList(const QString& token,
                           int project_id,
                           int page,
                           int limit,
                           const ResponseCallBack<PagedDataJson>& callback)
{
    QUrl url(fullURL(API_CASE_LIST));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("page", QString::number(page > 0 ? page : 1));
    query.addQueryItem("limit", QString::number(limit > 0 ? limit : DEFAULT_PAGE_LIMIT));
    url.setQuery(query);

    sendGet<PagedDataJson>(url, callback, token);
}

void HttpApis::addCase(const QString& token,
                       const QString& name,
                       int case_type_id,
                       const QStringList& road_types,
                       const QStringList& bus_types,
                       const QStringList& other_types,
                       int project_id,
                       const ResponseCallBack<CaseJson>& callback)
{
    QVariantMap params;
    params["name"] = name;
    params["project_id"] = project_id;
    params["case_type_id"] = case_type_id;
    params["road_types"] = road_types;
    params["bus_types"] = bus_types;
    params["other_types"] = other_types;

    sendPost<CaseJson>(fullURL(API_CASE_ADD), params, callback, token);
}

void HttpApis::editCase(const QString& token,
                        int id,
                        const QString& name,
                        int case_type_id,
                        const QStringList& road_types,
                        const QStringList& bus_types,
                        const QStringList& other_types,
                        const ResponseCallBack<ResponseJson>& callback)
{
    QVariantMap params;
    params["id"] = id;
    params["name"] = name;
    params["case_type_id"] = case_type_id;
    params["road_types"] = road_types;
    params["bus_types"] = bus_types;
    params["other_types"] = other_types;

    sendPost<ResponseJson>(fullURL(API_CASE_EDIT), params, callback, token);
}

void HttpApis::getCaseDetail(const QString& token, int id, const ResponseCallBack<CaseDetailJson>& callback)
{
    sendGet<CaseDetailJson>(QUrl(fullURL(API_CASE_DETAIL + QString("/%1").arg(id))), callback, token);
}

void HttpApis::editCaseName(const QString& token,
                            int id,
                            const QString& name,
                            const ResponseCallBack<ResponseJson>& callback)
{
    const QVariantMap params = {
        { "id", id },
        { "name", name }
    };

    sendPost<ResponseJson>(fullURL(API_CASE_EDIT_NAME), params, callback, token);
}

void HttpApis::editCaseStatus(const QString& token,
                              int id,
                              int status,
                              const ResponseCallBack<ResponseJson>& callback)
{
    const QVariantMap params = {
        { "id", id },
        { "status", status }
    };

    sendPost<ResponseJson>(fullURL(API_CASE_EDIT_STATUS), params, callback, token);
}

void HttpApis::deleteCase(const QString& token, int id, const ResponseCallBack<ResponseJson>& callback)
{
    auto resultful_callback = [callback](bool suc, const QString& msg, const QByteArray& data) {
        handleResponse<ResponseJson>(suc, msg, data, callback);
    };

    QVariantMap params;
    const QString url = fullURL(API_CASE_DEL + QString("/%1").arg(id));
    QRequest2::instance().remove(url, resultful_callback, params, defaultHeaders(), defaultRawHeaders(token));
}

void HttpApis::caseEditResult(const QString& token,
                              int case_id,
                              const QString& result,
                              const QString& url,
                              const ResponseCallBack<CaseEditResultJson>& callback)
{
    const QVariantMap params = {
        { "id", case_id },
        { "result", result },
        { "url", url }
    };

    auto resultful_callback = [callback](bool suc, const QString& msg, const QByteArray& data) {
        handleResponse<CaseEditResultJson>(suc, msg, data, callback);
    };

    QRequest2::instance().post(fullURL(API_CASE_EDIT_RESULT), resultful_callback, params, defaultHeaders(), defaultRawHeaders(token));
}

void HttpApis::caseFieldBatch(const QString& token,
                              int project_id,
                              int case_id,
                              const QString& tablefullname,
                              const QString& field,
                              int type,
                              double num,
                              const ResponseCallBack<ResponseJson>& callback)
{
    QVariantMap params;
    params["project_id"] = project_id;
    params["case_id"] = case_id;
    params["tablefullname"] = tablefullname;
    params["field"] = field;
    params["type"] = type;
    params["num"] = num;

    sendPost<ResponseJson>(fullURL(API_CASE_FIELD_BATCH), params, callback, token);
}

void HttpApis::createExpectLine(const QString& token,
                                int project_id,
                                int case_id,
                                const QString& tablefullname,
                                const ResponseCallBack<PropertyJson>& callback)
{
    QUrl url(fullURL(API_CASE_CREATE_EXPECT_LINE));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("tablefullname", tablefullname);
    url.setQuery(query);

    sendGet<PropertyJson>(url, callback, token);
}

void HttpApis::getTrace(const QString& token,
                        int project_id,
                        int case_id,
                        const QString& tablefullname,
                        int link_id,
                        const ResponseCallBack<TraceListJson>& callback)
{
    QUrl url(fullURL(API_CASE_GET_TRACE));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("tablefullname", tablefullname);
    query.addQueryItem("link_id", QString::number(link_id));
    url.setQuery(query);

    sendGet<TraceListJson>(url, callback, token);
}

void HttpApis::exportCaseSimulate(const QString& token,
                                  int project_id,
                                  int case_id,
                                  const QString& type,
                                  const ResponseCallBack<DownLoadCaseJson>& callback)
{
    QUrl url(fullURL(API_CASE_EXPORT_SIMULATE));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGetLongRunning<DownLoadCaseJson>(url, callback, token);
}

void HttpApis::grpcCaseExport(const QString& token,
                              int project_id,
                              int case_id,
                              const QString& type,
                              const ResponseCallBack<DownLoadCaseJson>& callback)
{
    QUrl url(fullURL(API_GRPC_CASE_EXPORT));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGetLongRunning<DownLoadCaseJson>(url, callback, token);
}

void HttpApis::exportPtCaseReport(const QString& token,
                                  int project_id,
                                  int case_id,
                                  const QString& type,
                                  const ResponseCallBack<DownLoadCaseJson>& callback)
{
    QUrl url(fullURL(API_CASE_PTCASE_EXPORT));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGetLongRunning<DownLoadCaseJson>(url, callback, token);
}

void HttpApis::copyCase(const QString& token,
                        int case_id,
                        const QString& name,
                        const ResponseCallBack<CaseJson>& callback)
{
    const QVariantMap params = {
        { "case_id", case_id },
        { "name", name }
    };

    sendPost<CaseJson>(fullURL(API_CASE_COPY), params, callback, token);
}

void HttpApis::copyProjectCase(const QString& token, int case_id, const ResponseCallBack<ResponseJson>& callback)
{
    const QVariantMap params = {
        { "case_id", case_id }
    };

    sendPost<ResponseJson>(fullURL(API_CASE_COPY_PROJECT), params, callback, token);
}

void HttpApis::exportCaseOsm(const QString& token,
                             int project_id,
                             int case_id,
                             const QString& type,
                             const ResponseCallBack<DownLoadCaseJson>& callback)
{
    QUrl url(fullURL(API_CASE_EXPORT_OSM));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGetLongRunning<DownLoadCaseJson>(url, callback, token);
}

void HttpApis::caseReportView(const QString& token,
                              int project_id,
                              int case_id,
                              const QString& type,
                              const ResponseCallBack<CaseReportViewJson>& callback)
{
    QUrl url(fullURL(API_CASE_REPORT_VIEW));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGet<CaseReportViewJson>(url, callback, token);
}

void HttpApis::caseReportDownload(const QString& token,
                                  int project_id,
                                  int case_id,
                                  const QString& type,
                                  const ResponseCallBack<DownLoadCaseJson>& callback)
{
    QUrl url(fullURL(API_CASE_REPORT_DOWNLOAD));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGetLongRunning<DownLoadCaseJson>(url, callback, token);
}

void HttpApis::caseBaseDataView(const QString& token,
                                int project_id,
                                const ResponseCallBack<CaseReportViewJson>& callback)
{
    QUrl url(fullURL(API_CASE_BASEDATA_VIEW));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    url.setQuery(query);

    sendGet<CaseReportViewJson>(url, callback, token);
}

void HttpApis::caseCostView(const QString& token,
                            int project_id,
                            int case_id,
                            const QString& type,
                            const ResponseCallBack<CaseReportViewJson>& callback)
{
    QUrl url(fullURL(API_CASE_COST_VIEW));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGet<CaseReportViewJson>(url, callback, token);
}

void HttpApis::caseComparisonView(const QString& token,
                                  int project_id,
                                  const QString& case_ids,
                                  const QString& check_extent,
                                  const QString& type,
                                  const ResponseCallBack<CaseReportViewJson>& callback)
{
    QUrl url(fullURL(API_CASE_CASECOMPARISON_VIEW));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_ids", case_ids);
    query.addQueryItem("check_extent", check_extent);
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGet<CaseReportViewJson>(url, callback, token);
}

void HttpApis::caseEditLog(const QString& token,
                           int project_id,
                           int case_id,
                           int page,
                           int limit,
                           const QString& type,
                           const ResponseCallBack<PagedDataJson>& callback)
{
    QUrl url(fullURL(API_CASE_EDITLOG));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("limit", QString::number(limit));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGet<PagedDataJson>(url, callback, token);
}

void HttpApis::getNewsList(int bid, const ResponseCallBack<NewsListJson>& callback)
{
    QUrl url(fullURL(API_NEWS));
    if (bid > 0) {
        QUrlQuery query;
        query.addQueryItem("bid", QString::number(bid));
        url.setQuery(query);
    }

    sendGet<NewsListJson>(url, callback);
}

void HttpApis::getNewsDetail(int id, const ResponseCallBack<NewsDetailJson>& callback)
{
    sendGet<NewsDetailJson>(QUrl(fullURL(API_NEWS + QString("/%1").arg(id))), callback);
}

void HttpApis::grpcCheck(const QString& token,
                         int project_id,
                         int case_id,
                         const QString& type,
                         const ResponseCallBack<GrpcCheckResultJson>& callback)
{
    QUrl url(fullURL(API_GRPC_CHECK));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    url.setQuery(query);

    sendGetLongRunning<GrpcCheckResultJson>(url, callback, token);
}

void HttpApis::grpcRunAlg(const QString& token,
                          int project_id,
                          int case_id,
                          const QString& type,
                          const QString& type2,
                          const QString& json,
                          const ResponseCallBack<GrpcRunAlgResultJson>& callback)
{
    QUrl url(fullURL(API_GRPC_RUNALG));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    query.addQueryItem("type2", type2);
    if (!json.trimmed().isEmpty()) {
        query.addQueryItem("json", json);
    }
    url.setQuery(query);

    sendGetLongRunning<GrpcRunAlgResultJson>(url, callback, token);
}

void HttpApis::grpcAiApi(const QString& token,
                         int project_id,
                         int case_id,
                         const QString& type,
                         const QString& type2,
                         const QString& prompt,
                         const QString& analysis_object,
                         const QString& engineer_notes,
                         const QString& business_goal,
                         const QString& planning_targets,
                         const QString& constraints,
                         const QString& ids,
                         const ResponseCallBack<GrpcAiApiResultJson>& callback)
{
    qDebug() << "aiapi Get, params as type = " << type
        << ", type2 = " << type2 << ", prompt = " << prompt
        << ", analysis_object = " << analysis_object << ", engineer_notes = " << engineer_notes
        << ", business_goal = " << business_goal << ", planning_targets = " << planning_targets
        << ", constraints = " << constraints;
    QUrl url(fullURL(API_GRPC_AIAPI));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    query.addQueryItem("type2", type2);
    query.addQueryItem("token", token);
    query.addQueryItem("prompt", prompt);
    query.addQueryItem("analysis_object", analysis_object);
    query.addQueryItem("engineer_notes", engineer_notes);
    query.addQueryItem("business_goal", business_goal);
    query.addQueryItem("planning_targets", planning_targets);
    query.addQueryItem("constraints", constraints);
    if (!ids.isEmpty()) {
        query.addQueryItem("ids", ids);
    }
    url.setQuery(query);

    sendGetLongRunning<GrpcAiApiResultJson>(url, callback, token);
}

GrpcStreamClient* HttpApis::createGrpcStream(QObject* parent)
{
    return new GrpcStreamClient(parent);
}

GrpcAiStreamClient* HttpApis::createGrpcAiStream(QObject* parent)
{
    return new GrpcAiStreamClient(parent);
}

void HttpApis::wfsGetCapabilities(const RawResponseCallback& callback)
{
    QUrl url(fullURL(API_WFS));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("request"), QStringLiteral("GetCapabilities"));
    url.setQuery(query);
    sendGetRaw(url, callback);
}

void HttpApis::wfsDescribeFeatureType(const QString& typeName, const RawResponseCallback& callback)
{
    QUrl url(fullURL(API_WFS));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("request"), QStringLiteral("DescribeFeatureType"));
    query.addQueryItem(QStringLiteral("typeName"), typeName);
    url.setQuery(query);
    sendGetRaw(url, callback);
}

void HttpApis::wfsGetFeature(const QVariantMap& queryParams, const RawResponseCallback& callback)
{
    QUrl url(fullURL(API_WFS));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("request"), QStringLiteral("GetFeature"));
    for (auto it = queryParams.constBegin(); it != queryParams.constEnd(); ++it) {
        query.addQueryItem(it.key(), it.value().toString());
    }
    url.setQuery(query);
    sendGetRaw(url, callback);
}

void HttpApis::wfsCapabilitiesShortcut(const RawResponseCallback& callback)
{
    sendGetRaw(QUrl(fullURL(API_WFS_CAPABILITIES)), callback);
}

void HttpApis::wfsFeatureShortcut(const QVariantMap& queryParams, const RawResponseCallback& callback)
{
    QUrl url(fullURL(API_WFS_FEATURE));
    QUrlQuery query;
    for (auto it = queryParams.constBegin(); it != queryParams.constEnd(); ++it) {
        query.addQueryItem(it.key(), it.value().toString());
    }
    url.setQuery(query);
    sendGetRaw(url, callback);
}

namespace {

bool isUsableWfsPrimaryKeyValue(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return false;
    }
    if (value.userType() == QMetaType::QString) {
        return !value.toString().trimmed().isEmpty();
    }
    return true;
}

QVariantMap buildWfsFeatureKeyBody(const QString& typeName,
                                   const QString& featureId,
                                   const QString& primaryKeyField,
                                   const QVariant& primaryKeyValue,
                                   const QVariantMap& feature)
{
    QVariantMap body;
    body.insert(QStringLiteral("typeName"), typeName);
    const QVariant effectiveFeatureId = isUsableWfsPrimaryKeyValue(primaryKeyValue)
                                          ? primaryKeyValue
                                          : QVariant(featureId);
    body.insert(QStringLiteral("featureId"), effectiveFeatureId);
    if (!primaryKeyField.trimmed().isEmpty() && isUsableWfsPrimaryKeyValue(primaryKeyValue)) {
        body.insert(primaryKeyField, primaryKeyValue);
    }
    if (!feature.isEmpty()) {
        body.insert(QStringLiteral("feature"), feature);
    }
    return body;
}

} // namespace

QVariantMap HttpApis::buildWfsBatchDeleteItem(const QString& typeName,
                                              const QString& featureId,
                                              const QString& primaryKeyField,
                                              const QVariant& primaryKeyValue)
{
    return buildWfsFeatureKeyBody(typeName, featureId, primaryKeyField, primaryKeyValue, {});
}

QVariantMap HttpApis::buildWfsBatchUpdateItem(const QString& typeName,
                                              const QString& featureId,
                                              const QVariantMap& feature,
                                              const QString& primaryKeyField,
                                              const QVariant& primaryKeyValue)
{
    return buildWfsFeatureKeyBody(typeName, featureId, primaryKeyField, primaryKeyValue, feature);
}

void HttpApis::wfsFeatureInsert(const QVariantMap& body, const RawResponseCallback& callback)
{
    auto wrapped = [callback](bool suc, const QString& msg, const QByteArray& data) {
        callback(suc, data, msg);
    };
    QRequest2::instance().post(fullURL(API_WFS_FEATURE), wrapped, body, defaultHeaders(), QVariantMap());
}

void HttpApis::wfsFeatureUpdate(const QVariantMap& body, const RawResponseCallback& callback)
{
    auto wrapped = [callback](bool suc, const QString& msg, const QByteArray& data) {
        callback(suc, data, msg);
    };
    QRequest2::instance().put(fullURL(API_WFS_FEATURE), wrapped, body, defaultHeaders(), QVariantMap());
}

void HttpApis::wfsFeatureDelete(const QVariantMap& body, const RawResponseCallback& callback)
{
    auto wrapped = [callback](bool suc, const QString& msg, const QByteArray& data) {
        callback(suc, data, msg);
    };
    QRequest2::instance().remove(fullURL(API_WFS_FEATURE), wrapped, body, defaultHeaders(), QVariantMap());
}

void HttpApis::wfsBatchTransactionJson(const QVariantMap& body, const QString& authToken, const RawResponseCallback& callback)
{
    auto wrapped = [callback](bool suc, const QString& msg, const QByteArray& data) {
        callback(suc, data, msg);
    };
    QRequest2::instance().post(fullURL(API_WFS), wrapped, body, defaultHeaders(), defaultRawHeaders(authToken));
}

QMap<QNetworkRequest::KnownHeaders, QVariant> HttpApis::defaultHeaders()
{
    QMap<QNetworkRequest::KnownHeaders, QVariant> headers;
    headers[QNetworkRequest::ContentTypeHeader] = "application/json";
    return headers;
}

QVariantMap HttpApis::defaultRawHeaders(const QString& token)
{
    QVariantMap rawHeaders;
    if (!token.isEmpty()) {
        rawHeaders["Authorization"] = "Bearer " + token;
    }
    return rawHeaders;
}
