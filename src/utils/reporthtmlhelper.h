/*****************************************************************//**
 * \file   reporthtmlhelper.h
 * \brief  从接口 JSON 中提取报告 HTML 内容
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef REPORTHTMLHELPER_H
#define REPORTHTMLHELPER_H

#include <QJsonObject>
#include <QString>

namespace ReportHtmlHelper {

QString extract(const QJsonObject& data);

} // namespace ReportHtmlHelper

#endif
