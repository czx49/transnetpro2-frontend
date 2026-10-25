/*****************************************************************//**
 * \file   loadingdialog.h
 * \brief  加载对话框
 * \details 显示加载进度的对话框
 *
 * \author zhangmg
 * \date   April 2026
 * \History 初始版本
 *********************************************************************/
#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include "framelessdialog.h"
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>

/**
 * \brief  加载对话框
 * \details 显示加载进度
 */
class LoadingDialog : public FramelessDialog {
    Q_OBJECT

public:
    enum class Theme {
        Light = 0,
        MapExport
    };

    explicit LoadingDialog(const QString& message, QWidget* parent = nullptr,
                           Theme theme = Theme::Light);

    void setProgress(qint64 current, qint64 total);

    void setMessage(const QString& message);

    void showNonBlocking();

    void closeDialog();

private:
    void applyTheme(Theme theme);

    QLabel* m_label = nullptr;
    QProgressBar* m_progress = nullptr;
};

#endif // LOADINGDIALOG_H
