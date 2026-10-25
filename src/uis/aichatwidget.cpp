/*****************************************************************//**
 * \file   aichatwidget.cpp
 * \brief  工作台流程步骤智能对话展示控件实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "aichatwidget.h"

#include "aiwordselectbar.h"
#include "aiclient/aichatsession.h"
#include "../casectrls/caseiconutils.h"
#include "httpapijson.h"

#include <QApplication>
#include <QAction>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QScrollArea>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShowEvent>
#include <QSize>
#include <QtMath>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

extern QStringList g_fontfamilies;

namespace {

constexpr int kFontChatTitle = 14;
constexpr int kFontChatBody = 13;
constexpr int kFontChatCaption = 12;

constexpr int kAvatarSize = 26;
constexpr int kRowSpacing = 8;
constexpr int kScrollRightGap = 8;
constexpr int kScrollAreaMinHeight = 240;
constexpr int kBubblePaddingH = 20;
constexpr int kBubblePaddingV = 16;
constexpr int kBubbleBorder = 1;
constexpr int kBubbleHeightSlack = 2;
constexpr int kMinViewportWidth = 80;
constexpr int kRelayoutMaxRetries = 3;

QString chatFontFamily()
{
    if (!g_fontfamilies.isEmpty()) {
        return g_fontfamilies.first();
    }
    if (qApp) {
        return qApp->font().family();
    }
    return QString();
}

QFont chatFont(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font;
    const QString family = chatFontFamily();
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

void applyChatFont(QWidget* widget, int pixelSize, QFont::Weight weight = QFont::Normal)
{
    if (!widget) {
        return;
    }
    widget->setFont(chatFont(pixelSize, weight));
}

QString chatBodyFontSizeCss()
{
    return QString::number(kFontChatBody);
}

QString commonQuestionButtonStyleSheet()
{
    return QStringLiteral(
        "QToolButton#aiCommonQuestionButton {"
        "   border: 1px solid rgba(47, 132, 227, 0.45);"
        "   border-radius: 6px;"
        "   background-color: #071b35;"
        "   color: rgba(110, 184, 255, 0.95);"
        "   padding: 0 8px;"
        "   min-height: 22px;"
        "   max-height: 22px;"
        "   text-align: left;"
        "}"
        "QToolButton#aiCommonQuestionButton:hover:enabled {"
        "   border-color: #66b5ff;"
        "   color: white;"
        "}"
        "QToolButton#aiCommonQuestionButton:disabled {"
        "   border-color: rgba(72, 112, 164, 0.28);"
        "   color: rgba(175, 199, 230, 0.40);"
        "}"
        "QToolButton#aiCommonQuestionButton::menu-indicator {"
        "   image: none;"
        "   width: 0;"
        "}");
}

QString commonQuestionMenuStyleSheet()
{
    return QStringLiteral(
        "QMenu {"
        "   background-color: #1e52b4;"
        "   border: 1px solid rgba(255, 255, 255, 0.18);"
        "   border-radius: 8px;"
        "   padding: 4px;"
        "}"
        "QMenu::item {"
        "   padding: 6px 24px 6px 10px;"
        "   color: rgba(255, 255, 255, 0.95);"
        "   border-radius: 4px;"
        "   font-size: %1px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: rgba(255, 255, 255, 0.14);"
        "}").arg(kFontChatBody);
}

QString aiAvatarPath()
{
    return QStringLiteral(":/icons/icons/workspace_ai_assistant.png");
}

QString userAvatarPath()
{
    return QStringLiteral(":/icons/icons/user@2x.png");
}

QPixmap roundedAvatar(const QString& path)
{
    return QPixmap(path).scaled(kAvatarSize, kAvatarSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString bubbleStyle(AIMessageRole role)
{
    const QString fontSize = chatBodyFontSizeCss();
    if (role == AIMessageRole::User) {
        return QStringLiteral(
            "QLabel {"
            "   color: rgba(255, 255, 255, 0.95);"
            "   background-color: #1e78df;"
            "   border-radius: 8px;"
            "   padding: 8px 10px;"
            "   font-size: %1px;"
            "   line-height: 1.45;"
            "}").arg(fontSize);
    }
    if (role == AIMessageRole::Error) {
        return QStringLiteral(
            "QLabel {"
            "   color: #ffd3d3;"
            "   background-color: rgba(220, 38, 38, 0.18);"
            "   border: 1px solid rgba(248, 113, 113, 0.45);"
            "   border-radius: 8px;"
            "   padding: 8px 10px;"
            "   font-size: %1px;"
            "   line-height: 1.45;"
            "}").arg(fontSize);
    }
    return QStringLiteral(
        "QLabel {"
        "   color: rgba(226, 238, 255, 0.95);"
        "   background-color: rgba(20, 56, 105, 0.92);"
        "   border: 1px solid rgba(71, 139, 230, 0.38);"
        "   border-radius: 8px;"
        "   padding: 8px 10px;"
        "   font-size: %1px;"
        "   line-height: 1.45;"
        "}").arg(fontSize);
}

QString bubbleHtmlDefaultStyleSheet()
{
    return QStringLiteral(
        "body {"
        "   color: rgba(226, 238, 255, 0.95);"
        "   font-size: %1px;"
        "   line-height: 1.45;"
        "   word-wrap: break-word;"
        "   word-break: break-all;"
        "   overflow-wrap: anywhere;"
        "}"
        "p { margin-top: 0; margin-bottom: 6px; }"
        "ul, ol { margin-top: 2px; margin-bottom: 6px; }"
        "li { margin-bottom: 2px; }"
        "strong { font-weight: 700; }"
        "code {"
        "   color: #9fd0ff;"
        "   background-color: rgba(3, 10, 22, 0.65);"
        "   word-break: break-all;"
        "   overflow-wrap: anywhere;"
        "}"
        "pre {"
        "   color: #9fd0ff;"
        "   background-color: rgba(3, 10, 22, 0.65);"
        "   white-space: pre-wrap;"
        "   word-wrap: break-word;"
        "   word-break: break-all;"
        "   overflow-wrap: anywhere;"
        "}"
        "a { color: rgba(226,238,255,0.95); }").arg(kFontChatBody);
}

QString markdownToBubbleHtml(const QString& markdown)
{
    QTextDocument doc;
    doc.setDefaultStyleSheet(bubbleHtmlDefaultStyleSheet());
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    doc.setMarkdown(markdown);
#else
    doc.setPlainText(markdown);
#endif
    return doc.toHtml();
}

bool bubbleHasBorder(const QLabel* bubble)
{
    return bubble && bubble->styleSheet().contains(QStringLiteral("border: 1px"));
}

QString detailLinkHtml(const QString& downloadUrl)
{
    if (downloadUrl.isEmpty()) {
        return QString();
    }
    return QStringLiteral("<p style=\"margin-top:8px;margin-bottom:0;\"><a style=\"color:rgba(226,238,255,0.95);\" href=\"%1\">%2</a></p>")
        .arg(downloadUrl.toHtmlEscaped(), QString::fromUtf8(u8"查看详情"));
}

void normalizeAssistantContent(QString& content, QString& downloadUrl)
{
    static const QRegularExpression legacyLink(
        QStringLiteral("\\n\\n\\[查看详情\\]\\(([^)]+)\\)\\s*$"));
    const QRegularExpressionMatch match = legacyLink.match(content);
    if (!match.hasMatch()) {
        return;
    }
    content = content.left(match.capturedStart()).trimmed();
    if (downloadUrl.isEmpty()) {
        downloadUrl = match.captured(1).trimmed();
    }
}

QString assistantBubbleHtml(const QString& content, const QString& downloadUrl)
{
    const QString trimmed = content.trimmed();
    QString htmlBody;
    if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) {
        htmlBody = QStringLiteral("<html><body style=\"color:rgba(226,238,255,0.95);font-size:%1px;"
                                  "line-height:1.45;word-wrap:break-word;word-break:break-all;"
                                  "overflow-wrap:anywhere;\"><pre style=\"margin:0;"
                                  "white-space:pre-wrap;word-wrap:break-word;word-break:break-all;"
                                  "overflow-wrap:anywhere;\">%2</pre></body></html>")
                       .arg(kFontChatBody)
                       .arg(trimmed.toHtmlEscaped());
    } else {
        htmlBody = markdownToBubbleHtml(content);
    }

    const QString link = detailLinkHtml(downloadUrl);
    if (link.isEmpty()) {
        return htmlBody;
    }
    if (htmlBody.contains(QStringLiteral("</body>"))) {
        return htmlBody.replace(QStringLiteral("</body>"), link + QStringLiteral("</body>"));
    }
    return htmlBody + link;
}

QString wrapPlainTextForBubble(const QString& text, int maxCharsPerLine = 42)
{
    if (text.size() <= maxCharsPerLine) {
        return text;
    }

    QString result;
    result.reserve(text.size() + text.size() / maxCharsPerLine);
    int lineLen = 0;
    for (const QChar ch : text) {
        result.append(ch);
        ++lineLen;
        if (lineLen < maxCharsPerLine) {
            continue;
        }
        if (ch.isSpace() || ch == QLatin1Char('&') || ch == QLatin1Char('?') || ch == QLatin1Char('/')
            || ch == QLatin1Char('=')) {
            result.append(QLatin1Char('\n'));
            lineLen = 0;
        } else if (lineLen >= maxCharsPerLine) {
            result.append(QLatin1Char('\n'));
            lineLen = 0;
        }
    }
    return result;
}

int maxChatBubbleWidth(const QScrollArea* scrollArea)
{
    if (!scrollArea || !scrollArea->viewport()) {
        return 220;
    }
    return qMax(120,
                scrollArea->viewport()->width() - kAvatarSize - kRowSpacing - kScrollRightGap);
}

void applyBubbleWidth(QLabel* bubble, const QScrollArea* scrollArea)
{
    if (!bubble) {
        return;
    }
    const int maxW = maxChatBubbleWidth(scrollArea);
    bubble->setMaximumWidth(maxW);
    bubble->setMinimumWidth(0);
    bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

int bubbleContentHeight(QLabel* bubble, int totalWidth)
{
    if (!bubble || totalWidth <= 0) {
        return 0;
    }

    const int border = bubbleHasBorder(bubble) ? kBubbleBorder : 0;
    const int contentWidth = qMax(1, totalWidth - kBubblePaddingH - 2 * border);
    QTextDocument doc;
    doc.setDefaultFont(bubble->font());
    doc.setDocumentMargin(0);
    if (bubble->textFormat() == Qt::PlainText) {
        doc.setPlainText(bubble->text());
    } else {
        // HTML 已由 markdownToBubbleHtml 写入样式，勿再套 defaultStyleSheet，否则高度会严重高估
        doc.setHtml(bubble->text());
    }
    doc.setTextWidth(contentWidth);
    return static_cast<int>(qCeil(doc.size().height()));
}

void reflowBubbleGeometry(QLabel* bubble, const QScrollArea* scrollArea)
{
    if (!bubble || !scrollArea || !scrollArea->viewport()) {
        return;
    }

    bubble->setMinimumHeight(0);
    bubble->setMaximumHeight(QWIDGETSIZE_MAX);

    const int w = qMin(bubble->maximumWidth() > 0 ? bubble->maximumWidth() : maxChatBubbleWidth(scrollArea),
                       maxChatBubbleWidth(scrollArea));
    if (w <= 0) {
        return;
    }

    const int contentH = bubbleContentHeight(bubble, w);
    const int border = bubbleHasBorder(bubble) ? kBubbleBorder : 0;
    const int docBasedH = contentH > 0
        ? (contentH + kBubblePaddingV + 2 * border + kBubbleHeightSlack)
        : 0;
    // heightForWidth 与 QLabel 绘制同源，取较大值兜住独立文档估矮导致的底部裁字
    const int labelBasedH = bubble->heightForWidth(w);
    const int fixedH = qMax(docBasedH, labelBasedH);
    if (fixedH <= 0) {
        return;
    }
    bubble->setFixedHeight(fixedH);
}

void setBubbleContent(QLabel* bubble,
                      AIMessageRole role,
                      const QString& content,
                      const QString& downloadUrl,
                      const QScrollArea* scrollArea,
                      bool skipReflow)
{
    if (!bubble) {
        return;
    }

    const QString displayText = content.isEmpty() ? QString::fromUtf8(u8"正在思考...") : content;
    if (role == AIMessageRole::User && !content.isEmpty()) {
        bubble->setTextFormat(Qt::PlainText);
        bubble->setText(wrapPlainTextForBubble(content));
        if (!skipReflow) {
            reflowBubbleGeometry(bubble, scrollArea);
        }
        return;
    }
    if (role == AIMessageRole::Assistant && !content.isEmpty()) {
        bubble->setTextFormat(Qt::RichText);
        bubble->setText(assistantBubbleHtml(content, downloadUrl));
        if (!skipReflow) {
            reflowBubbleGeometry(bubble, scrollArea);
        }
        return;
    }
    if (role == AIMessageRole::Error && !content.isEmpty()) {
        bubble->setTextFormat(Qt::PlainText);
        bubble->setText(wrapPlainTextForBubble(content));
        if (!skipReflow) {
            reflowBubbleGeometry(bubble, scrollArea);
        }
        return;
    }

    bubble->setTextFormat(Qt::PlainText);
    bubble->setText(displayText.isEmpty() ? displayText : wrapPlainTextForBubble(displayText));
    if (!skipReflow) {
        reflowBubbleGeometry(bubble, scrollArea);
    }
}

} // namespace

AIChatWidget::AIChatWidget(QWidget* parent)
    : QWidget(parent)
    , m_titleLabel(nullptr)
    , m_toggleButton(nullptr)
    , m_bodyWidget(nullptr)
    , m_scrollArea(nullptr)
    , m_messageListWidget(nullptr)
    , m_messageLayout(nullptr)
    , m_wordSelectBar(nullptr)
    , m_commonQuestionButton(nullptr)
    , m_commonQuestionMenu(nullptr)
    , m_inputEdit(nullptr)
    , m_sendButton(nullptr)
    , m_collapsed(true)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "AIChatWidget {"
        "   background-color: #07172f;"
        "   border: 1px solid rgba(60, 128, 220, 0.52);"
        "   border-radius: 8px;"
        "}"
        "AIChatWidget QLabel#aiChatTitle {"
        "   color: rgba(226, 238, 255, 0.95);"
        "   font-size: %1px;"
        "   font-weight: 700;"
        "}"
        "AIChatWidget QScrollArea {"
        "   border: none;"
        "   background-color: transparent;"
        "}"
        "AIChatWidget QScrollArea QWidget {"
        "   background-color: transparent;"
        "}"
        "AIChatWidget QLineEdit#aiChatInput {"
        "   border: 1px solid rgba(47, 132, 227, 0.45);"
        "   border-radius: 14px;"
        "   background-color: #030a16;"
        "   color: rgba(226, 238, 255, 0.92);"
        "   font-size: %1px;"
        "   padding: 4px 12px;"
        "   min-height: 24px;"
        "}"
        "AIChatWidget QLineEdit#aiChatInput::placeholder {"
        "   color: rgba(79, 178, 255, 0.55);"
        "}"
        "AIChatWidget QPushButton#aiChatSend {"
        "   border: none;"
        "   border-radius: 14px;"
        "   background-color: #1e78df;"
        "   min-width: 28px;"
        "   max-width: 28px;"
        "   min-height: 28px;"
        "   max-height: 28px;"
        "}"
        "AIChatWidget QPushButton#aiChatSend:hover {"
        "   background-color: #3294ff;"
        "}"
        "AIChatWidget QPushButton#aiChatSend:disabled {"
        "   background-color: rgba(30, 120, 223, 0.45);"
        "}").arg(kFontChatTitle));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    QWidget* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    QLabel* iconLabel = new QLabel(header);
    iconLabel->setPixmap(roundedAvatar(aiAvatarPath()));
    iconLabel->setFixedSize(kAvatarSize, kAvatarSize);
    headerLayout->addWidget(iconLabel);

    m_titleLabel = new QLabel(QString::fromUtf8(u8"智能对话"), header);
    m_titleLabel->setObjectName(QStringLiteral("aiChatTitle"));
    applyChatFont(m_titleLabel, kFontChatTitle, QFont::Bold);
    headerLayout->addWidget(m_titleLabel, 1);

    m_toggleButton = new QPushButton(QString::fromUtf8(u8"收起"), header);
    m_toggleButton->setObjectName(QStringLiteral("aiChatToggle"));
    m_toggleButton->setVisible(false);
    headerLayout->addWidget(m_toggleButton);

    layout->addWidget(header);

    m_bodyWidget = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(m_bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(4);

    m_scrollArea = new QScrollArea(m_bodyWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setMinimumHeight(kScrollAreaMinHeight);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_messageListWidget = new QWidget(m_scrollArea);
    m_messageLayout = new QVBoxLayout(m_messageListWidget);
    m_messageLayout->setContentsMargins(0, 2, 0, 2);
    m_messageLayout->setSpacing(8);
    m_messageLayout->addStretch(1);
    m_scrollArea->setWidget(m_messageListWidget);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (m_scrollArea->viewport()) {
        m_scrollArea->viewport()->installEventFilter(this);
    }
    bodyLayout->addWidget(m_scrollArea, 1);

    m_wordSelectBar = new AiWordSelectBar(m_bodyWidget);
    bodyLayout->addWidget(m_wordSelectBar, 0, Qt::AlignTop);

    m_commonQuestionButton = new QToolButton(m_bodyWidget);
    m_commonQuestionButton->setObjectName(QStringLiteral("aiCommonQuestionButton"));
    m_commonQuestionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_commonQuestionButton->setCursor(Qt::PointingHandCursor);
    m_commonQuestionButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_commonQuestionButton->setStyleSheet(commonQuestionButtonStyleSheet());
    applyChatFont(m_commonQuestionButton, kFontChatBody);
    m_commonQuestionMenu = new QMenu(m_commonQuestionButton);
    m_commonQuestionMenu->setStyleSheet(commonQuestionMenuStyleSheet());
    bodyLayout->addWidget(m_commonQuestionButton, 0, Qt::AlignTop);

    bodyLayout->addSpacing(0);

    QWidget* inputRow = new QWidget(m_bodyWidget);
    inputRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(8);

    m_inputEdit = new QLineEdit(inputRow);
    m_inputEdit->setObjectName(QStringLiteral("aiChatInput"));
    m_inputEdit->setPlaceholderText(QString::fromUtf8(u8"请输入"));
    applyChatFont(m_inputEdit, kFontChatTitle);
    inputLayout->addWidget(m_inputEdit, 1);

    m_sendButton = new QPushButton(inputRow);
    m_sendButton->setObjectName(QStringLiteral("aiChatSend"));
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setIcon(CaseIconUtils::workbenchSingleStateIcon(QStringLiteral("send")));
    m_sendButton->setIconSize(QSize(14, 14));
    inputLayout->addWidget(m_sendButton);

    bodyLayout->addWidget(inputRow, 0, Qt::AlignBottom);
    layout->addWidget(m_bodyWidget);

    connect(m_sendButton, &QPushButton::clicked, this, &AIChatWidget::onSendClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &AIChatWidget::onSendClicked);
    connect(m_commonQuestionButton, &QToolButton::clicked, this, &AIChatWidget::showCommonQuestionMenu);
    updateCommonQuestionButton();
    setCollapsed(true);
}

void AIChatWidget::setSession(const QSharedPointer<AIChatSession>& session)
{
    if (m_session == session && !m_useBackendAiApi) {
        return;
    }

    disconnectSession();
    m_session = session;
    if (m_session && !m_useBackendAiApi) {
        m_sessionConnections.append(connect(m_session.data(), &AIChatSession::messageAdded,
                                            this, &AIChatWidget::onSessionMessageAdded));
        m_sessionConnections.append(connect(m_session.data(), &AIChatSession::messageDelta,
                                            this, &AIChatWidget::onSessionMessageDelta));
        m_sessionConnections.append(connect(m_session.data(), &AIChatSession::runningChanged,
                                            this, &AIChatWidget::onSessionRunningChanged));
        renderSessionMessages();
        setInputBusy(m_session->isRunning());
        return;
    }

    if (!m_useBackendAiApi) {
        renderSessionMessages();
        setInputBusy(m_session && m_session->isRunning());
    }
}

QSharedPointer<AIChatSession> AIChatWidget::session() const
{
    return m_session;
}

void AIChatWidget::clear()
{
    while (m_messageLayout && m_messageLayout->count() > 1) {
        QLayoutItem* item = m_messageLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_messageLabels.clear();
    m_messageDownloadUrls.clear();
}

void AIChatWidget::appendPlainLine(const QString& line)
{
    if (line.isEmpty()) {
        return;
    }
    AIChatMessage msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.role = AIMessageRole::Log;
    msg.content = line;
    msg.createdAt = QDateTime::currentDateTime();
    appendMessage(msg, false);
}

void AIChatWidget::appendSystemLine(const QString& line)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    appendLogLine(QStringLiteral("[%1] %2").arg(ts, line));
}

void AIChatWidget::appendGrpcEvent(const GrpcStreamEventJson& event)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString title = event.j_data().j_title();
    const QString msg = event.j_message();
    const int pct = event.j_data().j_percent();

    QString line = QStringLiteral("[%1] ").arg(ts);
    if (!msg.isEmpty()) {
        line += msg;
    }
    if (!title.isEmpty()) {
        if (!msg.isEmpty()) {
            line += QStringLiteral("：");
        }
        line += title;
    }
    if (pct > 0) {
        line += QStringLiteral("  %1%").arg(pct);
    }
    appendLogLine(line);
}

void AIChatWidget::setIndeterminateBusy(bool busy)
{
    setInputBusy(busy);
}

void AIChatWidget::setUseBackendAiApi(bool enabled)
{
    if (m_useBackendAiApi == enabled) {
        return;
    }

    m_useBackendAiApi = enabled;
    if (m_useBackendAiApi) {
        disconnectSession();
        setInputBusy(false);
        return;
    }

    if (m_session) {
        setSession(m_session);
    }
}

bool AIChatWidget::useBackendAiApi() const
{
    return m_useBackendAiApi;
}

void AIChatWidget::appendAssistantReply(const QString& text)
{
    AIChatMessage msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.role = AIMessageRole::Assistant;
    msg.content = text;
    msg.createdAt = QDateTime::currentDateTime();
    appendMessage(msg, true);
}

QString AIChatWidget::beginAssistantReply()
{
    AIChatMessage msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.role = AIMessageRole::Assistant;
    msg.content = QString();
    msg.createdAt = QDateTime::currentDateTime();
    appendMessage(msg, true);
    return msg.id;
}

void AIChatWidget::finishAssistantReply(const QString& messageId,
                                        const QString& content,
                                        bool isError,
                                        const QString& downloadUrl)
{
    QString body = content.trimmed();
    QString linkUrl = downloadUrl.trimmed();
    normalizeAssistantContent(body, linkUrl);

    QLabel* bubble = m_messageLabels.value(messageId, nullptr);
    if (!bubble) {
        if (!body.isEmpty()) {
            if (isError) {
                appendSystemLine(body);
            } else {
                AIChatMessage msg;
                msg.id = messageId;
                msg.role = AIMessageRole::Assistant;
                msg.content = body;
                msg.downloadUrl = linkUrl;
                msg.createdAt = QDateTime::currentDateTime();
                appendMessage(msg, true);
            }
        }
        emit dialogMessageUpdated(messageId, body, isError, linkUrl);
        return;
    }

    const AIMessageRole role = isError ? AIMessageRole::Error : AIMessageRole::Assistant;
    bubble->setStyleSheet(bubbleStyle(role));
    applyChatFont(bubble, kFontChatBody);
    applyBubbleWidth(bubble, m_scrollArea);
    setBubbleContent(bubble, role, body, linkUrl, m_scrollArea, false);
    m_messageDownloadUrls.insert(messageId, linkUrl);
    emit dialogMessageUpdated(messageId, body, isError, linkUrl);
    scheduleRelayoutAndScroll();
}

void AIChatWidget::restoreChatTimeline(const QList<AIChatMessage>& messages)
{
    m_batchRestoring = true;
    clear();
    for (const AIChatMessage& message : messages) {
        appendMessage(message, false);
    }
    m_batchRestoring = false;
    scheduleRelayoutAndScroll();
}

void AIChatWidget::appendTimelineMessage(const AIChatMessage& message, bool emitDialogSignals)
{
    appendMessage(message, emitDialogSignals);
}

void AIChatWidget::setPercent(int percent)
{
    Q_UNUSED(percent);
}

void AIChatWidget::setCollapsed(bool collapsed)
{
    const bool wasCollapsed = m_collapsed;
    m_collapsed = collapsed;
    if (m_bodyWidget) {
        m_bodyWidget->setVisible(!collapsed);
    }
    if (wasCollapsed && !collapsed) {
        scheduleRelayoutAndScroll();
    }
}

bool AIChatWidget::isCollapsed() const
{
    return m_collapsed;
}

void AIChatWidget::setTitleText(const QString& title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

void AIChatWidget::toggleCollapsed()
{
    setCollapsed(!m_collapsed);
}

void AIChatWidget::onSendClicked()
{
    if (!m_inputEdit || m_inputBusy) {
        return;
    }
    const QString text = m_inputEdit->text().trimmed();
    m_inputEdit->clear();

    if (m_useBackendAiApi) {
        if (!text.isEmpty()) {
            AIChatMessage userMsg;
            userMsg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            userMsg.role = AIMessageRole::User;
            userMsg.content = text;
            userMsg.createdAt = QDateTime::currentDateTime();
            appendMessage(userMsg, true);
        }
        setInputBusy(true);
        const int l1 = m_wordSelectBar ? m_wordSelectBar->l1Index() : 0;
        const int l2 = m_wordSelectBar ? m_wordSelectBar->l2Index() : 0;
        emit promptSubmitted(text, l1, l2, m_commonQuestion);
        return;
    }

    if (text.isEmpty() || !m_session || m_session->isRunning()) {
        return;
    }
    m_session->sendMessage(text);
}

void AIChatWidget::showCommonQuestionMenu()
{
    if (!m_commonQuestionButton || !m_commonQuestionMenu || m_inputBusy || m_commonQuestions.isEmpty()) {
        return;
    }

    m_commonQuestionMenu->clear();
    QAction* clearAction = m_commonQuestionMenu->addAction(QString::fromUtf8(u8"不使用常见问题"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        setCommonQuestion(QString());
    });
    m_commonQuestionMenu->addSeparator();

    for (const QString& question : m_commonQuestions) {
        QAction* action = m_commonQuestionMenu->addAction(question);
        action->setCheckable(true);
        action->setChecked(question == m_commonQuestion);
        connect(action, &QAction::triggered, this, [this, question]() {
            setCommonQuestion(question);
        });
    }
    m_commonQuestionMenu->popup(m_commonQuestionButton->mapToGlobal(QPoint(0, m_commonQuestionButton->height())));
}

void AIChatWidget::onSessionMessageAdded(const AIChatMessage& message)
{
    appendMessage(message, false);
}

void AIChatWidget::onSessionMessageDelta(const QString& messageId, const QString& delta, const QString& fullContent)
{
    Q_UNUSED(delta);
    QLabel* label = m_messageLabels.value(messageId, nullptr);
    if (label) {
        const QString linkUrl = m_messageDownloadUrls.value(messageId);
        setBubbleContent(label, AIMessageRole::Assistant, fullContent, linkUrl, m_scrollArea, false);
        scrollToBottom();
    }
}

void AIChatWidget::onSessionRunningChanged(bool running)
{
    setInputBusy(running);
}

void AIChatWidget::disconnectSession()
{
    for (const QMetaObject::Connection& connection : m_sessionConnections) {
        disconnect(connection);
    }
    m_sessionConnections.clear();
}

void AIChatWidget::renderSessionMessages()
{
    clear();
    if (!m_session) {
        return;
    }
    const QVector<AIChatMessage> messages = m_session->visibleMessages();
    for (const AIChatMessage& message : messages) {
        appendMessage(message, false);
    }
}

void AIChatWidget::appendMessage(const AIChatMessage& message, bool emitDialogSignals)
{
    if (!m_messageLayout) {
        return;
    }

    if (message.role == AIMessageRole::Log) {
        appendLogLine(message.content);
        return;
    }

    QWidget* row = new QWidget(m_messageListWidget);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    QLabel* avatar = new QLabel(row);
    avatar->setFixedSize(kAvatarSize, kAvatarSize);
    avatar->setScaledContents(false);
    const bool isUser = message.role == AIMessageRole::User;
    avatar->setPixmap(roundedAvatar(isUser ? userAvatarPath() : aiAvatarPath()));

    QLabel* bubble = new QLabel(row);
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QString messageContent = message.content;
    QString messageDownloadUrl = message.downloadUrl;
    if (message.role == AIMessageRole::Assistant || message.role == AIMessageRole::Error) {
        normalizeAssistantContent(messageContent, messageDownloadUrl);
    }

    applyBubbleWidth(bubble, m_scrollArea);
    bubble->setStyleSheet(bubbleStyle(message.role));
    applyChatFont(bubble, kFontChatBody);
    if (!isUser) {
        configureAssistantBubble(bubble);
    }
    setBubbleContent(bubble, message.role, messageContent, messageDownloadUrl, m_scrollArea, m_batchRestoring);
    if (!messageDownloadUrl.isEmpty()) {
        m_messageDownloadUrls.insert(message.id, messageDownloadUrl);
    }

    const QString nameLabelStyle = QStringLiteral(
        "QLabel { color: rgba(143, 196, 255, 0.92); font-weight: 600; }");

    if (isUser) {
        rowLayout->addStretch(1);

        auto* userColumn = new QWidget(row);
        auto* userColumnLayout = new QVBoxLayout(userColumn);
        userColumnLayout->setContentsMargins(0, 0, 0, 0);
        userColumnLayout->setSpacing(4);

        QLabel* nameLabel = new QLabel(QString::fromUtf8(u8"用户"), userColumn);
        nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        nameLabel->setStyleSheet(nameLabelStyle);
        applyChatFont(nameLabel, kFontChatBody, QFont::DemiBold);
        userColumnLayout->addWidget(nameLabel);
        userColumnLayout->addWidget(bubble, 0, Qt::AlignRight);

        rowLayout->addWidget(userColumn, 0);
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);
    } else {
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);

        auto* aiColumn = new QWidget(row);
        auto* aiColumnLayout = new QVBoxLayout(aiColumn);
        aiColumnLayout->setContentsMargins(0, 0, 0, 0);
        aiColumnLayout->setSpacing(4);

        QLabel* nameLabel = new QLabel(QString::fromUtf8(u8"智能助手"), aiColumn);
        nameLabel->setStyleSheet(nameLabelStyle);
        applyChatFont(nameLabel, kFontChatBody, QFont::DemiBold);
        aiColumnLayout->addWidget(nameLabel);
        aiColumnLayout->addWidget(bubble);

        rowLayout->addWidget(aiColumn, 1);
    }

    m_messageLayout->insertWidget(qMax(0, m_messageLayout->count() - 1), row);
    m_messageLabels.insert(message.id, bubble);

    if (emitDialogSignals
        && (message.role == AIMessageRole::User || message.role == AIMessageRole::Assistant
            || message.role == AIMessageRole::Error)) {
        emit dialogMessageAdded(message);
    }
    if (!m_batchRestoring) {
        scrollToBottom();
    }
}

void AIChatWidget::appendLogLine(const QString& line, bool warning)
{
    if (!m_messageLayout || line.isEmpty()) {
        return;
    }

    QLabel* label = new QLabel(line, m_messageListWidget);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setStyleSheet(QStringLiteral(
        "QLabel {"
        "   color: %1;"
        "   background-color: rgba(3, 10, 22, 0.45);"
        "   border-radius: 4px;"
        "   padding: 3px 6px;"
        "}").arg(warning ? QStringLiteral("#ffd3d3") : QStringLiteral("#4fb2ff")));
    applyChatFont(label, kFontChatBody);
    m_messageLayout->insertWidget(qMax(0, m_messageLayout->count() - 1), label);
    if (!m_batchRestoring) {
        scrollToBottom();
    }
}

void AIChatWidget::scrollToBottom()
{
    if (!m_scrollArea || !m_scrollArea->verticalScrollBar()) {
        return;
    }
    auto doScroll = [this]() {
        if (m_messageListWidget) {
            m_messageListWidget->adjustSize();
            m_messageListWidget->updateGeometry();
        }
        if (m_scrollArea && m_scrollArea->verticalScrollBar()) {
            QScrollBar* bar = m_scrollArea->verticalScrollBar();
            bar->setValue(bar->maximum());
        }
    };
    QTimer::singleShot(0, this, doScroll);
    QTimer::singleShot(50, this, doScroll);
}

void AIChatWidget::scheduleRelayoutAndScroll()
{
    m_relayoutRetryCount = 0;
    QTimer::singleShot(0, this, [this]() { relayoutBubbles(); });
    QTimer::singleShot(50, this, [this]() { relayoutBubbles(); });
    QTimer::singleShot(150, this, [this]() { relayoutBubbles(); });
}

void AIChatWidget::configureAssistantBubble(QLabel* bubble)
{
    if (!bubble) {
        return;
    }
    bubble->setOpenExternalLinks(false);
    bubble->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(bubble, &QLabel::linkActivated, this, &AIChatWidget::onBubbleLinkActivated, Qt::UniqueConnection);
}

void AIChatWidget::relayoutBubbles()
{
    if (!m_scrollArea || !m_scrollArea->viewport() || !m_messageListWidget) {
        return;
    }

    const int viewportWidth = m_scrollArea->viewport()->width();
    if (viewportWidth < kMinViewportWidth) {
        if (m_relayoutRetryCount < kRelayoutMaxRetries) {
            ++m_relayoutRetryCount;
            QTimer::singleShot(0, this, [this]() { relayoutBubbles(); });
        }
        return;
    }

    m_relayoutRetryCount = 0;
    if (viewportWidth > 0) {
        m_messageListWidget->setMaximumWidth(viewportWidth);
    }
    for (auto it = m_messageLabels.constBegin(); it != m_messageLabels.constEnd(); ++it) {
        QLabel* bubble = it.value();
        if (!bubble) {
            continue;
        }
        applyBubbleWidth(bubble, m_scrollArea);
        reflowBubbleGeometry(bubble, m_scrollArea);
    }
    if (m_messageListWidget) {
        m_messageListWidget->updateGeometry();
    }
    scrollToBottom();
}

void AIChatWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    scheduleRelayoutAndScroll();
}

void AIChatWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scheduleRelayoutAndScroll();
}

bool AIChatWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (m_scrollArea && watched == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        scheduleRelayoutAndScroll();
    }
    return QWidget::eventFilter(watched, event);
}

void AIChatWidget::onBubbleLinkActivated(const QString& link)
{
    const QString url = link.trimmed();
    if (url.isEmpty()) {
        return;
    }
    emit downloadLinkActivated(url);
}

void AIChatWidget::setAiConfig(const AiConfigModel* config)
{
    if (m_wordSelectBar) {
        m_wordSelectBar->setConfig(config);
    }
}

AiWordSelectBar* AIChatWidget::wordSelectBar() const
{
    return m_wordSelectBar;
}

void AIChatWidget::setWordSelection(int l1Index, int l2Index)
{
    if (m_wordSelectBar) {
        m_wordSelectBar->setSelection(l1Index, l2Index);
    }
}

void AIChatWidget::setCommonQuestions(const QStringList& questions)
{
    m_commonQuestions.clear();
    for (const QString& question : questions) {
        const QString trimmed = question.trimmed();
        if (!trimmed.isEmpty() && !m_commonQuestions.contains(trimmed)) {
            m_commonQuestions.append(trimmed);
        }
    }
    if (!m_commonQuestion.isEmpty() && !m_commonQuestions.contains(m_commonQuestion)) {
        m_commonQuestion.clear();
    }
    updateCommonQuestionButton();
}

QString AIChatWidget::commonQuestion() const
{
    return m_commonQuestion;
}

void AIChatWidget::setCommonQuestion(const QString& question)
{
    const QString trimmed = question.trimmed();
    m_commonQuestion = m_commonQuestions.contains(trimmed) ? trimmed : QString();
    updateCommonQuestionButton();
}

void AIChatWidget::updateCommonQuestionButton()
{
    if (!m_commonQuestionButton) {
        return;
    }

    const QString placeholder = QString::fromUtf8(u8"常见问题");
    const bool hasQuestions = !m_commonQuestions.isEmpty();
    const QString label = m_commonQuestion.isEmpty() ? placeholder : m_commonQuestion;
    const QString text = label + QChar(0x25BE);
    m_commonQuestionButton->setText(text);
    m_commonQuestionButton->setToolTip(m_commonQuestion.isEmpty()
                                           ? QString::fromUtf8(u8"选择常见问题")
                                           : m_commonQuestion);
    m_commonQuestionButton->setEnabled(!m_inputBusy && hasQuestions);
}

void AIChatWidget::setInputBusy(bool busy)
{
    m_inputBusy = busy;
    if (m_inputEdit) {
        m_inputEdit->setEnabled(!busy);
    }
    if (m_sendButton) {
        m_sendButton->setEnabled(!busy);
    }
    if (m_wordSelectBar && m_useBackendAiApi) {
        m_wordSelectBar->setInteractionEnabled(!busy);
    }
    if (m_commonQuestionButton && m_useBackendAiApi) {
        m_commonQuestionButton->setEnabled(!busy && !m_commonQuestions.isEmpty());
    }
}
