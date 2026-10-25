/*****************************************************************//**
 * \file   renamecasedialog.cpp
 * \brief  重命名方案对话框实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "renamecasedialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include "../publicctrls/messagedialog.h"
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

RenameCaseDialog::RenameCaseDialog(QWidget* parent)
    : FramelessDialog(parent)
    , m_nameEdit(nullptr)
    , m_confirmButton(nullptr)
{
    setModal(true);
    setFixedSize(400, 230);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);

    initUi();
}

RenameCaseDialog::~RenameCaseDialog()
{
}

void RenameCaseDialog::setCaseName(const QString& name)
{
    m_caseName = name.trimmed();
    if (m_nameEdit) {
        m_nameEdit->setText(m_caseName);
        if (m_confirmButton) {
            m_confirmButton->setEnabled(!m_caseName.isEmpty());
        }
    }
}

QString RenameCaseDialog::caseName() const
{
    return m_caseName;
}

void RenameCaseDialog::reset()
{
    m_caseName.clear();
    if (m_nameEdit) {
        m_nameEdit->clear();
    }
    if (m_confirmButton) {
        m_confirmButton->setEnabled(false);
    }
}

void RenameCaseDialog::showEvent(QShowEvent* event)
{
    FramelessDialog::showEvent(event);
    if (m_nameEdit) {
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
    }
}

void RenameCaseDialog::onNameTextChanged(const QString& text)
{
    if (m_confirmButton) {
        m_confirmButton->setEnabled(!text.trimmed().isEmpty());
    }
}

void RenameCaseDialog::onConfirmClicked()
{
    if (!validateInput()) {
        return;
    }

    m_caseName = m_nameEdit->text().trimmed();
    accept();
}

void RenameCaseDialog::initUi()
{
    setTitleText(u8"重命名方案");
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + Phase2Ui::fieldStyleSheet(Phase2Ui::InputFieldTheme::LightDialogRounded));

    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(14);

    QLabel* nameLabel = new QLabel(u8"*方案名称", this);
    nameLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(u8"请输入方案名称...");
    connect(m_nameEdit, &QLineEdit::textChanged, this, &RenameCaseDialog::onNameTextChanged);
    mainLayout->addWidget(m_nameEdit);

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
    m_confirmButton->setEnabled(false);
    m_confirmButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(m_confirmButton, &QPushButton::clicked, this, &RenameCaseDialog::onConfirmClicked);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(m_confirmButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}

bool RenameCaseDialog::validateInput() const
{
    if (!m_nameEdit || m_nameEdit->text().trimmed().isEmpty()) {
        TnUi::showWarning(u8"提示", u8"请输入方案名称。");
        return false;
    }

    return true;
}
