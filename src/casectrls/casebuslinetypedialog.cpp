/*****************************************************************//**
 * \file   casebuslinetypedialog.cpp
 * \brief  公交路段新增保存时选择线路类型（busType）
 *
 * \author zhangmg
 * \date   August 2026
 *********************************************************************/
#include "casebuslinetypedialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

CaseBusLineTypeDialog::CaseBusLineTypeDialog(QWidget* parent)
    : FramelessDialog(parent)
    , m_typeCombo(nullptr)
    , m_confirmButton(nullptr)
{
    setModal(true);
    setFixedSize(400, 230);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);

    initUi();
}

CaseBusLineTypeDialog::~CaseBusLineTypeDialog()
{
}

bool CaseBusLineTypeDialog::execSelect(QString* busTypeOut, QWidget* parent)
{
    CaseBusLineTypeDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }
    if (busTypeOut) {
        *busTypeOut = dlg.selectedBusType();
    }
    return !dlg.selectedBusType().isEmpty();
}

QString CaseBusLineTypeDialog::selectedBusType() const
{
    if (!m_typeCombo) {
        return {};
    }
    return m_typeCombo->currentData().toString().trimmed();
}

void CaseBusLineTypeDialog::onConfirmClicked()
{
    if (selectedBusType().isEmpty()) {
        return;
    }
    accept();
}

void CaseBusLineTypeDialog::initUi()
{
    setTitleText(u8"新增公交线路类型");
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + Phase2Ui::fieldStyleSheet(Phase2Ui::InputFieldTheme::LightDialogRounded));

    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(14);

    QLabel* typeLabel = new QLabel(u8"*线路类型", this);
    typeLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(typeLabel);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(u8"新建", QStringLiteral("new"));
    m_typeCombo->addItem(u8"扩建", QStringLiteral("expand"));
    m_typeCombo->addItem(u8"改线", QStringLiteral("reroute"));
    m_typeCombo->setCurrentIndex(0);
    Phase2Ui::setupThemedCombo(m_typeCombo, Phase2Ui::InputFieldTheme::LightDialogRounded);
    mainLayout->addWidget(m_typeCombo);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->setSpacing(16);
    buttonLayout->addStretch();

    QPushButton* cancelButton = new QPushButton(u8"取消", this);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setFixedSize(82, 32);
    cancelButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(cancelButton, &QPushButton::clicked, this, &FramelessDialog::reject);

    m_confirmButton = new QPushButton(u8"确定", this);
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_confirmButton->setFixedSize(82, 32);
    m_confirmButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(m_confirmButton, &QPushButton::clicked, this, &CaseBusLineTypeDialog::onConfirmClicked);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(m_confirmButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}
