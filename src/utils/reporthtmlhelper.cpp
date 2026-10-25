/*****************************************************************//**
 * \file   reporthtmlhelper.cpp
 * \brief  从接口 JSON 中提取报告 HTML 内容
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "reporthtmlhelper.h"

namespace ReportHtmlHelper {

namespace {

QString pickContent(const QJsonObject& obj)
{
    const QStringList keys = {QStringLiteral("content"), QStringLiteral("html"), QStringLiteral("report")};
    for (const QString& key : keys) {
        const QString value = obj.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
}

} // namespace

QString extract(const QJsonObject& data)
{
    QString html = pickContent(data);
    if (html.isEmpty()) {
        const QJsonValue nested = data.value(QStringLiteral("data"));
        if (nested.isObject()) {
            html = pickContent(nested.toObject());
        }
    }
    return html;
}

} // namespace ReportHtmlHelper
