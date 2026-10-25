/**
 * @file grpcstreamclient.h
 * @author zhangmg
 * @date 2026-05-05
 * @brief gRPC 算法 SSE 流式接口客户端（GET /api/grpc/stream）
 */
#ifndef GRPCSTREAMCLIENT_H
#define GRPCSTREAMCLIENT_H

#include "httpapijson.h"

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

class QTimer;

/**
 * @brief 订阅算法进度 SSE，解析 `data:` JSON 并发射结构化事件
 */
class GrpcStreamClient : public QObject {
    Q_OBJECT

public:
    explicit GrpcStreamClient(QObject* parent = nullptr);
    ~GrpcStreamClient() override;

    void start(int project_id,
               int case_id,
               int isbase,
               const QString& type,
               const QString& type2,
               const QString& token,
               const QString& ids = QString(),
               const QString& json = QString());
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

#endif // GRPCSTREAMCLIENT_H
