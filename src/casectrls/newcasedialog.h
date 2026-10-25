/*****************************************************************//**
 * \file   newcasedialog.h
 * \brief  新建方案对话框
 * \details 提供方案名称与方案类型输入
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef NEWCASEDIALOG_H
#define NEWCASEDIALOG_H

#include "framelessdialog.h"

class QComboBox;
class QLineEdit;
class QPushButton;

class NewCaseDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit NewCaseDialog(QWidget* parent = nullptr);
    ~NewCaseDialog() override;

    QString caseName() const;
    QString caseTypeName() const;
    int caseTypeId() const;

    void reset();

    /** @brief 新建场景：首个方案锁定名称为「基础方案」，否则名称可编辑 */
    void applyNewCaseMode(bool firstCaseInProject);

    /** @brief 编辑场景：预填方案名称（会同步启用「确定」按钮状态） */
    void setCaseName(const QString& name);
    /** @brief 编辑场景：按 case_type_id 选中下拉项 */
    void setCaseTypeId(int typeId);

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
    QComboBox* m_caseTypeCombo;
    QPushButton* m_confirmButton;
    QString m_caseName;
    QString m_caseTypeName;
    int m_caseTypeId;
};

#endif // NEWCASEDIALOG_H
