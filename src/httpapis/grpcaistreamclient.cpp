/**
 * @file grpcaistreamclient.cpp
 * @author zhangmg
 * @date 2026-05-05
 * @brief GET /api/grpc/aistream SSE 客户端实现
 */

#include "grpcaistreamclient.h"

#include "httpapidefines.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantMap>

namespace {

constexpr int kMaxStreamMs = 3600000;
constexpr int kIdleTimeoutMs = 900000;

QVariantMap bearerRawHeaders(const QString& token)
{
    QVariantMap rawHeaders;
    if (!token.isEmpty()) {
        rawHeaders[QStringLiteral("Authorization")] = QStringLiteral("Bearer ") + token;
    }
    return rawHeaders;
}

} // namespace

GrpcAiStreamClient::GrpcAiStreamClient(QObject* parent)
    : QObject(parent)
    , mNam(new QNetworkAccessManager(this))
{
    qRegisterMetaType<GrpcStreamEventJson>("GrpcStreamEventJson");

    mDeadlineTimer = new QTimer(this);
    mDeadlineTimer->setSingleShot(true);
    connect(mDeadlineTimer, &QTimer::timeout, this, [this]() {
        finishWithError(QString::fromUtf8(u8"SSE 超时：已达到最长等待时间"));
    });

    mIdleTimer = new QTimer(this);
    mIdleTimer->setSingleShot(true);
    connect(mIdleTimer, &QTimer::timeout, this, [this]() {
        finishWithError(QString::fromUtf8(u8"SSE 超时：长时间未收到数据"));
    });
}

GrpcAiStreamClient::~GrpcAiStreamClient()
{
    stop();
}

void GrpcAiStreamClient::armTimeoutTimers()
{
    if (mDeadlineTimer) {
        mDeadlineTimer->start(kMaxStreamMs);
    }
    bumpIdleTimer();
}

void GrpcAiStreamClient::stopTimeoutTimers()
{
    if (mDeadlineTimer) {
        mDeadlineTimer->stop();
    }
    if (mIdleTimer) {
        mIdleTimer->stop();
    }
}

void GrpcAiStreamClient::bumpIdleTimer()
{
    if (mIdleTimer) {
        mIdleTimer->start(kIdleTimeoutMs);
    }
}

void GrpcAiStreamClient::start(int project_id,
                               int case_id,
                               const QString& type,
                               const QString& type2,
                               const QString& token,
                               const QString& prompt)
{
    stop();
    mFailEmitted = false;
    mCompletedEmitted = false;
    mLastResult = QJsonObject();

    QUrl url(fullURL(API_GRPC_AISTREAM));
    QUrlQuery query;
    query.addQueryItem("project_id", QString::number(project_id));
    query.addQueryItem("case_id", QString::number(case_id));
    query.addQueryItem("type", type);
    query.addQueryItem("type2", type2);
    query.addQueryItem("token", token);
    query.addQueryItem("prompt", prompt);
    url.setQuery(query);

    QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(0);
#endif
    req.setRawHeader("Accept", "text/event-stream");
    req.setRawHeader("Cache-Control", "no-cache");

    const QVariantMap rawHeaders = bearerRawHeaders(token);
    for (auto it = rawHeaders.constBegin(); it != rawHeaders.constEnd(); ++it) {
        req.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
    }

    mReply = mNam->get(req);
    connect(mReply, &QNetworkReply::readyRead, this, &GrpcAiStreamClient::onReadyRead);
    connect(mReply, &QNetworkReply::finished, this, &GrpcAiStreamClient::onReplyFinished);

    armTimeoutTimers();
    emit started();
}

void GrpcAiStreamClient::stop()
{
    stopTimeoutTimers();
    if (mReply) {
        disconnect(mReply, nullptr, this, nullptr);
        mReply->abort();
        mReply->deleteLater();
        mReply.clear();
    }
    mBuffer.clear();
}

bool GrpcAiStreamClient::isRunning() const
{
    return mReply && !mReply->isFinished();
}

void GrpcAiStreamClient::onReadyRead()
{
    if (!mReply) {
        return;
    }

    mBuffer.append(mReply->readAll());
    flushParseBuffer();
}

void GrpcAiStreamClient::flushParseBuffer()
{
    while (true) {
        const int sep = mBuffer.indexOf("\n\n");
        if (sep < 0) {
            break;
        }

        const QByteArray block = mBuffer.left(sep);
        mBuffer.remove(0, sep + 2);
        processSseEventBlock(block);
        if (mFailEmitted || mCompletedEmitted) {
            break;
        }
    }
}

void GrpcAiStreamClient::processSseEventBlock(const QByteArray& block)
{
    const QString text = QString::fromUtf8(block);
    const QStringList lines = text.split(QLatin1Char('\n'));
    QStringList dataParts;
    for (const QString& line : lines) {
        if (line.startsWith(QLatin1String("data:"))) {
            QString payload = line.mid(5);
            if (payload.startsWith(QLatin1Char(' '))) {
                payload = payload.mid(1);
            }
            dataParts.append(payload);
        }
    }

    if (dataParts.isEmpty()) {
        return;
    }

    const QString jsonText = dataParts.join(QLatin1Char('\n'));
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject eventObj = root;
    if (root.contains(QStringLiteral("data"))) {
        const QJsonValue v = root.value(QStringLiteral("data"));
        if (v.isObject()) {
            eventObj = v.toObject();
        }
    }

    const GrpcStreamEventJson ev(eventObj);
    emit progress(ev);
    bumpIdleTimer();

    const int code = ev.j_code();
    if (code < 0) {
        finishWithError(ev.j_message().isEmpty() ? QStringLiteral("stream error") : ev.j_message());
        return;
    }

    const QJsonValue rv = ev.j_result();
    if (rv.isObject()) {
        const QJsonObject ro = rv.toObject();
        if (!ro.isEmpty()) {
            mLastResult = ro;
        }
    }

    if (code == 1 && ev.j_data().j_done() == 1) {
        completeOkAndStop();
    }
}

void GrpcAiStreamClient::completeOkAndStop()
{
    if (mCompletedEmitted || mFailEmitted) {
        return;
    }
    mCompletedEmitted = true;
    stopTimeoutTimers();
    emit finished(true, QString(), mLastResult);
    if (mReply) {
        disconnect(mReply, nullptr, this, nullptr);
        mReply->abort();
        mReply->deleteLater();
        mReply.clear();
    }
    mBuffer.clear();
}

void GrpcAiStreamClient::finishWithError(const QString& errMsg)
{
    if (mFailEmitted) {
        return;
    }
    mFailEmitted = true;
    stopTimeoutTimers();
    emit finished(false, errMsg, QJsonObject());
    stop();
}

void GrpcAiStreamClient::onReplyFinished()
{
    if (!mReply) {
        return;
    }

    mBuffer.append(mReply->readAll());
    flushParseBuffer();

    if (!mBuffer.trimmed().isEmpty() && !mFailEmitted && !mCompletedEmitted) {
        processSseEventBlock(mBuffer);
    }
    mBuffer.clear();

    const QNetworkReply::NetworkError nerr = mReply->error();
    const QString httpErr = mReply->errorString();
    mReply->deleteLater();
    mReply.clear();

    stopTimeoutTimers();

    if (mFailEmitted || mCompletedEmitted) {
        return;
    }

    if (nerr != QNetworkReply::NoError && nerr != QNetworkReply::OperationCanceledError) {
        emit finished(false, httpErr, QJsonObject());
        return;
    }

    if (nerr == QNetworkReply::OperationCanceledError) {
        emit finished(false, QStringLiteral("canceled"), QJsonObject());
        return;
    }

    emit finished(true, QString(), mLastResult);
}
