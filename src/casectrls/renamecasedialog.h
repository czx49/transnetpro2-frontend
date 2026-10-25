/*****************************************************************//**
 * \file   renamecasedialog.h
 * \brief  重命名方案对话框
 * \details 对齐新建项目弹窗样式的轻量级重命名弹窗
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef RENAMECASEDIALOG_H
#define RENAMECASEDIALOG_H

#include "framelessdialog.h"

class QLineEdit;
class QPushButton;

class RenameCaseDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit RenameCaseDialog(QWidget* parent = nullptr);
    ~RenameCaseDialog() override;

    void setCaseName(const QString& name);
    QString caseName() const;
    void reset();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onNameTextChanged(const QString& text);
    void onConfirmClicked();

private:
    void initUi();
    bool validateInput() const;

private:
    QLineEdit* m_nameEdit;
    QPushButton* m_confirmButton;
    QString m_caseName;
};

#endif // RENAMECASEDIALOG_H
