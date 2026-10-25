/*****************************************************************//**
 * \file   aichatwidget.h
 * \brief  工作台流程步骤智能对话展示控件
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef AICHATWIDGET_H
#define AICHATWIDGET_H

#include "aiclient/aimessage.h"

#include <QHash>
#include <QMetaObject>
#include <QResizeEvent>
#include <QSharedPointer>
#include <QList>
#include <QStringList>
#include <QWidget>
#include <QVector>

class AIChatSession;
class AiConfigModel;
class AiWordSelectBar;
class GrpcStreamEventJson;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

/**
 * \brief 设计稿中的可折叠智能对话卡片，供每个流程节点独立持有
 */
class AIChatWidget : public QWidget {
    Q_OBJECT

public:
    explicit AIChatWidget(QWidget* parent = nullptr);

    void setSession(const QSharedPointer<AIChatSession>& session);
    QSharedPointer<AIChatSession> session() const;
    void clear();
    void appendPlainLine(const QString& line);
    void appendSystemLine(const QString& line);
    void appendGrpcEvent(const GrpcStreamEventJson& event);
    void setIndeterminateBusy(bool busy);
    void setPercent(int percent);
    void setCollapsed(bool collapsed);
    bool isCollapsed() const;
    void setTitleText(const QString& title);
    void setUseBackendAiApi(bool enabled);
    bool useBackendAiApi() const;
    void setInputBusy(bool busy);
    void setAiConfig(const AiConfigModel* config);
    AiWordSelectBar* wordSelectBar() const;
    void setWordSelection(int l1Index, int l2Index);
    void setCommonQuestions(const QStringList& questions);
    QString commonQuestion() const;
    /** @brief 在 AI 头像侧追加助手气泡（同步回复） */
    void appendAssistantReply(const QString& text);
    /** @brief 显示 AI「正在思考…」占位气泡，返回 messageId 供 finishAssistantReply 更新 */
    QString beginAssistantReply();
    void finishAssistantReply(const QString& messageId,
                              const QString& content,
                              bool isError = false,
                              const QString& downloadUrl = QString());
    /** @brief 按时间线顺序恢复全部消息（对话 + 运行日志） */
    void restoreChatTimeline(const QList<AIChatMessage>& messages);
    void appendTimelineMessage(const AIChatMessage& message, bool emitDialogSignals = false);
    void relayoutBubbles();

signals:
    void promptSubmitted(const QString& text, int l1Index, int l2Index, const QString& commonQuestion);
    void dialogMessageAdded(const AIChatMessage& message);
    void dialogMessageUpdated(const QString& messageId,
                              const QString& content,
                              bool isError,
                              const QString& downloadUrl);
    void downloadLinkActivated(const QString& url);

private slots:
    void toggleCollapsed();
    void onSendClicked();
    void showCommonQuestionMenu();
    void onBubbleLinkActivated(const QString& link);
    void onSessionMessageAdded(const AIChatMessage& message);
    void onSessionMessageDelta(const QString& messageId, const QString& delta, const QString& fullContent);
    void onSessionRunningChanged(bool running);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void disconnectSession();
    void renderSessionMessages();
    void appendMessage(const AIChatMessage& message, bool emitDialogSignals);
    void appendLogLine(const QString& line, bool warning = false);
    void scrollToBottom();
    void scheduleRelayoutAndScroll();
    void configureAssistantBubble(QLabel* bubble);
    void setCommonQuestion(const QString& question);
    void updateCommonQuestionButton();

private:
    QLabel* m_titleLabel;
    QPushButton* m_toggleButton;
    QWidget* m_bodyWidget;
    QScrollArea* m_scrollArea;
    QWidget* m_messageListWidget;
    QVBoxLayout* m_messageLayout;
    AiWordSelectBar* m_wordSelectBar;
    QToolButton* m_commonQuestionButton;
    QMenu* m_commonQuestionMenu;
    QLineEdit* m_inputEdit;
    QPushButton* m_sendButton;
    bool m_collapsed;
    bool m_batchRestoring = false;
    int m_relayoutRetryCount = 0;
    bool m_useBackendAiApi = false;
    bool m_inputBusy = false;
    QStringList m_commonQuestions;
    QString m_commonQuestion;
    QSharedPointer<AIChatSession> m_session;
    QVector<QMetaObject::Connection> m_sessionConnections;
    QHash<QString, QLabel*> m_messageLabels;
    QHash<QString, QString> m_messageDownloadUrls;
};

#endif // AICHATWIDGET_H
