/*****************************************************************//**
 * \file   aichatsession.cpp
 * \brief  单个 AI 对话会话实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "aichatsession.h"

#include "aiknowledgebase.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUuid>

namespace {

constexpr int kCharsPerToken = 4;
constexpr int kRecentContextMessages = 8;

QString assistantRoleDescription()
{
    return QString::fromUtf8(
        u8"你是“TNP AI智能助手”，服务于 TransNetPro2 城市交通 GIS 与交通网络分析软件。"
        u8"请基于软件功能、知识库和交通/GIS专业常识回答；无法确认时明确说明不确定。"
        u8"回答应准确、简短、专业，不要臆测。");
}

QJsonObject makeMessageObject(const QString& role, const QString& content)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("role"), role);
    obj.insert(QStringLiteral("content"), content);
    return obj;
}

QString bearerValue(const QString& apiKey)
{
    return QStringLiteral("Bearer ") + apiKey;
}

} // namespace

AIChatSession::AIChatSession(const AIClientConfig& config, AIKnowledgeBase* knowledgeBase, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_knowledgeBase(knowledgeBase)
    , m_key(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_nam(new QNetworkAccessManager(this))
    , m_idleTimer(new QTimer(this))
{
    qRegisterMetaType<AIChatMessage>("AIChatMessage");
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, [this]() {
        finishWithError(QString::fromUtf8(u8"AI 回复超时，请稍后重试"));
    });
}

AIChatSession::~AIChatSession()
{
    cancel();
}

QString AIChatSession::key() const
{
    return m_key;
}

bool AIChatSession::isRunning() const
{
    return m_running;
}

QVector<AIChatMessage> AIChatSession::visibleMessages() const
{
    return m_visibleMessages;
}

QVector<AIChatMessage> AIChatSession::contextMessages() const
{
    return m_contextMessages;
}

void AIChatSession::setContextLimit(int contextLimit)
{
    if (contextLimit > 0) {
        m_config.contextLimit = contextLimit;
    }
}

void AIChatSession::sendMessage(const QString& content)
{
    const QString userContent = content.trimmed();
    if (userContent.isEmpty() || m_running) {
        return;
    }

    AIChatMessage userMessage;
    userMessage.id = nextMessageId();
    userMessage.role = AIMessageRole::User;
    userMessage.content = userContent;
    userMessage.createdAt = QDateTime::currentDateTime();
    userMessage.inContext = true;
    m_visibleMessages.append(userMessage);
    m_contextMessages.append(userMessage);
    emit messageAdded(userMessage);

    if (!m_config.isUsable()) {
        finishWithError(QString::fromUtf8(u8"AI 服务未配置，请检查 ai_client.ini"));
        return;
    }

    if (shouldCompact()) {
        requestCompactThenStart(userContent);
        return;
    }
    startChatRequest(userContent);
}

void AIChatSession::cancel()
{
    stopIdleTimer();
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply.clear();
    }
    m_streamBuffer.clear();
    m_currentAssistantId.clear();
    m_currentAssistantContent.clear();
    setRunning(false);
}

void AIChatSession::startChatRequest(const QString& userContent)
{
    cancel();
    setRunning(true);

    AIChatMessage assistantMessage;
    assistantMessage.id = nextMessageId();
    assistantMessage.role = AIMessageRole::Assistant;
    assistantMessage.createdAt = QDateTime::currentDateTime();
    assistantMessage.inContext = false;
    m_currentAssistantId = assistantMessage.id;
    m_visibleMessages.append(assistantMessage);
    emit messageAdded(assistantMessage);

    const QJsonObject body = buildChatRequestBody(userContent, m_config.streamEnabled);
    QNetworkRequest request = buildNetworkRequest();
    m_reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AIChatSession::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AIChatSession::onReplyFinished);
    armIdleTimer();
}

void AIChatSession::requestCompactThenStart(const QString& userContent)
{
    setRunning(true);

    QStringList lines;
    for (const AIChatMessage& message : m_contextMessages) {
        if (message.role == AIMessageRole::User || message.role == AIMessageRole::Assistant) {
            lines.append(QStringLiteral("%1: %2").arg(roleName(message.role), message.content));
        }
    }

    QJsonArray messages;
    messages.append(makeMessageObject(QStringLiteral("system"), QString::fromUtf8(u8"请将以下多轮对话压缩为简短摘要，保留用户目标、关键结论和未完成事项。")));
    messages.append(makeMessageObject(QStringLiteral("user"), lines.join(QLatin1Char('\n')).left(20000)));

    QJsonObject body;
    body.insert(QStringLiteral("model"), m_config.modelId);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), false);

    QNetworkReply* compactReply = m_nam->post(buildNetworkRequest(), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(compactReply, &QNetworkReply::finished, this, [this, compactReply, userContent]() {
        const QByteArray data = compactReply->readAll();
        const bool ok = compactReply->error() == QNetworkReply::NoError;
        compactReply->deleteLater();

        if (ok) {
            const QString summary = extractNonStreamContent(data).trimmed();
            if (!summary.isEmpty()) {
                applyCompactSummary(summary);
            } else {
                trimContextForLimit();
            }
        } else {
            trimContextForLimit();
        }

        setRunning(false);
        startChatRequest(userContent);
    });
}

void AIChatSession::applyCompactSummary(const QString& summary)
{
    QVector<AIChatMessage> recent;
    for (int i = m_contextMessages.size() - 1; i >= 0 && recent.size() < kRecentContextMessages; --i) {
        const AIChatMessage& message = m_contextMessages.at(i);
        if (message.role == AIMessageRole::User || message.role == AIMessageRole::Assistant) {
            recent.prepend(message);
        }
    }

    AIChatMessage summaryMessage;
    summaryMessage.id = nextMessageId();
    summaryMessage.role = AIMessageRole::System;
    summaryMessage.content = QString::fromUtf8(u8"会话摘要：") + summary;
    summaryMessage.createdAt = QDateTime::currentDateTime();
    summaryMessage.inContext = true;

    m_contextMessages.clear();
    m_contextMessages.append(summaryMessage);
    for (const AIChatMessage& message : recent) {
        m_contextMessages.append(message);
    }
}

void AIChatSession::trimContextForLimit()
{
    while (estimatedContextTokens() > m_config.contextLimit * m_config.autoCompactRatio
           && m_contextMessages.size() > 1) {
        m_contextMessages.removeFirst();
    }
}

bool AIChatSession::shouldCompact() const
{
    return estimatedContextTokens() >= static_cast<int>(m_config.contextLimit * m_config.autoCompactRatio);
}

int AIChatSession::estimatedContextTokens() const
{
    int chars = assistantRoleDescription().size();
    for (const AIChatMessage& message : m_contextMessages) {
        if (message.inContext) {
            chars += message.content.size();
        }
    }
    return qMax(1, chars / kCharsPerToken);
}

QJsonArray AIChatSession::buildRequestMessages(const QString& userContent) const
{
    QJsonArray messages;
    messages.append(makeMessageObject(QStringLiteral("system"), assistantRoleDescription()));

    if (m_knowledgeBase) {
        const int remainingTokens = static_cast<int>(m_config.contextLimit * m_config.autoCompactRatio) - estimatedContextTokens();
        const int knowledgeMaxChars = qBound(1000, remainingTokens * kCharsPerToken, 4000);
        const QString knowledge = m_knowledgeBase->buildContextForQuestion(userContent, knowledgeMaxChars);
        if (!knowledge.isEmpty()) {
            messages.append(makeMessageObject(
                QStringLiteral("system"),
                QString::fromUtf8(u8"以下是软件用户知识库片段，仅在相关时参考，不要暴露内部来源或接口细节：\n") + knowledge));
        }
    }

    for (const AIChatMessage& message : m_contextMessages) {
        if (!message.inContext || message.content.isEmpty()) {
            continue;
        }
        if (message.role == AIMessageRole::User || message.role == AIMessageRole::Assistant || message.role == AIMessageRole::System) {
            messages.append(makeMessageObject(roleName(message.role), message.content));
        }
    }
    return messages;
}

QJsonObject AIChatSession::buildChatRequestBody(const QString& userContent, bool stream) const
{
    QJsonObject body;
    body.insert(QStringLiteral("model"), m_config.modelId);
    body.insert(QStringLiteral("messages"), buildRequestMessages(userContent));
    body.insert(QStringLiteral("stream"), stream);
    return body;
}

QNetworkRequest AIChatSession::buildNetworkRequest() const
{
    QNetworkRequest request(QUrl(m_config.chatCompletionsUrl()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", m_config.streamEnabled ? "text/event-stream" : "application/json");
    request.setRawHeader("Authorization", bearerValue(m_config.apiKey).toUtf8());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(m_config.requestTimeoutMs);
#endif
    return request;
}

void AIChatSession::onReadyRead()
{
    if (!m_reply) {
        return;
    }
    armIdleTimer();
    if (!m_config.streamEnabled) {
        return;
    }
    m_streamBuffer.append(m_reply->readAll());
    processStreamBuffer();
}

void AIChatSession::onReplyFinished()
{
    if (!m_reply) {
        return;
    }

    stopIdleTimer();
    const QNetworkReply::NetworkError error = m_reply->error();
    const QString errorString = m_reply->errorString();
    const QByteArray remaining = m_reply->readAll();
    if (!remaining.isEmpty()) {
        m_streamBuffer.append(remaining);
    }

    m_reply->deleteLater();
    m_reply.clear();

    if (error != QNetworkReply::NoError) {
        finishWithError(errorString);
        return;
    }

    if (m_config.streamEnabled) {
        processStreamBuffer();
        if (!m_streamBuffer.trimmed().isEmpty()) {
            processStreamBlock(m_streamBuffer);
            m_streamBuffer.clear();
        }
        finishAssistantMessage();
    } else {
        m_currentAssistantContent = extractNonStreamContent(m_streamBuffer);
        emit messageDelta(m_currentAssistantId, m_currentAssistantContent, m_currentAssistantContent);
        finishAssistantMessage();
    }
}

void AIChatSession::processStreamBuffer()
{
    while (true) {
        int sep = m_streamBuffer.indexOf("\n\n");
        int sepLen = 2;
        if (sep < 0) {
            sep = m_streamBuffer.indexOf("\r\n\r\n");
            sepLen = 4;
        }
        if (sep < 0) {
            break;
        }

        const QByteArray block = m_streamBuffer.left(sep);
        m_streamBuffer.remove(0, sep + sepLen);
        processStreamBlock(block);
    }
}

void AIChatSession::processStreamBlock(const QByteArray& block)
{
    const QStringList lines = QString::fromUtf8(block).split(QLatin1Char('\n'));
    QStringList dataParts;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.startsWith(QLatin1String("data:"))) {
            QString payload = line.mid(5).trimmed();
            if (payload == QLatin1String("[DONE]")) {
                finishAssistantMessage();
                return;
            }
            dataParts.append(payload);
        }
    }
    if (dataParts.isEmpty()) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(dataParts.join(QLatin1Char('\n')).toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    for (const QJsonValue& choiceValue : choices) {
        const QJsonObject choice = choiceValue.toObject();
        const QJsonObject deltaObj = choice.value(QStringLiteral("delta")).toObject();
        const QString delta = deltaObj.value(QStringLiteral("content")).toString();
        if (!delta.isEmpty()) {
            m_currentAssistantContent += delta;
            emit messageDelta(m_currentAssistantId, delta, m_currentAssistantContent);
        }
    }
}

void AIChatSession::finishAssistantMessage()
{
    if (m_currentAssistantId.isEmpty()) {
        return;
    }

    for (AIChatMessage& message : m_visibleMessages) {
        if (message.id == m_currentAssistantId) {
            message.content = m_currentAssistantContent;
            break;
        }
    }

    if (!m_currentAssistantContent.trimmed().isEmpty()) {
        AIChatMessage contextMessage;
        contextMessage.id = m_currentAssistantId;
        contextMessage.role = AIMessageRole::Assistant;
        contextMessage.content = m_currentAssistantContent;
        contextMessage.createdAt = QDateTime::currentDateTime();
        contextMessage.inContext = true;
        m_contextMessages.append(contextMessage);
    }

    emit messageFinished(m_currentAssistantId, m_currentAssistantContent);
    m_currentAssistantId.clear();
    m_currentAssistantContent.clear();
    setRunning(false);
}

void AIChatSession::finishWithError(const QString& message)
{
    cancel();

    AIChatMessage errorMessage;
    errorMessage.id = nextMessageId();
    errorMessage.role = AIMessageRole::Error;
    errorMessage.content = message.isEmpty() ? QString::fromUtf8(u8"AI 对话失败") : message;
    errorMessage.createdAt = QDateTime::currentDateTime();
    errorMessage.inContext = false;
    m_visibleMessages.append(errorMessage);
    emit messageAdded(errorMessage);
    emit errorOccurred(errorMessage.content);
}

void AIChatSession::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    emit runningChanged(m_running);
}

void AIChatSession::armIdleTimer()
{
    if (m_idleTimer) {
        m_idleTimer->start(m_config.streamIdleTimeoutMs);
    }
}

void AIChatSession::stopIdleTimer()
{
    if (m_idleTimer) {
        m_idleTimer->stop();
    }
}

QString AIChatSession::nextMessageId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString AIChatSession::roleName(AIMessageRole role)
{
    switch (role) {
    case AIMessageRole::User:
        return QStringLiteral("user");
    case AIMessageRole::Assistant:
        return QStringLiteral("assistant");
    case AIMessageRole::System:
        return QStringLiteral("system");
    case AIMessageRole::Log:
    case AIMessageRole::Error:
    default:
        return QStringLiteral("system");
    }
}

QString AIChatSession::extractNonStreamContent(const QByteArray& data)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return QString();
    }

    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return QString();
    }
    const QJsonObject message = choices.first().toObject().value(QStringLiteral("message")).toObject();
    return message.value(QStringLiteral("content")).toString();
}
