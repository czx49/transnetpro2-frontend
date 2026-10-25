/*****************************************************************//**
 * \file   aichatsession.h
 * \brief  单个 AI 对话会话
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef AICHATSESSION_H
#define AICHATSESSION_H

#include "aiclientconfig.h"
#include "aimessage.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVector>

class AIKnowledgeBase;
class QNetworkReply;
class QTimer;

class AIChatSession : public QObject {
    Q_OBJECT

public:
    explicit AIChatSession(const AIClientConfig& config, AIKnowledgeBase* knowledgeBase, QObject* parent = nullptr);
    ~AIChatSession() override;

    QString key() const;
    bool isRunning() const;
    QVector<AIChatMessage> visibleMessages() const;
    QVector<AIChatMessage> contextMessages() const;

    void setContextLimit(int contextLimit);
    void sendMessage(const QString& content);
    void cancel();

signals:
    void messageAdded(const AIChatMessage& message);
    void messageDelta(const QString& messageId, const QString& delta, const QString& fullContent);
    void messageFinished(const QString& messageId, const QString& content);
    void errorOccurred(const QString& message);
    void runningChanged(bool running);

private slots:
    void onReadyRead();
    void onReplyFinished();

private:
    void startChatRequest(const QString& userContent);
    void requestCompactThenStart(const QString& userContent);
    void applyCompactSummary(const QString& summary);
    void trimContextForLimit();
    bool shouldCompact() const;
    int estimatedContextTokens() const;
    QJsonArray buildRequestMessages(const QString& userContent) const;
    QJsonObject buildChatRequestBody(const QString& userContent, bool stream) const;
    QNetworkRequest buildNetworkRequest() const;
    void processStreamBuffer();
    void processStreamBlock(const QByteArray& block);
    void finishAssistantMessage();
    void finishWithError(const QString& message);
    void setRunning(bool running);
    void armIdleTimer();
    void stopIdleTimer();
    static QString nextMessageId();
    static QString roleName(AIMessageRole role);
    static QString extractNonStreamContent(const QByteArray& data);

private:
    AIClientConfig m_config;
    AIKnowledgeBase* m_knowledgeBase = nullptr;
    QString m_key;
    QVector<AIChatMessage> m_visibleMessages;
    QVector<AIChatMessage> m_contextMessages;
    QNetworkAccessManager* m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    QTimer* m_idleTimer = nullptr;
    QByteArray m_streamBuffer;
    QString m_currentAssistantId;
    QString m_currentAssistantContent;
    bool m_running = false;
};

#endif // AICHATSESSION_H
