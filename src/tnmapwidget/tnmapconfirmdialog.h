/*****************************************************************//**
 * \file   tnmapconfirmdialog.h
 * \brief  地图模块无边框确认框（parent 必须为 nullptr，避免继承错误样式）
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef TNMAPCONFIRMDIALOG_H
#define TNMAPCONFIRMDIALOG_H

#include <QString>

namespace TnMapUi {

/** @return 用户点击「是」为 true */
bool confirmYesNo(const QString& title, const QString& message);

} // namespace TnMapUi

#endif // TNMAPCONFIRMDIALOG_H
