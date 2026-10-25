/*****************************************************************//**
 * \file   aiclient.cpp
 * \brief  AI 对话客户端与会话管理实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "aiclient.h"

#include "aichatsession.h"
#include "logprinter_ex.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

AIClient& AIClient::instance()
{
    static AIClient ins;
    ins.initialize();
    return ins;
}

AIClient::AIClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void AIClient::initialize()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    m_config = AIClientConfig::loadFromAppDir();
    m_knowledgeBase.loadDirectory(m_config.knowledgeDir);
    fetchContextLimit();
}

QSharedPointer<AIChatSession> AIClient::createSession()
{
    initialize();
    auto sessionPtr = QSharedPointer<AIChatSession>::create(m_config, &m_knowledgeBase);
    m_sessions.insert(sessionPtr->key(), sessionPtr);
    return sessionPtr;
}

QSharedPointer<AIChatSession> AIClient::session(const QString& key) const
{
    return m_sessions.value(key);
}

bool AIClient::hasSession(const QString& key) const
{
    return m_sessions.contains(key);
}

const AIClientConfig& AIClient::config() const
{
    return m_config;
}

const AIKnowledgeBase& AIClient::knowledgeBase() const
{
    return m_knowledgeBase;
}

int AIClient::appliedContextLimit() const
{
    return m_config.contextLimit;
}

void AIClient::fetchContextLimit()
{
    if (!m_config.isUsable()) {
        m_config.contextLimit = AIClientConfig::DefaultContextLimit;
        logAppliedContextLimit(QString::fromUtf8(u8"AI 服务未配置，使用默认上下文上限"));
        return;
    }

    QNetworkRequest request(QUrl(m_config.modelDetailUrl()));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_config.apiKey).toUtf8());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif

    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();

        if (!ok) {
            m_config.contextLimit = AIClientConfig::DefaultContextLimit;
            logAppliedContextLimit(QString::fromUtf8(u8"获取模型上下文上限失败，使用默认值"));
            return;
        }
        applyContextLimitFromResponse(data);
    });
}

void AIClient::applyContextLimitFromResponse(const QByteArray& data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonObject root = doc.object();
    int limit = root.value(QStringLiteral("context_length")).toInt();
    if (limit <= 0) {
        limit = root.value(QStringLiteral("max_context_length")).toInt();
    }
    if (limit <= 0) {
        const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
        limit = metadata.value(QStringLiteral("context_length")).toInt();
        if (limit <= 0) {
            limit = metadata.value(QStringLiteral("max_context_length")).toInt();
        }
    }

    if (limit <= 0) {
        m_config.contextLimit = AIClientConfig::DefaultContextLimit;
        logAppliedContextLimit(QString::fromUtf8(u8"模型信息未包含上下文上限，使用默认值"));
        return;
    }

    m_config.contextLimit = limit;
    for (const QSharedPointer<AIChatSession>& sessionPtr : m_sessions) {
        if (sessionPtr) {
            sessionPtr->setContextLimit(limit);
        }
    }
    logAppliedContextLimit(QString::fromUtf8(u8"已应用上游模型上下文上限"));
}

void AIClient::logAppliedContextLimit(const QString& reason) const
{
    LOG_INFO() << QString::fromUtf8(u8"AI 上下文上限：%1，模型：%2，原因：%3")
                      .arg(m_config.contextLimit)
                      .arg(m_config.modelId)
                      .arg(reason);
}
