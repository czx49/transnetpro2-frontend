/*****************************************************************//**
 * \file   newcasedialog.cpp
 * \brief  新建方案对话框实现
 * \details 对齐方案模块新建交互，支持方案名称与类型输入
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "newcasedialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "casetype.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include "../publicctrls/messagedialog.h"
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

NewCaseDialog::NewCaseDialog(QWidget* parent)
    : FramelessDialog(parent)
    , m_nameEdit(nullptr)
    , m_caseTypeCombo(nullptr)
    , m_confirmButton(nullptr)
    , m_caseTypeId(static_cast<int>(CaseTypeId::TrafficPolicy))
{
    setModal(true);
    setFixedSize(420, 300);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);

    initUi();
}

NewCaseDialog::~NewCaseDialog()
{
}

QString NewCaseDialog::caseName() const
{
    return m_caseName;
}

QString NewCaseDialog::caseTypeName() const
{
    return m_caseTypeName;
}

int NewCaseDialog::caseTypeId() const
{
    return m_caseTypeId;
}

void NewCaseDialog::reset()
{
    m_caseName.clear();
    m_caseTypeName.clear();
    m_caseTypeId = static_cast<int>(CaseTypeId::TrafficPolicy);

    if (m_nameEdit) {
        m_nameEdit->clear();
        m_nameEdit->setReadOnly(false);
    }
    if (m_caseTypeCombo && m_caseTypeCombo->count() > 0) {
        m_caseTypeCombo->setCurrentIndex(0);
    }
}

void NewCaseDialog::applyNewCaseMode(bool firstCaseInProject)
{
    if (!m_nameEdit || !m_confirmButton) {
        return;
    }

    if (firstCaseInProject) {
        const QString baseName = QString::fromUtf8(u8"基础方案");
        m_nameEdit->setReadOnly(true);
        m_nameEdit->setText(baseName);
        onNameTextChanged(baseName);
    } else {
        m_nameEdit->setReadOnly(false);
        if (m_nameEdit->text().trimmed().isEmpty()) {
            m_confirmButton->setEnabled(false);
        }
    }
}

void NewCaseDialog::setCaseName(const QString& name)
{
    if (m_nameEdit) {
        m_nameEdit->setText(name);
        onNameTextChanged(name);
    }
}

void NewCaseDialog::setCaseTypeId(int typeId)
{
    if (!m_caseTypeCombo) {
        return;
    }

    m_caseTypeId = typeId;
    for (int i = 0; i < m_caseTypeCombo->count(); ++i) {
        if (m_caseTypeCombo->itemData(i).toInt() == typeId) {
            m_caseTypeCombo->setCurrentIndex(i);
            return;
        }
    }
    m_caseTypeCombo->setCurrentIndex(0);
}

void NewCaseDialog::showEvent(QShowEvent* event)
{
    FramelessDialog::showEvent(event);
    if (m_nameEdit) {
        m_nameEdit->setFocus();
    }
}

void NewCaseDialog::onNameTextChanged(const QString& text)
{
    if (m_confirmButton) {
        m_confirmButton->setEnabled(!text.trimmed().isEmpty());
    }
}

void NewCaseDialog::onConfirmClicked()
{
    if (!validateInput()) {
        return;
    }

    m_caseName = m_nameEdit->text().trimmed();
    m_caseTypeName = m_caseTypeCombo->currentText().trimmed();
    m_caseTypeId = m_caseTypeCombo->currentData().toInt();
    accept();
}

void NewCaseDialog::initUi()
{
    setTitleText(QString::fromUtf8(u8"新建方案"));
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());

    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(14);

    QLabel* nameLabel = new QLabel(QString::fromUtf8(u8"*方案名称"), this);
    nameLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QString::fromUtf8(u8"请输入方案名称"));
    Phase2Ui::applyFieldStyle(m_nameEdit, Phase2Ui::InputFieldTheme::LightDialogRounded);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewCaseDialog::onNameTextChanged);
    mainLayout->addWidget(m_nameEdit);

    QLabel* typeLabel = new QLabel(QString::fromUtf8(u8"*方案类型"), this);
    typeLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(typeLabel);

    m_caseTypeCombo = new QComboBox(this);
    m_caseTypeCombo->setCursor(Qt::PointingHandCursor);
    m_caseTypeCombo->addItem(caseTypeIdDisplayName(CaseTypeId::TrafficPolicy),
                             static_cast<int>(CaseTypeId::TrafficPolicy));
    Phase2Ui::setupThemedCombo(m_caseTypeCombo, Phase2Ui::InputFieldTheme::LightDialogRounded);
    mainLayout->addWidget(m_caseTypeCombo);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 8, 0, 20);
    buttonLayout->setSpacing(16);
    buttonLayout->addStretch();

    QPushButton* cancelButton = new QPushButton(QString::fromUtf8(u8"取消"), this);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setFixedSize(82, 32);
    cancelButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(cancelButton, &QPushButton::clicked, this, &FramelessDialog::reject);

    m_confirmButton = new QPushButton(QString::fromUtf8(u8"确定"), this);
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_confirmButton->setFixedSize(82, 32);
    m_confirmButton->setEnabled(false);
    m_confirmButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(m_confirmButton, &QPushButton::clicked, this, &NewCaseDialog::onConfirmClicked);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(m_confirmButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

bool NewCaseDialog::validateInput() const
{
    if (!m_nameEdit || m_nameEdit->text().trimmed().isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
                          QString::fromUtf8(u8"请输入方案名称。"));
        return false;
    }

    if (!m_caseTypeCombo || m_caseTypeCombo->currentText().trimmed().isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
                          QString::fromUtf8(u8"请选择方案类型。"));
        return false;
    }

    return true;
}
