/*****************************************************************//**
 * \file   reportviewdialog.h
 * \brief  机动车分析报告 HTML 查看弹窗
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef REPORTVIEWDIALOG_H
#define REPORTVIEWDIALOG_H

#include "framelessdialog.h"

#include <QString>

class ReportViewDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit ReportViewDialog(QWidget* parent = nullptr);

    void setHtmlContent(const QString& html);

    static void showModal(const QString& html,
                          const QString& title = QString::fromUtf8(u8"机动车分析报告"));
};

#endif // REPORTVIEWDIALOG_H
