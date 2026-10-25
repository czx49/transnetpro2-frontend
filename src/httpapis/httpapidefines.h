#ifndef HTTPAPIDEFINES_H
#define HTTPAPIDEFINES_H

#include "httpserverconfig.h"
#include <QString>

// 统一通过服务端配置拼接完整 URL，避免在调用侧散落主机地址。
#define HTTP_API_HOST (HttpServerConfig::instance().hostName())

// 基础配置
#define API_CONFIG_BASE "/api/config/base"
#define API_CONFIG_AICONFIG "/api/config/aiconfig"
#define API_CONFIG_QUESTION "/api/config/question"
#define API_CONFIG_REGIONS "/api/config/regions"
#define API_CONFIG_FIELD_CHINESE "/api/config/field_chinese"

// 产品版本
#define API_VERSION_GET "/api/config/getversion"

// 用户相关
#define API_LOGIN "/api/user/login"
#define API_USER_CHANGE_PASSWORD "/api/user/change-password"

// 项目相关
#define API_PROJECT_PAGE "/api/project/page"
#define API_PROJECT_ADD "/api/project/add"
#define API_PROJECT_DETAIL "/api/project/detail"
#define API_PROJECT_EDIT "/api/project/edit"
#define API_PROJECT_DEL "/api/project/del"
#define API_PROJECT_EXPORT "/api/project/export"
#define API_PROJECT_CLRTABLE "/api/project/clrtable"
#define API_PROJECT_EXPORTCASE "/api/project/exportcase"
#define API_PROJECT_UPDATE_CENTROID "/api/project/update_centroid"
#define API_PROJECT_CLIP "/api/project/clip"
#define API_PROJECT_CLIP_PART "/api/project/clippart"
#define API_PROJECT_CLIP_DEL "/api/project/clipdel"
#define API_PROJECT_SELECT_DATA_ALL "/api/project/select_data_all"
#define API_PROJECT_SELECT_DATA_PAGE "/api/project/select_data_page"
#define API_PROJECT_BASE_ANALYSIS "/api/project/base_analysis"
#define API_PROJECT_POPDATA "/api/project/popdata"
#define API_PROJECT_GEN_COMMUNITY "/api/project/gen_community"
#define API_PROJECT_CHECK_OBS "/api/project/check_obs"

// 文件导入
#define API_FILES_DRCSV "/api/files/drcsv"
#define API_FILES_DRSHPZIP "/api/files/drshpzip"

// 方案相关
#define API_CASE_LIST_ALL "/api/case/listAll"
#define API_CASE_LIST "/api/case/list"
#define API_CASE_DETAIL "/api/case/detail"
#define API_CASE_ADD "/api/case/add"
#define API_CASE_EDIT "/api/case/edit"
#define API_CASE_EDIT_NAME "/api/case/edit-name"
#define API_CASE_EDIT_STATUS "/api/case/edit-status"
#define API_CASE_DEL "/api/case/del"
#define API_CASE_EDIT_RESULT "/api/case/edit-result"
#define API_CASE_FIELD_BATCH "/api/case/fieldbatch"
#define API_CASE_CREATE_EXPECT_LINE "/api/case/create_expect_line"
#define API_CASE_GET_TRACE "/api/case/get_trace"
#define API_CASE_EXPORT_SIMULATE "/api/case/export_simulate"
#define API_CASE_COPY "/api/case/copy"
#define API_CASE_EXPORT_OSM "/api/case/export_osm"
#define API_CASE_REPORT_VIEW "/api/case/report_view"
#define API_CASE_REPORT_DOWNLOAD "/api/case/report_download"
#define API_CASE_BASEDATA_VIEW "/api/case/basedata_view"
#define API_CASE_COST_VIEW "/api/case/cost_view"
#define API_CASE_CASECOMPARISON_VIEW "/api/case/casecomparison_view"
#define API_CASE_PTCASE_EXPORT "/api/case/ptcase_export"
#define API_CASE_COPY_PROJECT "/api/case/copyproject"
#define API_CASE_EDITLOG "/api/case/editlog"

// 文章相关
#define API_NEWS "/api/news"

// 算法相关
#define API_GRPC_CHECK "/api/grpc/check"
#define API_GRPC_RUNALG "/api/grpc/runalg"
#define API_GRPC_STREAM "/api/grpc/stream"
#define API_GRPC_AISTREAM "/api/grpc/aistream"
#define API_GRPC_AIAPI "/api/grpc/aiapi"
#define API_GRPC_CASE_EXPORT "/api/grpc/case_export"

// WFS（公开，响应多为 GeoJSON / 自定义 JSON，非统一 code 包装）
#define API_WFS "/wfs"
#define API_WFS_CAPABILITIES "/wfs/capabilities"
#define API_WFS_FEATURE "/wfs/feature"

/**
 * @brief 将接口相对路径拼接为完整请求地址
 */
inline QString fullURL(const QString& path)
{
    return QString("%1%2").arg(HTTP_API_HOST).arg(path.startsWith('/') ? path : QString("/%1").arg(path));
}

#endif // HTTPAPIDEFINES_H
