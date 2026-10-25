/*****************************************************************//**
 * \file   casecomparisondialog.h
 * \brief  方案对比选择弹窗
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASECOMPARISONDIALOG_H
#define CASECOMPARISONDIALOG_H

#include "../publicctrls/framelessdialog.h"
#include "caseitem.h"

#include <QList>

class QCheckBox;
class QScrollArea;
class QPushButton;
class QVBoxLayout;

class CaseComparisonDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit CaseComparisonDialog(const QList<CaseItem>& caseItems, QWidget* parent = nullptr);
    ~CaseComparisonDialog() override;

    QList<int> selectedCaseIds() const;

private slots:
    void onConfirmClicked();

private:
    void initUi();
    void addCaseRow(const CaseItem& caseItem);

private:
    struct CaseRow {
        int caseId = -1;
        QCheckBox* checkBox = nullptr;
    };

    QList<CaseRow> m_caseRows;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_listHost = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    QPushButton* m_confirmButton = nullptr;
};

#endif // CASECOMPARISONDIALOG_H
