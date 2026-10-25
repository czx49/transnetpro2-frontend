/*****************************************************************//**
 * \file   loadingdialog.cpp
 * \brief  加载对话框
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "loadingdialog.h"

#include "../publicctrls/phase2dialogstyle.h"

LoadingDialog::LoadingDialog(const QString& message, QWidget* parent, Theme theme)
    : FramelessDialog(parent)
{
    setTitleText(QString::fromUtf8(u8"加载中"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);

    auto* layout = contentLayout();
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(12);

    m_label = new QLabel(message, this);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(10);

    layout->addWidget(m_label);
    layout->addWidget(m_progress);

    applyTheme(theme);
}

void LoadingDialog::applyTheme(Theme theme)
{
    if (theme == Theme::MapExport) {
        setFixedSize(420, 132);
        setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                      + QStringLiteral(
                          "#framelessDialogContent QLabel {"
                          "   color: #cbd5e1;"
                          "   font-size: 13px;"
                          "}"
                          "QProgressBar {"
                          "   border: none;"
                          "   border-radius: 5px;"
                          "   background-color: #3d4f66;"
                          "}"
                          "QProgressBar::chunk {"
                          "   border-radius: 5px;"
                          "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
                          "       stop:0 #22c55e, stop:0.5 #4ade80, stop:1 #22c55e);"
                          "}"));
        return;
    }

    setFixedSize(300, 124);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + Phase2Ui::dialogProgressBarStyleSheet()
                  + QStringLiteral(
                      "#framelessDialogContent QLabel {"
                      "   color: #ffffff;"
                      "   font-size: 13px;"
                      "}"));
}

void LoadingDialog::setProgress(qint64 current, qint64 total)
{
    if (!m_progress) {
        return;
    }
    if (total > 0) {
        m_progress->setRange(0, 100);
        m_progress->setValue(static_cast<int>(current * 100 / total));
    }
}

void LoadingDialog::setMessage(const QString& message)
{
    if (m_label) {
        m_label->setText(message);
    }
}

void LoadingDialog::showNonBlocking()
{
    show();
}

void LoadingDialog::closeDialog()
{
    close();
}
