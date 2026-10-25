/*****************************************************************//**
 * \file   casecomparisondialog.cpp
 * \brief  方案对比选择弹窗实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casecomparisondialog.h"

#include "../publicctrls/phase2dialogstyle.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include "../publicctrls/messagedialog.h"
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QString dialogStyleSheet()
{
    return Phase2Ui::dialogShellStyleSheet()
        + QStringLiteral(
        "#framelessDialogTitleBar {"
        "   border-bottom: 1px solid rgba(255, 255, 255, 0.12);"
        "}"
        "#compareCaseHint {"
        "   color: #cbd5e1;"
        "   font-size: 13px;"
        "}"
        "#compareCaseListHost {"
        "   background: transparent;"
        "}"
        "QScrollArea {"
        "   border: none;"
        "   background: transparent;"
        "}"
        "QScrollArea > QWidget > QWidget {"
        "   background: transparent;"
        "}"
        "QFrame#compareCaseRow {"
        "   background-color: #ffffff;"
        "   border: 1px solid #c5d9f0;"
        "   border-radius: 10px;"
        "}"
        "#framelessDialogContent QFrame#compareCaseRow QCheckBox {"
        "   background: transparent;"
        "   color: #1f2937;"
        "   font-size: 13px;"
        "   spacing: 8px;"
        "}"
        "#framelessDialogContent QFrame#compareCaseRow QCheckBox::indicator {"
        "   width: 16px;"
        "   height: 16px;"
        "}"
        "#framelessDialogContent QFrame#compareCaseRow QLabel#compareCaseTag {"
        "   color: #ffffff;"
        "   font-size: 12px;"
        "   font-weight: 600;"
        "   border-radius: 10px;"
        "   padding: 2px 8px;"
        "}");
}

QString caseTagStyle(int isBase)
{
    return QStringLiteral(
        "QLabel#compareCaseTag { background-color: %1; }")
        .arg(isBase == 1 ? QStringLiteral("#1890ff") : QStringLiteral("#6b7280"));
}

} // namespace

CaseComparisonDialog::CaseComparisonDialog(const QList<CaseItem>& caseItems, QWidget* parent)
    : FramelessDialog(parent)
{
    setTitleText(QString::fromUtf8(u8"方案对比"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    setModal(true);
    resize(640, 520);
    setStyleSheet(dialogStyleSheet());

    initUi();

    for (const CaseItem& caseItem : caseItems) {
        addCaseRow(caseItem);
    }

    if (m_caseRows.isEmpty()) {
        QLabel* emptyLabel = new QLabel(QString::fromUtf8(u8"当前项目暂无方案。"), m_listHost);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QStringLiteral("QLabel { color: #cbd5e1; font-size: 13px; }"));
        m_listLayout->addWidget(emptyLabel);
    }
    if (m_listLayout) {
        m_listLayout->addStretch(1);
    }
}

CaseComparisonDialog::~CaseComparisonDialog() = default;

QList<int> CaseComparisonDialog::selectedCaseIds() const
{
    QList<int> ids;
    ids.reserve(m_caseRows.size());
    for (const CaseRow& row : m_caseRows) {
        if (row.checkBox && row.checkBox->isChecked()) {
            ids.append(row.caseId);
        }
    }
    return ids;
}

void CaseComparisonDialog::onConfirmClicked()
{
    const QList<int> ids = selectedCaseIds();
    if (ids.size() < 2) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
                             QString::fromUtf8(u8"请至少勾选两个方案进行对比。"));
        return;
    }

    accept();
}

void CaseComparisonDialog::initUi()
{
    QVBoxLayout* root = contentLayout();
    root->setContentsMargins(18, 12, 18, 18);
    root->setSpacing(12);

    QLabel* hintLabel = new QLabel(QString::fromUtf8(u8"勾选两个及以上方案后生成对比报告。"), this);
    hintLabel->setObjectName(QStringLiteral("compareCaseHint"));
    hintLabel->setWordWrap(true);
    root->addWidget(hintLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_listHost = new QWidget(m_scrollArea);
    m_listHost->setObjectName(QStringLiteral("compareCaseListHost"));
    m_listHost->setAttribute(Qt::WA_StyledBackground, true);

    m_listLayout = new QVBoxLayout(m_listHost);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(10);

    m_scrollArea->setWidget(m_listHost);
    root->addWidget(m_scrollArea, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(10);
    buttonRow->addStretch();

    QPushButton* cancelButton = new QPushButton(QString::fromUtf8(u8"取消"), this);
    cancelButton->setObjectName(QStringLiteral("compareCancelButton"));
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(cancelButton, &QPushButton::clicked, this, &FramelessDialog::reject);

    m_confirmButton = new QPushButton(QString::fromUtf8(u8"确定"), this);
    m_confirmButton->setObjectName(QStringLiteral("compareConfirmButton"));
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_confirmButton->setDefault(true);
    m_confirmButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(m_confirmButton, &QPushButton::clicked, this, &CaseComparisonDialog::onConfirmClicked);

    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(m_confirmButton);
    root->addLayout(buttonRow);
}

void CaseComparisonDialog::addCaseRow(const CaseItem& caseItem)
{
    if (!m_listLayout || !m_listHost) {
        return;
    }

    QWidget* rowWidget = new QFrame(m_listHost);
    rowWidget->setObjectName(QStringLiteral("compareCaseRow"));
    rowWidget->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(14, 12, 14, 12);
    rowLayout->setSpacing(12);

    QCheckBox* checkBox = new QCheckBox(caseItem.name, rowWidget);
    checkBox->setCursor(Qt::PointingHandCursor);
    checkBox->setChecked(caseItem.isBase == 1);
    checkBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    rowLayout->addWidget(checkBox, 1);

    QLabel* tagLabel = new QLabel(caseItem.isBase == 1 ? QString::fromUtf8(u8"基础方案")
                                                       : QString::fromUtf8(u8"普通方案"),
                                  rowWidget);
    tagLabel->setObjectName(QStringLiteral("compareCaseTag"));
    tagLabel->setStyleSheet(caseTagStyle(caseItem.isBase));
    rowLayout->addWidget(tagLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    m_listLayout->addWidget(rowWidget);

    CaseRow row;
    row.caseId = caseItem.id;
    row.checkBox = checkBox;
    m_caseRows.append(row);
}
