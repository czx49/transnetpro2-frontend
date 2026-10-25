/*****************************************************************//**
 * \file   caseodtypedialog.h
 * \brief  OD数据生成前选择 OD 类型（motor/slow）
 *
 * \author zhangmg
 * \date   October 2026
 *********************************************************************/
#ifndef CASEODTYPEDIALOG_H
#define CASEODTYPEDIALOG_H

#include "framelessdialog.h"

class QRadioButton;

/** @brief OD 数据生成类型选择弹窗 */
class CaseOdTypeDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit CaseOdTypeDialog(QWidget* parent = nullptr);
    ~CaseOdTypeDialog() override;

    /** @brief 弹出选择框；确认返回 true 并写出英文 odMode（motor/slow） */
    static bool execSelect(QString* odModeOut, QWidget* parent = nullptr);

    QString selectedOdMode() const;

private:
    void initUi();

    QRadioButton* m_motorRadio = nullptr;
    QRadioButton* m_slowRadio = nullptr;
};

#endif // CASEODTYPEDIALOG_H