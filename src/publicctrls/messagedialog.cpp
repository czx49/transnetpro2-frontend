/*****************************************************************//**
 * \file   messagedialog.cpp
 * \brief  轻量消息 / 确认弹窗实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "messagedialog.h"

#include "framelessdialog.h"
#include "phase2dialogstyle.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>

namespace TnUi {

namespace {

enum class MessageKind {
    Information,
    Warning,
    Critical
};

void showMessageDialog(const QString& title, const QString& message, MessageKind kind)
{
    FramelessDialog dialog(nullptr);
    dialog.setTitleText(title);
    dialog.setMinimizeButtonVisible(false);
    dialog.setMaximizeButtonVisible(false);
    dialog.resize(460, 180);
    dialog.setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                         + QStringLiteral(
                             "#framelessDialogContent { background: transparent; }"
                             "#framelessDialogContent QWidget { background: transparent; }"));

    QVBoxLayout* layout = dialog.contentLayout();
    layout->setContentsMargins(22, 16, 22, 18);
    layout->setSpacing(16);

    QWidget* body = new QWidget(&dialog);
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(QStringLiteral("QWidget { background-color: transparent; }"));
    QHBoxLayout* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(14);

    QString iconText = QStringLiteral("i");
    QString iconBg = QStringLiteral("#1890ff");
    if (kind == MessageKind::Warning) {
        iconText = QStringLiteral("!");
        iconBg = QStringLiteral("#f59e0b");
    } else if (kind == MessageKind::Critical) {
        iconText = QStringLiteral("!");
        iconBg = QStringLiteral("#ef4444");
    }

    QLabel* iconLabel = new QLabel(iconText, body);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(40, 40);
    iconLabel->setStyleSheet(QStringLiteral(
                                 "QLabel {"
                                 "   border-radius: 20px;"
                                 "   background-color: %1;"
                                 "   color: white;"
                                 "   font-size: 24px;"
                                 "   font-weight: 700;"
                                 "}")
                                 .arg(iconBg));
    bodyLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    QLabel* textLabel = new QLabel(message, body);
    textLabel->setWordWrap(true);
    textLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "   background-color: transparent;"
        "   color: #ffffff;"
        "   font-size: 14px;"
        "   line-height: 1.5;"
        "}"));
    bodyLayout->addWidget(textLabel, 1);
    layout->addWidget(body, 1);

    QPushButton* okButton = new QPushButton(QString::fromUtf8(u8"确定"), &dialog);
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    QObject::connect(okButton, &QPushButton::clicked, &dialog, &FramelessDialog::accept);
    layout->addWidget(okButton, 0, Qt::AlignHCenter);
    dialog.exec();
}

} // namespace

void showInformation(const QString& title, const QString& message)
{
    showMessageDialog(title, message, MessageKind::Information);
}

void showWarning(const QString& title, const QString& message)
{
    showMessageDialog(title, message, MessageKind::Warning);
}

void showCritical(const QString& title, const QString& message)
{
    showMessageDialog(title, message, MessageKind::Critical);
}

bool confirmYesNo(const QString& title, const QString& message)
{
    FramelessDialog dlg(nullptr);
    dlg.setTitleText(title);
    dlg.setMinimizeButtonVisible(false);
    dlg.setMaximizeButtonVisible(false);
    dlg.resize(440, 180);
    dlg.setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                      + QStringLiteral("#framelessDialogContent { background: transparent; }"));

    QVBoxLayout* root = dlg.contentLayout();
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(16);

    auto* row = new QHBoxLayout();
    QLabel* iconLab = new QLabel(&dlg);
    iconLab->setFixedSize(40, 40);
    iconLab->setAlignment(Qt::AlignCenter);
    iconLab->setStyleSheet(QStringLiteral(
        "QLabel { background: #2188ff; color: white; border-radius: 20px; font-size: 22px; font-weight: 700; }"));
    iconLab->setText(QStringLiteral("?"));

    QLabel* msgLab = new QLabel(message, &dlg);
    msgLab->setWordWrap(true);
    msgLab->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffffff; font-size: 14px; background: transparent; }"));
    row->addWidget(iconLab, 0, Qt::AlignTop);
    row->addWidget(msgLab, 1);
    root->addLayout(row);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton* yesBtn = new QPushButton(QString::fromUtf8(u8"是"), &dlg);
    QPushButton* noBtn = new QPushButton(QString::fromUtf8(u8"否"), &dlg);
    yesBtn->setCursor(Qt::PointingHandCursor);
    noBtn->setCursor(Qt::PointingHandCursor);
    yesBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    noBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    QObject::connect(yesBtn, &QPushButton::clicked, &dlg, &FramelessDialog::accept);
    QObject::connect(noBtn, &QPushButton::clicked, &dlg, &FramelessDialog::reject);
    btnRow->addWidget(noBtn);
    btnRow->addWidget(yesBtn);
    root->addLayout(btnRow);

    return dlg.exec() == QDialog::Accepted;
}

} // namespace TnUi
