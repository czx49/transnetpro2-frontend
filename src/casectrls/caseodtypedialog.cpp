/*****************************************************************//**
 * \file   caseodtypedialog.cpp
 * \brief  OD数据生成前选择 OD 类型（motor/slow）
 *
 * \author zhangmg
 * \date   October 2026
 *********************************************************************/
#include "caseodtypedialog.h"

#include "../publicctrls/phase2dialogstyle.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

CaseOdTypeDialog::CaseOdTypeDialog(QWidget* parent)
    : FramelessDialog(parent)
{
    setModal(true);
    setFixedSize(400, 210);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);

    initUi();
}

CaseOdTypeDialog::~CaseOdTypeDialog()
{
}

bool CaseOdTypeDialog::execSelect(QString* odModeOut, QWidget* parent)
{
    CaseOdTypeDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }
    if (odModeOut) {
        *odModeOut = dlg.selectedOdMode();
    }
    return !dlg.selectedOdMode().isEmpty();
}

QString CaseOdTypeDialog::selectedOdMode() const
{
    if (m_slowRadio && m_slowRadio->isChecked()) {
        return QStringLiteral("slow");
    }
    return QStringLiteral("motor");
}

void CaseOdTypeDialog::initUi()
{
    setTitleText(u8"OD数据生成");
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());

    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(14);

    QLabel* typeLabel = new QLabel(u8"OD类型", this);
    typeLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(typeLabel);

    m_motorRadio = new QRadioButton(u8"机动车", this);
    m_slowRadio = new QRadioButton(u8"慢行", this);
    m_motorRadio->setChecked(true);

    QVBoxLayout* radioLayout = new QVBoxLayout();
    radioLayout->setContentsMargins(0, 0, 0, 0);
    radioLayout->setSpacing(8);
    radioLayout->addWidget(m_motorRadio);
    radioLayout->addWidget(m_slowRadio);
    mainLayout->addLayout(radioLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->setSpacing(16);
    buttonLayout->addStretch();

    QPushButton* cancelButton = new QPushButton(u8"取消", this);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setFixedSize(82, 32);
    cancelButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(cancelButton, &QPushButton::clicked, this, &FramelessDialog::reject);

    QPushButton* confirmButton = new QPushButton(u8"确定", this);
    confirmButton->setCursor(Qt::PointingHandCursor);
    confirmButton->setFixedSize(82, 32);
    confirmButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(confirmButton, &QPushButton::clicked, this, &FramelessDialog::accept);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(confirmButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}
