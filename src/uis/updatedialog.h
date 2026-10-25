/*****************************************************************//**
 * \file   updatedialog.h
 * \brief  产品升级对话框
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include "framelessdialog.h"

class QWidget;

namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    ~UpdateDialog() override;

private slots:
    void slotBtn_update_clicked();
    void slotBtn_ignore_clicked();

private:
    void setUpdateInfo();
    void setUpdating(bool updating);
    QString formatSize(qint64 bytes) const;
    QString getFileMd5(const QString& filePath) const;
    bool checkFileMd5(const QString& filePath, const QString& targetMd5) const;

private:
    QWidget* m_contentWidget;
    Ui::UpdateDialog* ui;
};

#endif // UPDATEDIALOG_H
