#ifndef HTTPAPIJSON_H
#define HTTPAPIJSON_H

#include "qjsonreader.h"
#include <QtCore>

/**
 * @brief 通用属性值包装
 * @details 用于 `data` 为简单对象、布尔值或字符串等不值得单独建模的场景
 */
class PropertyJson : public QJsonReader {
public:
    PropertyJson() = default;
    PropertyJson(const PropertyJson& other) = default;
    explicit PropertyJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit PropertyJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    template <typename T>
    T value() const
    {
        return m_jsonValue.toVariant().value<T>();
    }
};
Q_DECLARE_METATYPE(PropertyJson);

/**
 * @brief `/api/config/question` 常见问题项
 */
class QuestionItemJson : public QJsonReader {
public:
    QuestionItemJson() = default;
    QuestionItemJson(const QuestionItemJson& other) = default;
    explicit QuestionItemJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit QuestionItemJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, name)
};
Q_DECLARE_METATYPE(QuestionItemJson);

/**
 * @brief `/api/config/question` 常见问题列表
 */
class QuestionListJson : public QJsonReader {
public:
    QuestionListJson() = default;
    QuestionListJson(const QuestionListJson& other) = default;
    explicit QuestionListJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit QuestionListJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE2(QList<QuestionItemJson>, data)
};
Q_DECLARE_METATYPE(QuestionListJson);

/**
 * @brief `/api/config/regions` 省市项
 */
class RegionItemJson : public QJsonReader {
public:
    RegionItemJson() = default;
    RegionItemJson(const RegionItemJson& other) = default;
    explicit RegionItemJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit RegionItemJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(QString, name)
};
Q_DECLARE_METATYPE(RegionItemJson);

/**
 * @brief `/api/config/regions` 省市列表
 */
class RegionListJson : public QJsonReader {
public:
    RegionListJson() = default;
    RegionListJson(const RegionListJson& other) = default;
    explicit RegionListJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit RegionListJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE2(QList<RegionItemJson>, data)
};
Q_DECLARE_METATYPE(RegionListJson);

/**
 * @brief 接口统一响应外层包装
 * @details 所有接口默认遵循 { code, message, data } 结构
 */
class ResponseJson : public QJsonReader {
public:
    ResponseJson() = default;
    ResponseJson(const ResponseJson& other) = default;
    explicit ResponseJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ResponseJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, code)
    PROPERTY_DEFINE(QString, message)
    PROPERTY_DEFINE(QJsonValue, data)
};
Q_DECLARE_METATYPE(ResponseJson);

/**
 * @brief 登录用户信息
 */
class UserJson : public QJsonReader {
public:
    UserJson() = default;
    UserJson(const UserJson& other) = default;
    explicit UserJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit UserJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, user_id)
    PROPERTY_DEFINE(QString, username)
    PROPERTY_DEFINE(QString, real_name)
    PROPERTY_DEFINE(QString, mobile)
    PROPERTY_DEFINE(int, status)
    PROPERTY_DEFINE(int, project_num)
    PROPERTY_DEFINE(QString, url)
    PROPERTY_DEFINE(QString, mapurl)
    PROPERTY_DEFINE(QString, mapurl_vector)
    PROPERTY_DEFINE(QString, mapurl_note)
};
Q_DECLARE_METATYPE(UserJson);

/**
 * @brief 登录接口返回数据
 */
class LoginResponseJson : public QJsonReader {
public:
    LoginResponseJson() = default;
    LoginResponseJson(const LoginResponseJson& other) = default;
    explicit LoginResponseJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit LoginResponseJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, token)
    PROPERTY_DEFINE(UserJson, user)
};
Q_DECLARE_METATYPE(LoginResponseJson);

/**
 * @brief 产品版本信息
 */
class VersionResponseJson : public QJsonReader {
public:
    VersionResponseJson() = default;
    VersionResponseJson(const VersionResponseJson& other) = default;
    explicit VersionResponseJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit VersionResponseJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(QString, version)
    PROPERTY_DEFINE(QString, url)
    PROPERTY_DEFINE(QString, fileName)
    PROPERTY_DEFINE(QString, md5)
    PROPERTY_DEFINE(QString, desc)
    PROPERTY_DEFINE(qint64, size)
    PROPERTY_DEFINE(QString, created_at)
};
Q_DECLARE_METATYPE(VersionResponseJson);

/**
 * @brief 基础配置标题项
 */
class BaseConfigHelpItemJson : public QJsonReader {
public:
    BaseConfigHelpItemJson() = default;
    BaseConfigHelpItemJson(const BaseConfigHelpItemJson& other) = default;
    explicit BaseConfigHelpItemJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit BaseConfigHelpItemJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(QString, title)
    PROPERTY_DEFINE(QString, img)
    PROPERTY_DEFINE(QString, url)
};
Q_DECLARE_METATYPE(BaseConfigHelpItemJson);

class BaseConfigTitleJson : public QJsonReader {
public:
    BaseConfigTitleJson() = default;
    BaseConfigTitleJson(const BaseConfigTitleJson& other) = default;
    explicit BaseConfigTitleJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit BaseConfigTitleJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(QString, tit)
    PROPERTY_DEFINE(QString, title)
};
Q_DECLARE_METATYPE(BaseConfigTitleJson);

/**
 * @brief `/api/config/base` 返回的基础配置对象
 */
class BaseConfigJson : public QJsonReader {
public:
    BaseConfigJson() = default;
    BaseConfigJson(const BaseConfigJson& other) = default;
    explicit BaseConfigJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit BaseConfigJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, site_name)
    PROPERTY_DEFINE(QString, mapurl)
    PROPERTY_DEFINE(QString, mapurl2)
    PROPERTY_DEFINE(QString, mapurl3)
    PROPERTY_DEFINE(QString, maxkb)
    PROPERTY_DEFINE(QString, mapselurl)
    PROPERTY_DEFINE(QString, tit1)
    PROPERTY_DEFINE(QString, tit2)
    PROPERTY_DEFINE(QList<BaseConfigTitleJson>, titlist)
    PROPERTY_DEFINE(QString, contact)
    PROPERTY_DEFINE(QString, email1)
    PROPERTY_DEFINE(QString, email2)
    PROPERTY_DEFINE(QString, phone)
    PROPERTY_DEFINE(QString, address)
    PROPERTY_DEFINE(QString, study_url)
    PROPERTY_DEFINE(QString, manual_url)
    PROPERTY_DEFINE(QString, ai_url)
    PROPERTY_DEFINE(QList<BaseConfigHelpItemJson>, help_list)
};
Q_DECLARE_METATYPE(BaseConfigJson);

/**
 * @brief 项目列表项
 */
class ProjectItemJson : public QJsonReader {
public:
    ProjectItemJson() = default;
    ProjectItemJson(const ProjectItemJson& other) = default;
    explicit ProjectItemJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ProjectItemJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(QString, name)
    PROPERTY_DEFINE(QString, desc)
    PROPERTY_DEFINE(QList<int>, users)
    PROPERTY_DEFINE(int, num)
    PROPERTY_DEFINE(int, status)
    PROPERTY_DEFINE(int, sort)
    PROPERTY_DEFINE(int, is_delete)
    PROPERTY_DEFINE(QString, created_at)
    PROPERTY_DEFINE(QString, updated_at)
    PROPERTY_DEFINE(int, province_id)
    PROPERTY_DEFINE(QString, province)
    PROPERTY_DEFINE(int, city_id)
    PROPERTY_DEFINE(QString, city)
};
Q_DECLARE_METATYPE(ProjectItemJson);

/**
 * @brief 通用分页结构
 * @details `data` 保留为 `QJsonObject` 列表，便于同一类型兼容项目/方案等不同列表项
 */
class PagedDataJson : public QJsonReader {
public:
    PagedDataJson() = default;
    PagedDataJson(const PagedDataJson& other) = default;
    explicit PagedDataJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit PagedDataJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, total)
    PROPERTY_DEFINE(int, page)
    PROPERTY_DEFINE(int, limit)
    PROPERTY_DEFINE(int, totalPages)
    PROPERTY_DEFINE(QList<QJsonObject>, data)
};
Q_DECLARE_METATYPE(PagedDataJson);

/**
 * @brief 项目/方案图层树叶子节点
 */
class MenuChildrenJson : public QJsonReader {
public:
    MenuChildrenJson() = default;
    MenuChildrenJson(const MenuChildrenJson& other) = default;
    explicit MenuChildrenJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit MenuChildrenJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, name)
    PROPERTY_DEFINE(QString, value)
    PROPERTY_DEFINE(QString, val)
    PROPERTY_DEFINE(int, level)
};
Q_DECLARE_METATYPE(MenuChildrenJson);

/**
 * @brief 项目/方案图层树分组节点
 */
class MenuJson : public QJsonReader {
public:
    MenuJson() = default;
    MenuJson(const MenuJson& other) = default;
    explicit MenuJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit MenuJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, name)
    PROPERTY_DEFINE(QString, value)
    PROPERTY_DEFINE(QString, val)
    PROPERTY_DEFINE(int, level)
    PROPERTY_DEFINE(QList<MenuChildrenJson>, children)
};
Q_DECLARE_METATYPE(MenuJson);

/**
 * @brief 项目详情工具节点
 */
class ProjectToolJson : public QJsonReader {
public:
    ProjectToolJson() = default;
    ProjectToolJson(const ProjectToolJson& other) = default;
    explicit ProjectToolJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ProjectToolJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, name)
    PROPERTY_DEFINE(QString, type)
    PROPERTY_DEFINE(QString, url)
};
Q_DECLARE_METATYPE(ProjectToolJson);

/**
 * @brief `/api/project/popdata` 返回的 data
 */
class ProjectPopDataResultJson : public QJsonReader {
public:
    ProjectPopDataResultJson() = default;
    ProjectPopDataResultJson(const ProjectPopDataResultJson& other) = default;
    explicit ProjectPopDataResultJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ProjectPopDataResultJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, message)
};
Q_DECLARE_METATYPE(ProjectPopDataResultJson);

/**
 * @brief `/api/project/gen_community` 返回的 data
 */
class ProjectGenCommunityResultJson : public QJsonReader {
public:
    ProjectGenCommunityResultJson() = default;
    ProjectGenCommunityResultJson(const ProjectGenCommunityResultJson& other) = default;
    explicit ProjectGenCommunityResultJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ProjectGenCommunityResultJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(bool, success)
    PROPERTY_DEFINE(int, inserted)
    PROPERTY_DEFINE(QString, message)
};
Q_DECLARE_METATYPE(ProjectGenCommunityResultJson);

/**
 * @brief `/api/project/check_obs` 返回的 data
 * @details obs：1 有路段观测流量数据，0 无数据
 */
class ProjectCheckObsResultJson : public QJsonReader {
public:
    ProjectCheckObsResultJson() = default;
    ProjectCheckObsResultJson(const ProjectCheckObsResultJson& other) = default;
    explicit ProjectCheckObsResultJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ProjectCheckObsResultJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, obs)
};
Q_DECLARE_METATYPE(ProjectCheckObsResultJson);

/**
 * @brief 项目/方案详情中的数据库连接配置
 */
class DBConfigJson : public QJsonReader {
public:
    DBConfigJson() = default;
    DBConfigJson(const DBConfigJson& other) = default;
    explicit DBConfigJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit DBConfigJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, host)
    PROPERTY_DEFINE(int, port)
    PROPERTY_DEFINE(QString, dbname)
    PROPERTY_DEFINE(QString, user)
    PROPERTY_DEFINE(QString, password)
    PROPERTY_DEFINE(QString, scenario_prefix)
};
Q_DECLARE_METATYPE(DBConfigJson);

/**
 * @brief 算法参数配置
 */
class AlgorithmParamsJson : public QJsonReader {
public:
    AlgorithmParamsJson() = default;
    AlgorithmParamsJson(const AlgorithmParamsJson& other) = default;
    explicit AlgorithmParamsJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit AlgorithmParamsJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, conv)
    PROPERTY_DEFINE(int, max_iter)
    PROPERTY_DEFINE(int, cost_scalar)
};
Q_DECLARE_METATYPE(AlgorithmParamsJson);

/**
 * @brief 方案数据统计信息
 */
class CaseCountJson : public QJsonReader {
public:
    CaseCountJson() = default;
    CaseCountJson(const CaseCountJson& other) = default;
    explicit CaseCountJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit CaseCountJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, other_od)
    PROPERTY_DEFINE(int, other_toll_gate)
    PROPERTY_DEFINE(int, other_mobile_cdr)
    PROPERTY_DEFINE(int, other_bus_route)
    PROPERTY_DEFINE(int, other_bus_customized)
    PROPERTY_DEFINE(int, other_bus_school)
    PROPERTY_DEFINE(int, other_taxi_trip)
    PROPERTY_DEFINE(int, other_population)
    PROPERTY_DEFINE(int, other_observation)
    PROPERTY_DEFINE(int, other_taz_socioeconomic)
    PROPERTY_DEFINE(int, other_trip_production_attraction)
    PROPERTY_DEFINE(int, other_trip_distribution_hist)
    PROPERTY_DEFINE(int, road_point)
    PROPERTY_DEFINE(int, road_way)
    PROPERTY_DEFINE(int, road_community)
    PROPERTY_DEFINE(int, bus_point)
    PROPERTY_DEFINE(int, bus_way)
    PROPERTY_DEFINE(int, bus_community)
    PROPERTY_DEFINE(int, pt_stop)
    PROPERTY_DEFINE(int, pt_route)
    PROPERTY_DEFINE(int, pt_shape)
    PROPERTY_DEFINE(int, pt_transit)
    PROPERTY_DEFINE(int, pt_walk)
    PROPERTY_DEFINE(int, pt_trip)
    PROPERTY_DEFINE(int, slow_road_point)
    PROPERTY_DEFINE(int, slow_road_way)
    PROPERTY_DEFINE(int, slow_road_community)
    PROPERTY_DEFINE(int, slow_other_od)
};
Q_DECLARE_METATYPE(CaseCountJson);

/**
 * @brief 项目详情
 */
class ProjectDetailJson : public QJsonReader {
public:
    ProjectDetailJson() = default;
    ProjectDetailJson(const ProjectDetailJson& other) = default;
    explicit ProjectDetailJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ProjectDetailJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(ProjectItemJson, info)
    PROPERTY_DEFINE(QString, dbfix)
    PROPERTY_DEFINE(QString, dbuserfix)
    PROPERTY_DEFINE(QString, osmurl)
    PROPERTY_DEFINE(AlgorithmParamsJson, algorithm_params)
    PROPERTY_DEFINE(DBConfigJson, db_conn_str)
    PROPERTY_DEFINE(QList<MenuJson>, menu)
    PROPERTY_DEFINE(QList<ProjectToolJson>, tools)
};
Q_DECLARE_METATYPE(ProjectDetailJson);

/**
 * @brief 单个方案的基础信息
 */
class CaseJson : public QJsonReader {
public:
    CaseJson() = default;
    CaseJson(const CaseJson& other) = default;
    explicit CaseJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit CaseJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(int, project_id)
    PROPERTY_DEFINE(int, user_id)
    PROPERTY_DEFINE(int, case_type_id)
    PROPERTY_DEFINE(QString, name)
    PROPERTY_DEFINE(QList<QString>, road_types)
    PROPERTY_DEFINE(QList<QString>, bus_types)
    PROPERTY_DEFINE(QList<QString>, other_types)
    PROPERTY_DEFINE(int, status)
    PROPERTY_DEFINE(int, flag)
    PROPERTY_DEFINE(QString, created_at)
    PROPERTY_DEFINE(QString, updated_at)
    PROPERTY_DEFINE(CaseCountJson, case_count)
    PROPERTY_DEFINE(int, isbase)
    PROPERTY_DEFINE(QList<MenuJson>, list)
};
Q_DECLARE_METATYPE(CaseJson);

/**
 * @brief `/api/case/listAll` 返回的非分页方案列表
 */
class CaseListJson : public QJsonReader {
public:
    CaseListJson() = default;
    CaseListJson(const CaseListJson& other) = default;
    explicit CaseListJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit CaseListJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE2(QList<CaseJson>, data)
};
Q_DECLARE_METATYPE(CaseListJson);

/**
 * @brief 方案详情
 */
class CaseDetailJson : public QJsonReader {
public:
    CaseDetailJson() = default;
    CaseDetailJson(const CaseDetailJson& other) = default;
    explicit CaseDetailJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit CaseDetailJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(int, project_id)
    PROPERTY_DEFINE(int, user_id)
    PROPERTY_DEFINE(int, case_type_id)
    PROPERTY_DEFINE(QString, name)
    PROPERTY_DEFINE(QList<QString>, road_types)
    PROPERTY_DEFINE(QList<QString>, bus_types)
    PROPERTY_DEFINE(QList<QString>, other_types)
    PROPERTY_DEFINE(int, status)
    PROPERTY_DEFINE(int, flag)
    PROPERTY_DEFINE(int, motor_alg1)
    PROPERTY_DEFINE(int, motor_alg2)
    PROPERTY_DEFINE(int, slow_alg1)
    PROPERTY_DEFINE(int, slow_alg2)
    PROPERTY_DEFINE(int, bus_alg1)
    PROPERTY_DEFINE(int, bus_alg2)
    PROPERTY_DEFINE(QString, created_at)
    PROPERTY_DEFINE(QString, updated_at)
    PROPERTY_DEFINE(DBConfigJson, db_conn_str)
    PROPERTY_DEFINE(AlgorithmParamsJson, algorithm_params)
    PROPERTY_DEFINE(QList<MenuJson>, menu)
    PROPERTY_DEFINE(CaseCountJson, case_count)
    PROPERTY_DEFINE(int, isbase)
    PROPERTY_DEFINE(QList<MenuJson>, list)
};
Q_DECLARE_METATYPE(CaseDetailJson);

/**
 * @brief 下载类接口统一返回
 */
class DownLoadCaseJson : public QJsonReader {
public:
    DownLoadCaseJson() = default;
    DownLoadCaseJson(const DownLoadCaseJson& other) = default;
    explicit DownLoadCaseJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit DownLoadCaseJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, fileName)
    PROPERTY_DEFINE(QString, downloadUrl)
    PROPERTY_DEFINE(QString, bucket)
    PROPERTY_DEFINE(QString, file_name)
    PROPERTY_DEFINE(QString, download_url)
    PROPERTY_DEFINE(QString, content)

    QString resolvedFileName() const
    {
        const QString camel = j_fileName().trimmed();
        return camel.isEmpty() ? j_file_name().trimmed() : camel;
    }

    QString resolvedDownloadUrl() const
    {
        const QString camel = j_downloadUrl().trimmed();
        return camel.isEmpty() ? j_download_url().trimmed() : camel;
    }
};
Q_DECLARE_METATYPE(DownLoadCaseJson);

/**
 * @brief 分析报告查看接口返回
 */
class CaseReportViewJson : public QJsonReader {
public:
    CaseReportViewJson() = default;
    CaseReportViewJson(const CaseReportViewJson& other) = default;
    explicit CaseReportViewJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit CaseReportViewJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, content)
};
Q_DECLARE_METATYPE(CaseReportViewJson);

/**
 * @brief 项目表导出结果
 */
class ProjectExportResult : public QJsonReader {
public:
    ProjectExportResult() = default;
    ProjectExportResult(const ProjectExportResult& other) = default;
    explicit ProjectExportResult(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit ProjectExportResult(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(QString, fileName)
    PROPERTY_DEFINE(QString, downloadUrl)
    PROPERTY_DEFINE(QString, bucket)
    PROPERTY_DEFINE(int, fileSize)
    PROPERTY_DEFINE(int, recordCount)
    PROPERTY_DEFINE(QString, exportType)
    PROPERTY_DEFINE(QString, tableName)
};
Q_DECLARE_METATYPE(ProjectExportResult);

/**
 * @brief 方案结果更新接口返回
 * @details 当前文档仅明确为成功/失败包装，因此不再展开业务字段
 */
class CaseEditResultJson : public QJsonReader {
public:
    CaseEditResultJson() = default;
    CaseEditResultJson(const CaseEditResultJson& other) = default;
    explicit CaseEditResultJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit CaseEditResultJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }
};
Q_DECLARE_METATYPE(CaseEditResultJson);

/**
 * @brief 生成质心接口返回
 * @details 当前文档仅给出成功包装
 */
class UpdateCentroidJson : public QJsonReader {
public:
    UpdateCentroidJson() = default;
    UpdateCentroidJson(const UpdateCentroidJson& other) = default;
    explicit UpdateCentroidJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit UpdateCentroidJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }
};
Q_DECLARE_METATYPE(UpdateCentroidJson);

/**
 * @brief 道路溯源单条路径结果
 */
class TraceResultItemJson : public QJsonReader {
public:
    TraceResultItemJson() = default;
    TraceResultItemJson(const TraceResultItemJson& other) = default;
    explicit TraceResultItemJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit TraceResultItemJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, origin_id)
    PROPERTY_DEFINE(int, dest_id)
    PROPERTY_DEFINE(qint64, path_id)
    PROPERTY_DEFINE(QString, path_flow)
    PROPERTY_DEFINE(QString, path_cost)
    PROPERTY_DEFINE(QString, path_links)
};
Q_DECLARE_METATYPE(TraceResultItemJson);

/**
 * @brief 道路溯源结果列表
 */
class TraceListJson : public QJsonReader {
public:
    TraceListJson() = default;
    TraceListJson(const TraceListJson& other) = default;
    explicit TraceListJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit TraceListJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE2(QList<TraceResultItemJson>, data)
};
Q_DECLARE_METATYPE(TraceListJson);

/**
 * @brief 文章列表项
 */
class NewsItemJson : public QJsonReader {
public:
    NewsItemJson() = default;
    NewsItemJson(const NewsItemJson& other) = default;
    explicit NewsItemJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit NewsItemJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(int, bid)
    PROPERTY_DEFINE(QString, title)
    PROPERTY_DEFINE(QString, desc)
    PROPERTY_DEFINE(QString, content)
    PROPERTY_DEFINE(QString, cover)
    PROPERTY_DEFINE(QString, created_at)
    PROPERTY_DEFINE(QString, updated_at)
};
Q_DECLARE_METATYPE(NewsItemJson);

/**
 * @brief 文章列表返回
 */
class NewsListJson : public QJsonReader {
public:
    NewsListJson() = default;
    NewsListJson(const NewsListJson& other) = default;
    explicit NewsListJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit NewsListJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE2(QList<NewsItemJson>, data)
};
Q_DECLARE_METATYPE(NewsListJson);

/**
 * @brief 文章详情返回
 */
class NewsDetailJson : public QJsonReader {
public:
    NewsDetailJson() = default;
    NewsDetailJson(const NewsDetailJson& other) = default;
    explicit NewsDetailJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit NewsDetailJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, id)
    PROPERTY_DEFINE(int, bid)
    PROPERTY_DEFINE(QString, title)
    PROPERTY_DEFINE(QString, desc)
    PROPERTY_DEFINE(QString, content)
    PROPERTY_DEFINE(QString, cover)
    PROPERTY_DEFINE(QString, created_at)
    PROPERTY_DEFINE(QString, updated_at)
};
Q_DECLARE_METATYPE(NewsDetailJson);

/**
 * @brief 算法检查接口结果
 * @details 文档未给稳定字段，这里保留原始对象供上层按需解析
 */
class GrpcCheckResultJson : public QJsonReader {
public:
    GrpcCheckResultJson() = default;
    GrpcCheckResultJson(const GrpcCheckResultJson& other) = default;
    explicit GrpcCheckResultJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit GrpcCheckResultJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE2(QJsonObject, data)
};
Q_DECLARE_METATYPE(GrpcCheckResultJson);

/**
 * @brief 算法运行接口结果
 * @details 文档未给稳定字段，这里保留原始对象供上层按需解析
 */
class GrpcRunAlgResultJson : public QJsonReader {
public:
    GrpcRunAlgResultJson() = default;
    GrpcRunAlgResultJson(const GrpcRunAlgResultJson& other) = default;
    explicit GrpcRunAlgResultJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit GrpcRunAlgResultJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE2(QJsonObject, data)
    QJsonValue j_payload() const
    {
        return m_jsonValue;
    }
};
Q_DECLARE_METATYPE(GrpcRunAlgResultJson);

/**
 * @brief `/api/grpc/aiapi` 同步 GET 返回的 data
 */
class GrpcAiApiResultJson : public QJsonReader {
public:
    GrpcAiApiResultJson() = default;
    GrpcAiApiResultJson(const GrpcAiApiResultJson& other) = default;
    explicit GrpcAiApiResultJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit GrpcAiApiResultJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, code)
    PROPERTY_DEFINE(QString, message)
    PROPERTY_DEFINE(QString, content)
    PROPERTY_DEFINE(QString, download_url)
    PROPERTY_DEFINE2(QJsonValue, data)
    PROPERTY_DEFINE2(QJsonValue, result)
};
Q_DECLARE_METATYPE(GrpcAiApiResultJson);

/**
 * @brief `/api/project/select_data_all` 返回的 data（结构由服务端表数据决定，整体保留为可访问的 JSON）
 */
class SelectDataJson : public QJsonReader {
public:
    SelectDataJson() = default;
    SelectDataJson(const SelectDataJson& other) = default;
    explicit SelectDataJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit SelectDataJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    QJsonValue j_payload() const
    {
        return m_jsonValue;
    }

    QJsonObject j_rawObject() const
    {
        return m_jsonValue.toObject();
    }
};
Q_DECLARE_METATYPE(SelectDataJson);

/**
 * @brief `/api/project/select_data_page` 返回的分页 data
 */
class SelectDataPageJson : public QJsonReader {
public:
    SelectDataPageJson() = default;
    SelectDataPageJson(const SelectDataPageJson& other) = default;
    explicit SelectDataPageJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit SelectDataPageJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, total)
    PROPERTY_DEFINE(int, page)
    PROPERTY_DEFINE(int, limit)
    PROPERTY_DEFINE(int, totalPages)
    PROPERTY_DEFINE(QList<QJsonObject>, data)
};
Q_DECLARE_METATYPE(SelectDataPageJson);

/**
 * @brief gRPC 流式 SSE 内层进度 data（对应事件里 `data` 嵌套对象）
 */
class GrpcStreamProgressDataJson : public QJsonReader {
public:
    GrpcStreamProgressDataJson() = default;
    GrpcStreamProgressDataJson(const GrpcStreamProgressDataJson& other) = default;
    explicit GrpcStreamProgressDataJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit GrpcStreamProgressDataJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, percent)
    PROPERTY_DEFINE(int, done)
    PROPERTY_DEFINE(QString, title)
};
Q_DECLARE_METATYPE(GrpcStreamProgressDataJson);

/**
 * @brief gRPC 流式 SSE 单条业务事件（对应解包后 `data` 对象：code / message / data / result）
 */
class GrpcStreamEventJson : public QJsonReader {
public:
    GrpcStreamEventJson() = default;
    GrpcStreamEventJson(const GrpcStreamEventJson& other) = default;
    explicit GrpcStreamEventJson(const QJsonValue& json)
        : QJsonReader(json)
    {
    }

    explicit GrpcStreamEventJson(const QByteArray& jsonData)
        : QJsonReader(jsonData)
    {
    }

public:
    PROPERTY_DEFINE(int, code)
    PROPERTY_DEFINE(QString, message)
    PROPERTY_DEFINE(GrpcStreamProgressDataJson, data)
    QJsonValue j_result() const
    {
        return m_jsonValue.toObject().value("result");
    }
};
Q_DECLARE_METATYPE(GrpcStreamEventJson);

#endif // HTTPAPIJSON_H
