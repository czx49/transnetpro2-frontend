/*****************************************************************//**
 * \file   reportviewdialog.cpp
 * \brief  机动车分析报告 HTML 查看弹窗实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "reportviewdialog.h"

#include "../publicctrls/phase2dialogstyle.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

const QString& reportEnhanceStyleBlock()
{
    static const QString style = QString::fromUtf8(u8R"(
body { background-color: #ffffff !important; color: #1f2937 !important; font-size: 13px; line-height: 1.5; }
h1, h2, h3, h4, h5, h6 { color: #111827 !important; font-weight: 700; }
p, span, td, th, li, div, label { color: #374151 !important; }
a { color: #1e78df !important; }
table { border-collapse: collapse; }
th, td { border: 1px solid #d7e1ed; padding: 6px 8px; color: #374151 !important; }
thead th, tr:nth-child(even) { background-color: #eef4fd !important; }
)");
    return style;
}

QString wrapReportHtmlForDisplay(const QString& html)
{
    const QString trimmed = html.trimmed();
    if (trimmed.isEmpty()) {
        return QString::fromUtf8(u8"<p>暂无报告内容</p>");
    }

    const QString styleTag = QString::fromUtf8(u8"<style>%1</style>").arg(reportEnhanceStyleBlock());

    if (trimmed.contains(QStringLiteral("<html"), Qt::CaseInsensitive)) {
        const int headIndex = trimmed.indexOf(QStringLiteral("<head"), 0, Qt::CaseInsensitive);
        if (headIndex >= 0) {
            const int headClose = trimmed.indexOf(QLatin1Char('>'), headIndex);
            if (headClose >= 0) {
                return trimmed.left(headClose + 1) + styleTag + trimmed.mid(headClose + 1);
            }
        }
        const QRegularExpression htmlTagRe(QStringLiteral("<html\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch htmlMatch = htmlTagRe.match(trimmed);
        if (htmlMatch.hasMatch()) {
            const int insertAt = htmlMatch.capturedEnd();
            return trimmed.left(insertAt) + styleTag + trimmed.mid(insertAt);
        }
        return styleTag + trimmed;
    }

    return QString::fromUtf8(u8"<!DOCTYPE html><html><head><meta charset=\"utf-8\">%1</head><body>%2</body></html>")
        .arg(styleTag, trimmed);
}

constexpr char kReportDocumentStyleSheet[] = R"(
body { background-color: #ffffff; color: #1f2937; font-size: 13px; line-height: 1.5; }
h1, h2, h3, h4, h5, h6 { color: #111827; font-weight: 700; }
p, span, td, th, li, div, label { color: #374151; }
a { color: #1e78df; }
table { border-collapse: collapse; }
th, td { border: 1px solid #d7e1ed; padding: 6px 8px; }
)";

constexpr char kReportContentStyle[] =
    "QTextBrowser {"
    "   border: 1px solid #b8c7da;"
    "   border-radius: 6px;"
    "   background-color: #ffffff;"
    "   color: #111827;"
    "   padding: 8px;"
    "   font-size: 13px;"
    "}"
    "QScrollArea { border: none; background: transparent; }";
} // namespace

ReportViewDialog::ReportViewDialog(QWidget* parent)
    : FramelessDialog(parent)
{
    setTitleText(QString::fromUtf8(u8"机动车分析报告"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(true);
    resize(1000, 720);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet() + QString::fromUtf8(kReportContentStyle));

    QVBoxLayout* layout = contentLayout();
    layout->setContentsMargins(18, 12, 18, 18);
    layout->setSpacing(12);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* browser = new QTextBrowser(scrollArea);
    browser->setObjectName(QStringLiteral("reportViewBrowser"));
    browser->setOpenExternalLinks(true);
    scrollArea->setWidget(browser);
    layout->addWidget(scrollArea, 1);

    auto* footer = new QHBoxLayout();
    footer->addStretch();
    auto* closeButton = new QPushButton(QString::fromUtf8(u8"关闭"), this);
    closeButton->setFixedSize(88, 32);
    closeButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(closeButton, &QPushButton::clicked, this, &ReportViewDialog::accept);
    footer->addWidget(closeButton);
    layout->addLayout(footer);
}

void ReportViewDialog::setHtmlContent(const QString& html)
{
    auto* browser = findChild<QTextBrowser*>(QStringLiteral("reportViewBrowser"));
    if (!browser) {
        return;
    }
    browser->document()->setDefaultStyleSheet(QString::fromUtf8(kReportDocumentStyleSheet));
    browser->setHtml(wrapReportHtmlForDisplay(html));
}

void ReportViewDialog::showModal(const QString& html, const QString& title)
{
    ReportViewDialog dialog(nullptr);
    dialog.setTitleText(title);
    dialog.setHtmlContent(html);
    dialog.exec();
}
