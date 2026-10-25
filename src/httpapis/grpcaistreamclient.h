/**
 * @file grpcaistreamclient.h
 * @author zhangmg
 * @date 2026-05-05
 * @brief 普通方案建议 SSE（GET /api/grpc/aistream）
 */
#ifndef GRPCAISTREAMCLIENT_H
#define GRPCAISTREAMCLIENT_H

#include "httpapijson.h"

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

class QTimer;

/**
 * @brief 订阅方案建议类 SSE，事件格式与 GrpcStreamClient 一致
 */
class GrpcAiStreamClient : public QObject {
    Q_OBJECT

public:
    explicit GrpcAiStreamClient(QObject* parent = nullptr);
    ~GrpcAiStreamClient() override;

    void start(int project_id,
               int case_id,
               const QString& type,
               const QString& type2,
               const QString& token,
               const QString& prompt);
    void stop();
    bool isRunning() const;

signals:
    void started();
    void progress(const GrpcStreamEventJson& event);
    void finished(bool success, const QString& errMsg, const QJsonObject& result);

private slots:
    void onReadyRead();
    void onReplyFinished();

private:
    void flushParseBuffer();
    void processSseEventBlock(const QByteArray& block);
    void finishWithError(const QString& errMsg);
    void completeOkAndStop();
    void armTimeoutTimers();
    void stopTimeoutTimers();
    void bumpIdleTimer();

private:
    QNetworkAccessManager* mNam = nullptr;
    QPointer<QNetworkReply> mReply;
    QByteArray mBuffer;
    QJsonObject mLastResult;
    bool mFailEmitted = false;
    bool mCompletedEmitted = false;
    QTimer* mDeadlineTimer = nullptr;
    QTimer* mIdleTimer = nullptr;
};

#endif // GRPCAISTREAMCLIENT_H
