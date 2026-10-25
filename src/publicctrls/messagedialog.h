/*****************************************************************//**
 * \file   messagedialog.h
 * \brief  轻量消息 / 确认弹窗（FramelessDialog 深色套件）
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include <QString>

namespace TnUi {

void showInformation(const QString& title, const QString& message);
void showWarning(const QString& title, const QString& message);
void showCritical(const QString& title, const QString& message);

/** @return 用户点击「是」为 true */
bool confirmYesNo(const QString& title, const QString& message);

} // namespace TnUi

#endif // MESSAGEDIALOG_H
