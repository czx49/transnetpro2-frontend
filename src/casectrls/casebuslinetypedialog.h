/*****************************************************************//**
 * \file   casebuslinetypedialog.h
 * \brief  公交路段新增保存时选择线路类型（busType）
 *
 * \author zhangmg
 * \date   August 2026
 *********************************************************************/
#ifndef CASEBUSLINETYPEDIALOG_H
#define CASEBUSLINETYPEDIALOG_H

#include "framelessdialog.h"

class QComboBox;
class QPushButton;

class CaseBusLineTypeDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit CaseBusLineTypeDialog(QWidget* parent = nullptr);
    ~CaseBusLineTypeDialog() override;

    /** @brief 弹出选择框；确认返回 true 并写出英文 busType（new/expand/reroute） */
    static bool execSelect(QString* busTypeOut, QWidget* parent = nullptr);

    QString selectedBusType() const;

private slots:
    void onConfirmClicked();

private:
    void initUi();

private:
    QComboBox* m_typeCombo;
    QPushButton* m_confirmButton;
};

#endif // CASEBUSLINETYPEDIALOG_H
