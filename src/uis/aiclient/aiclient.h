/*****************************************************************//**
 * \file   aiclient.h
 * \brief  AI 对话客户端与会话管理
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef AICLIENT_H
#define AICLIENT_H

#include "aiclientconfig.h"
#include "aiknowledgebase.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <QString>

class AIChatSession;
class QNetworkReply;

class AIClient : public QObject {
    Q_OBJECT

public:
    static AIClient& instance();

    void initialize();
    QSharedPointer<AIChatSession> createSession();
    QSharedPointer<AIChatSession> session(const QString& key) const;
    bool hasSession(const QString& key) const;

    const AIClientConfig& config() const;
    const AIKnowledgeBase& knowledgeBase() const;
    int appliedContextLimit() const;

private:
    explicit AIClient(QObject* parent = nullptr);
    void fetchContextLimit();
    void applyContextLimitFromResponse(const QByteArray& data);
    void logAppliedContextLimit(const QString& reason) const;

private:
    bool m_initialized = false;
    AIClientConfig m_config;
    AIKnowledgeBase m_knowledgeBase;
    QHash<QString, QSharedPointer<AIChatSession>> m_sessions;
    QNetworkAccessManager* m_nam = nullptr;
};

#endif // AICLIENT_H
