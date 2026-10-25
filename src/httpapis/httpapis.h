#ifndef HTTPAPIS_H
#define HTTPAPIS_H

#include "httpapijson.h"
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QtNetwork>

// 文档约定默认分页大小为 10。
#define DEFAULT_PAGE_LIMIT 50

class HttpApisPrivate;
class GrpcStreamClient;
class GrpcAiStreamClient;

/**
 * @brief HTTP 接口统一封装入口
 * @details
 * 所有方法均直接对应 `docs/api.md` 中的接口定义。
 * 需要鉴权的接口由调用方显式传入 token，公开接口则不附带 Authorization。
 */
class HttpApis {
public:
    /**
     * @brief 统一异步回调签名
     * @param suc 请求与业务状态是否成功
     * @param response 已解析的 data 对象
     * @param errMsg 失败时的错误消息
     */
    template <class T>
    using ResponseCallBack = std::function<void(bool suc, const QSharedPointer<T>& response, const QString& errMsg)>;

    /**
     * @brief 原始响应回调（用于 WFS 等非统一 {code,data} 包装的接口）
     */
    using RawResponseCallback = std::function<void(bool suc, const QByteArray& body, const QString& errMsg)>;

public:
    HttpApis();
    virtual ~HttpApis();

public:
    static HttpApis& instance();

    // 基础与用户接口
    static void getVersion(const ResponseCallBack<VersionResponseJson>& callback);
    static void login(const QString& username, const QString& password, const ResponseCallBack<LoginResponseJson>& callback);
    static void getBaseConfig(const ResponseCallBack<BaseConfigJson>& callback);
    static void getAiConfig(const ResponseCallBack<PropertyJson>& callback);
    static void getCommonQuestions(int isbase,
                                   const QString& type,
                                   const ResponseCallBack<QuestionListJson>& callback);
    static void getRegions(int province, const ResponseCallBack<RegionListJson>& callback);
    static void getFieldChinese(const ResponseCallBack<PropertyJson>& callback);

    // 项目接口
    static void getProjectList(const QString& token,
                               int pageindex,
                               int limit,
                               const QString& name,
                               const ResponseCallBack<PagedDataJson>& callback);
    static void createProject(const QString& token,
                              const QString& name,
                              const QString& desc,
                              int provinceId,
                              const QString& province,
                              int cityId,
                              const QString& city,
                              const ResponseCallBack<PropertyJson>& callback);
    static void editProject(const QString& token,
                            int id,
                            const QString& name,
                            const QString& desc,
                            int provinceId,
                            const QString& province,
                            int cityId,
                            const QString& city,
                            const ResponseCallBack<ResponseJson>& callback);
    static void projectDetail(const QString& token, int id, const ResponseCallBack<ProjectDetailJson>& callback);
    static void deleteProject(const QString& token, int id, const ResponseCallBack<ResponseJson>& callback);
    static void clearProjectTable(const QString& token,
                                  int project_id,
                                  const QString& table,
                                  const ResponseCallBack<ResponseJson>& callback);
    static void exportProjectTable(const QString& token,
                                   const QString& type,
                                   int project_id,
                                   int case_id,
                                   const QString& table,
                                   const ResponseCallBack<ProjectExportResult>& callback);
    static void exportCase(const QString& token,
                           int project_id,
                           int case_id,
                           const ResponseCallBack<DownLoadCaseJson>& callback);
    static void projectUpdateCentroid(const QString& token,
                                      int projectId,
                                      int caseId,
                                      const QString& tableFullName,
                                      const ResponseCallBack<UpdateCentroidJson>& callback);
    static void clipLayer(const QString& token,
                          int project_id,
                          int case_id,
                          const QString& community_table,
                          const QString& way_table,
                          const QString& clip_name,
                          const ResponseCallBack<ResponseJson>& callback);
    static void clipPart(const QString& token,
                         int project_id,
                         int case_id,
                         const QString& way_table,
                         const QString& clip_name,
                         const QString& wkt,
                         const ResponseCallBack<ResponseJson>& callback);
    static void deleteClipLayer(const QString& token,
                                int project_id,
                                int case_id,
                                const QString& tablefullname,
                                const ResponseCallBack<ResponseJson>& callback);
    static void projectBaseAnalysis(const QString& token,
                                    int project_id,
                                    const ResponseCallBack<PropertyJson>& callback);
    static void projectPopData(const QString& token,
                               int project_id,
                               const QVariantList& dataList,
                               const ResponseCallBack<ProjectPopDataResultJson>& callback);
    static void projectGenCommunity(const QString& token,
                                    int project_id,
                                    const QVariantList& dataList,
                                    const ResponseCallBack<ProjectGenCommunityResultJson>& callback);
    static void checkProjectObs(const QString& token,
                                int project_id,
                                const ResponseCallBack<ProjectCheckObsResultJson>& callback);
    static void selectDataAll(const QString& token,
                              const QString& tablefullname,
                              const ResponseCallBack<SelectDataJson>& callback);
    static void selectDataPage(const QString& token,
                               const QString& tablefullname,
                               int page,
                               int limit,
                               const ResponseCallBack<SelectDataPageJson>& callback);

    // 文件接口
    static void uploadCSV(const QString& token,
                          const QString& type,
                          int project_id,
                          int case_id,
                          const QString& table,
                          const QString& filePath,
                          const ResponseCallBack<ResponseJson>& callback);
    /** @brief 上传 SHP 打包后的 zip 压缩包（服务端解压导入） */
    static void uploadShpZip(const QString& token,
                             const QString& type,
                             int project_id,
                             int case_id,
                             const QString& table,
                             const QString& filePath,
                             const ResponseCallBack<ResponseJson>& callback);
    static void changePassword(const QString& token,
                               const QString& oldPassword,
                               const QString& newPassword,
                               const ResponseCallBack<ResponseJson>& callback);

    // 方案接口
    static void getCaseListAll(const QString& token, const ResponseCallBack<CaseListJson>& callback);
    static void getCaseList(const QString& token,
                            int project_id,
                            int page,
                            int limit,
                            const ResponseCallBack<PagedDataJson>& callback);
    static void addCase(const QString& token,
                        const QString& name,
                        int case_type_id,
                        const QStringList& road_types,
                        const QStringList& bus_types,
                        const QStringList& other_types,
                        int project_id,
                        const ResponseCallBack<CaseJson>& callback);
    static void editCase(const QString& token,
                         int id,
                         const QString& name,
                         int case_type_id,
                         const QStringList& road_types,
                         const QStringList& bus_types,
                         const QStringList& other_types,
                         const ResponseCallBack<ResponseJson>& callback);
    static void getCaseDetail(const QString& token, int id, const ResponseCallBack<CaseDetailJson>& callback);
    static void editCaseName(const QString& token,
                             int id,
                             const QString& name,
                             const ResponseCallBack<ResponseJson>& callback);
    static void editCaseStatus(const QString& token,
                               int id,
                               int status,
                               const ResponseCallBack<ResponseJson>& callback);
    static void deleteCase(const QString& token, int id, const ResponseCallBack<ResponseJson>& callback);
    static void caseEditResult(const QString& token,
                               int case_id,
                               const QString& result,
                               const QString& url,
                               const ResponseCallBack<CaseEditResultJson>& callback);
    static void caseFieldBatch(const QString& token,
                               int project_id,
                               int case_id,
                               const QString& tablefullname,
                               const QString& field,
                               int type,
                               double num,
                               const ResponseCallBack<ResponseJson>& callback);
    static void createExpectLine(const QString& token,
                                 int project_id,
                                 int case_id,
                                 const QString& tablefullname,
                                 const ResponseCallBack<PropertyJson>& callback);
    static void getTrace(const QString& token,
                         int project_id,
                         int case_id,
                         const QString& tablefullname,
                         int link_id,
                         const ResponseCallBack<TraceListJson>& callback);
    static void exportCaseSimulate(const QString& token,
                                   int project_id,
                                   int case_id,
                                   const QString& type,
                                   const ResponseCallBack<DownLoadCaseJson>& callback);
    static void grpcCaseExport(const QString& token,
                               int project_id,
                               int case_id,
                               const QString& type,
                               const ResponseCallBack<DownLoadCaseJson>& callback);
    static void exportPtCaseReport(const QString& token,
                                   int project_id,
                                   int case_id,
                                   const QString& type,
                                   const ResponseCallBack<DownLoadCaseJson>& callback);
    static void copyCase(const QString& token,
                         int case_id,
                         const QString& name,
                         const ResponseCallBack<CaseJson>& callback);
    static void copyProjectCase(const QString& token, int case_id, const ResponseCallBack<ResponseJson>& callback);
    static void exportCaseOsm(const QString& token,
                              int project_id,
                              int case_id,
                              const QString& type,
                              const ResponseCallBack<DownLoadCaseJson>& callback);
    static void caseReportView(const QString& token,
                               int project_id,
                               int case_id,
                               const QString& type,
                               const ResponseCallBack<CaseReportViewJson>& callback);
    static void caseReportDownload(const QString& token,
                                   int project_id,
                                   int case_id,
                                   const QString& type,
                                   const ResponseCallBack<DownLoadCaseJson>& callback);
    static void caseBaseDataView(const QString& token,
                                 int project_id,
                                 const ResponseCallBack<CaseReportViewJson>& callback);
    static void caseCostView(const QString& token,
                             int project_id,
                             int case_id,
                             const QString& type,
                             const ResponseCallBack<CaseReportViewJson>& callback);
    static void caseComparisonView(const QString& token,
                                   int project_id,
                                   const QString& case_ids,
                                   const QString& check_extent,
                                   const QString& type,
                                   const ResponseCallBack<CaseReportViewJson>& callback);
    static void caseEditLog(const QString& token,
                            int project_id,
                            int case_id,
                            int page,
                            int limit,
                            const QString& type,
                            const ResponseCallBack<PagedDataJson>& callback);

    // 文章接口
    static void getNewsList(int bid, const ResponseCallBack<NewsListJson>& callback);
    static void getNewsDetail(int id, const ResponseCallBack<NewsDetailJson>& callback);

    // 算法接口
    static void grpcCheck(const QString& token,
                          int project_id,
                          int case_id,
                          const QString& type,
                          const ResponseCallBack<GrpcCheckResultJson>& callback);
    static void grpcRunAlg(const QString& token,
                           int project_id,
                           int case_id,
                           const QString& type,
                           const QString& type2,
                           const QString& json,
                           const ResponseCallBack<GrpcRunAlgResultJson>& callback);
    static void grpcAiApi(const QString& token,
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
                          const ResponseCallBack<GrpcAiApiResultJson>& callback);
    /**
     * @brief 创建 gRPC 算法 SSE 流客户端（独立管理与 HttpApis 静态 GET 不同的长连接）
     */
    static GrpcStreamClient* createGrpcStream(QObject* parent = nullptr);
    static GrpcAiStreamClient* createGrpcAiStream(QObject* parent = nullptr);

    // WFS（公开接口，body 为原始 JSON / GeoJSON）
    static void wfsGetCapabilities(const RawResponseCallback& callback);
    static void wfsDescribeFeatureType(const QString& typeName, const RawResponseCallback& callback);
    static void wfsGetFeature(const QVariantMap& queryParams, const RawResponseCallback& callback);
    static void wfsCapabilitiesShortcut(const RawResponseCallback& callback);
    static void wfsFeatureShortcut(const QVariantMap& queryParams, const RawResponseCallback& callback);
    static void wfsFeatureInsert(const QVariantMap& body, const RawResponseCallback& callback);
    static void wfsFeatureUpdate(const QVariantMap& body, const RawResponseCallback& callback);
    static void wfsFeatureDelete(const QVariantMap& body, const RawResponseCallback& callback);
    static void wfsBatchTransactionJson(const QVariantMap& body, const QString& authToken, const RawResponseCallback& callback);

    /**
     * @brief 构造 POST /wfs 批量 deletes[] 单条元素
     * @details primaryKeyValue 有效时 featureId 使用主键值，否则用 fallback；并写入动态主键字段
     */
    static QVariantMap buildWfsBatchDeleteItem(const QString& typeName,
                                             const QString& featureId,
                                             const QString& primaryKeyField = {},
                                             const QVariant& primaryKeyValue = {});

    /**
     * @brief 构造 POST /wfs 批量 updates[] 单条元素
     * @details primaryKeyValue 有效时 featureId 使用主键值，否则用 fallback；并写入动态主键字段
     */
    static QVariantMap buildWfsBatchUpdateItem(const QString& typeName,
                                             const QString& featureId,
                                             const QVariantMap& feature,
                                             const QString& primaryKeyField = {},
                                             const QVariant& primaryKeyValue = {});

public:
    /**
     * @brief 默认已知请求头
     * @details 当前统一按 JSON body 请求发送
     */
    static QMap<QNetworkRequest::KnownHeaders, QVariant> defaultHeaders();

    /**
     * @brief 根据 token 构造原始请求头
     * @details token 为空时返回空 headers，便于同一套发送逻辑兼容公开接口
     */
    static QVariantMap defaultRawHeaders(const QString& token);

    template <class T>
    static void handleResponse(bool suc, const QString& msg, const QByteArray& data, const ResponseCallBack<T>& callback)
    {
        if (!suc && data.isEmpty()) {
            callback(false, nullptr, msg);
            return;
        }

        ResponseJson responseJson(data);
        if (responseJson.j_code() != 200) {
            callback(false, nullptr, responseJson.j_message());
            return;
        }

        // 约定所有接口都按 { code, message, data } 包装，业务对象统一从 data 解包。
        callback(true, QJsonReader::dataAsObjectPtr<T>(responseJson.j_data()), QString());
    }

private:
    HttpApisPrivate* d_p = nullptr;
};

#endif // HTTPAPIS_H
