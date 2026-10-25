#include "qrequest2.h"
#include <mutex>

namespace {

QString networkErrorMessage(QNetworkReply* reply)
{
    if (!reply) {
        return QString::fromUtf8(u8"响应超时");
    }
    const auto err = reply->error();
    if (err == QNetworkReply::OperationCanceledError || err == QNetworkReply::TimeoutError) {
        return QString::fromUtf8(u8"响应超时");
    }
    return reply->errorString();
}

} // namespace

QRequest2::QRequest2(QObject *parent)
    : QObject(parent)
{
}

QRequest2::~QRequest2()
{
}

/**
 * @brief 获取 QRequest2 单例实例
 * @return QRequest2 的单例引用
 * @details 使用 std::once_flag 和 std::call_once 确保线程安全的单例初始化
 *          这种实现在所有 C++ 版本中都是线程安全的
 */
QRequest2 &QRequest2::instance()
{
    static std::once_flag flag;
    static QRequest2 *instance = nullptr;
    std::call_once(flag, []()
                   { instance = new QRequest2(); });
    return *instance;
}

#ifdef ENABLED_QML
void QRequest2::post(const QString &url,
                     QJSValue callbackFunc,
                     const QVariant &param,
                     const QVariantMap &headers,
                     const QVariantMap &rowHeaders)
{
    _post(url, callbackFunc, param, KnownHeaderMap(), rowHeaders, nullptr);
}
#endif

void QRequest2::post(const QString &url,
                     QRequest2CallBack callbackFunc,
                     const QVariant &param,
                     const KnownHeaderMap &headers,
                     const QVariantMap &rawHeaders,
                     QString *error,
                     int transferTimeoutMs)
{
    _post(url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

QRequest2::stHttpResponse QRequest2::postSync(const QString &url,
                                              const QVariant &param,
                                              const KnownHeaderMap &headers,
                                              const QVariantMap &rawHeaders,
                                              QString *error)
{
    QEventLoop loop;
    QRequest2::stHttpResponse res;
    auto callBack = [=, &res, &loop](bool s, const QString &m, const QByteArray &d)
    {
        res.setSuc(s);
        res.setMsg(m);
        res.setData(d);
        if (error)
        {
            *error = m;
        }
        loop.quit();
    };
    _post(url, callBack, param, headers, rawHeaders, error);

    loop.exec();
    return res;
}

#ifdef ENABLED_QML
void QRequest2::postFile(const QString &filepath,
                         const QString &url,
                         QJSValue callbackFunc,
                         const QVariant &param,
                         const QString &filename,
                         const QVariantMap &headers,
                         const QVariantMap &rawHeaders)
{
    _postFile(filepath, url, callbackFunc, filename, KnownHeaderMap(), rawHeaders, nullptr);
}
#endif

void QRequest2::postFile(const QString &filepath,
                         const QString &url,
                         QRequest2CallBack callbackFunc,
                         const QVariant &param,
                         const QString &filename,
                         const KnownHeaderMap &headers,
                         const QVariantMap &rawHeaders,
                         QString *error,
                         int transferTimeoutMs)
{
    _postFile(filepath, url, callbackFunc, filename, headers, rawHeaders, error, transferTimeoutMs);
}

QRequest2::stHttpResponse QRequest2::postFileSync(const QString &filepath,
                                                  const QString &url,
                                                  const QString &filename,
                                                  const KnownHeaderMap &headers,
                                                  const QVariantMap &rawHeaders,
                                                  QString *error)
{
    QEventLoop loop;
    QRequest2::stHttpResponse res;

    auto callBack = [=, &res, &loop](bool s, const QString &m, const QByteArray &d)
    {
        res.setSuc(s);
        res.setMsg(m);
        res.setData(d);
        if (error)
        {
            *error = m;
        }
        loop.quit();
    };
    _postFile(filepath, url, callBack, filename, headers, rawHeaders, error);

    loop.exec();
    return res;
}

#ifdef ENABLED_QML
void QRequest2::get(const QString &url,
                    QJSValue callbackFunc,
                    const QVariant &param,
                    const QVariantMap &headers,
                    const QVariantMap &rawHeaders)
{
    _get(url, callbackFunc, param, KnownHeaderMap(), rawHeaders, nullptr);
}
#endif

void QRequest2::get(const QString &url,
                    QRequest2CallBack callbackFunc,
                    const QVariant &param,
                    const KnownHeaderMap &headers,
                    const QVariantMap &rawHeaders,
                    QString *error,
                    int transferTimeoutMs)
{
    _get(url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

QRequest2::stHttpResponse QRequest2::getSync(const QString &url,
                                             const QVariant &param,
                                             const KnownHeaderMap &headers,
                                             const QVariantMap &rawHeaders,
                                             QString *error)
{
    QEventLoop loop;
    QRequest2::stHttpResponse res;

    auto callBack = [=, &res, &loop](bool s, const QString &m, const QByteArray &d)
    {
        res.setSuc(s);
        res.setMsg(m);
        res.setData(d);
        if (error)
        {
            *error = m;
        }
        loop.quit();
    };
    _get(url, callBack, param, headers, rawHeaders, error);

    loop.exec();
    return res;
}

#ifdef ENABLED_QML
void QRequest2::put(const QString &url,
                    QJSValue callbackFunc,
                    const QVariant &param,
                    const QVariantMap &headers,
                    const QVariantMap &rawHeaders)
{
    _put(url, callbackFunc, param, KnownHeaderMap(), rawHeaders, nullptr);
}
#endif

void QRequest2::put(const QString &url,
                    QRequest2CallBack callbackFunc,
                    const QVariant &param,
                    const KnownHeaderMap &headers,
                    const QVariantMap &rawHeaders,
                    QString *error,
                    int transferTimeoutMs)
{
    _put(url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

QRequest2::stHttpResponse QRequest2::putSync(const QString &url,
                                             const QVariant &param,
                                             const KnownHeaderMap &headers,
                                             const QVariantMap &rawHeaders,
                                             QString *error)
{
    QEventLoop loop;
    QRequest2::stHttpResponse res;

    auto callBack = [=, &res, &loop](bool s, const QString &m, const QByteArray &d)
    {
        res.setSuc(s);
        res.setMsg(m);
        res.setData(d);
        if (error)
        {
            *error = m;
        }
        loop.quit();
    };
    _put(url, callBack, param, headers, rawHeaders, error);

    loop.exec();
    return res;
}

#ifdef ENABLED_QML
void QRequest2::remove(const QString &url,
                       QJSValue callbackFunc,
                       const QVariant &param,
                       const QVariantMap &headers,
                       const QVariantMap &rawHeaders)
{
    _remove(url, callbackFunc, param, KnownHeaderMap(), rawHeaders, nullptr);
}
#endif

void QRequest2::remove(const QString &url,
                       QRequest2CallBack callbackFunc,
                       const QVariant &param,
                       const KnownHeaderMap &headers,
                       const QVariantMap &rawHeaders,
                       QString *error,
                       int transferTimeoutMs)
{
    _remove(url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

QRequest2::stHttpResponse QRequest2::removeSync(const QString &url,
                                                const QVariant &param,
                                                const KnownHeaderMap &headers,
                                                const QVariantMap &rawHeaders,
                                                QString *error)
{
    QEventLoop loop;
    QRequest2::stHttpResponse res;

    auto callBack = [=, &res, &loop](bool s, const QString &m, const QByteArray &d)
    {
        res.setSuc(s);
        res.setMsg(m);
        res.setData(d);
        if (error)
        {
            *error = m;
        }
        loop.quit();
    };
    _remove(url, callBack, param, headers, rawHeaders, error);

    loop.exec();
    return res;
}

#ifdef ENABLED_QML
void QRequest2::patch(const QString &url,
                      QJSValue callbackFunc,
                      const QVariant &param,
                      const QVariantMap &headers,
                      const QVariantMap &rawHeaders)
{
    _patch(url, callbackFunc, param, KnownHeaderMap(), rawHeaders, nullptr);
}
#endif

void QRequest2::patch(const QString &url,
                      QRequest2CallBack callbackFunc,
                      const QVariant &param,
                      const KnownHeaderMap &headers,
                      const QVariantMap &rawHeaders,
                      QString *error,
                      int transferTimeoutMs)
{
    _patch(url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

QRequest2::stHttpResponse QRequest2::patchSync(const QString &url,
                                               const QVariant &param,
                                               const KnownHeaderMap &headers,
                                               const QVariantMap &rawHeaders,
                                               QString *error)
{
    QEventLoop loop;
    QRequest2::stHttpResponse res;

    auto callBack = [=, &res, &loop](bool s, const QString &m, const QByteArray &d)
    {
        res.setSuc(s);
        res.setMsg(m);
        res.setData(d);
        if (error)
        {
            *error = m;
        }
        loop.quit();
    };
    _patch(url, callBack, param, headers, rawHeaders, error);

    loop.exec();
    return res;
}

void QRequest2::_addHeaderContent(QNetworkRequest &request, const KnownHeaderMap &headers, const QVariantMap &rawHeaders)
{
    KnownHeaderMap::const_iterator hItor = headers.begin();
    for (hItor; hItor != headers.end(); hItor++)
    {
        request.setHeader(hItor.key(), hItor.value());
    }
    QVariantMap::const_iterator rhItor = rawHeaders.begin();
    for (rhItor; rhItor != rawHeaders.end(); rhItor++)
    {
        request.setRawHeader(rhItor.key().toUtf8(), rhItor.value().toByteArray());
    }
}

QNetworkReply *QRequest2::_createReplyBase(const QByteArray &verb,
                                           const QString &url,
                                           const QVariant &param,
                                           const KnownHeaderMap &headers,
                                           const QVariantMap &rawHeaders,
                                           QString *error,
                                           int transferTimeoutMs)
{
    qDebug() << "--------------------Restful Request>>>>>>>>>>>>>>>>>>>";
    qDebug() << QString("Method: %1").arg(verb.data());
    qDebug() << QString("Url: %1").arg(url);
    qDebug() << "Param: " << param;
    qDebug() << "Headers: " << headers;
    qDebug() << "RawHeaders: " << rawHeaders;
    qDebug() << "--------------------Restful Request End---------------";
    QNetworkRequest request(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(transferTimeoutMs);
#endif
    _addHeaderContent(request, headers, rawHeaders);
    QJsonDocument doc = QJsonDocument::fromVariant(param);
    QNetworkReply *reply = QRequest2::network()->sendCustomRequest(request, verb, doc.toJson());

    return reply;
}

void QRequest2::_createReply(const QByteArray &verb,
                             const QString &url,
                             QRequest2CallBack callbackFunc,
                             const QVariant &param,
                             const KnownHeaderMap &headers,
                             const QVariantMap &rawHeaders,
                             QString *error,
                             int transferTimeoutMs)
{
    QNetworkReply *reply = _createReplyBase(verb, url, param, headers, rawHeaders, error, transferTimeoutMs);
    _processReply(reply, callbackFunc);
}


void QRequest2::_processReply(QNetworkReply *reply, QRequest2CallBack callbackFunc)
{
    if (reply)
    {
        auto finishCallBack = [=]()
        {
            QByteArray data;
            if (reply->isOpen()) {
                data = reply->readAll();
            }
            qDebug() << "<<<<<<<<<<<<<<<<<<<<<<Restful Response--------------------";
            if (reply->error() == QNetworkReply::NoError)
            {
                callbackFunc(true, "", data);
                qDebug() << "SUCCESS";
                qDebug() << "RESPONSE:" << QString::fromUtf8(data);
            }
            else
            {
                callbackFunc(false, networkErrorMessage(reply), data);
                qDebug() << "FAILED";
                qDebug() << "ERROR:" << reply->error() << reply->errorString();
                qDebug() << "RESPONSE:" << QString::fromUtf8(data);
            }
            reply->deleteLater();
            qDebug() << "--------------------Restful Response End------------------";
        };
        QObject::connect(reply, &QNetworkReply::finished, finishCallBack);
    }
}

#ifdef ENABLED_QML
void QRequest2::_processReply(QNetworkReply *reply, QJSValue callbackFunc)
{
    if (reply)
    {
        auto finishCallback = [reply, callbackFunc]()
        {
            qDebug() << "<<<<<<<<<<<<<<<<<<<<<<<Restful Response--------------------";
            if (callbackFunc.isCallable())
            {
                QJSValue callback = callbackFunc; // 创建本地副本
                if (reply->error() == QNetworkReply::NoError)
                {
                    callback.call({QJSValue(true), QJSValue(""), QJSValue(QString::fromUtf8(reply->readAll()))});
                }
                else
                {
                    callback.call({QJSValue(false), QJSValue(networkErrorMessage(reply)), QJSValue("")});
                }
            }
            reply->deleteLater();
            qDebug() << "--------------------Restful Response End------------------";
        };

        QObject::connect(reply, &QNetworkReply::finished, finishCallback);
    }
}

void QRequest2::_createReply(const QByteArray &verb,
                             const QString &url,
                             QJSValue callbackFunc,
                             const QVariant &param,
                             const KnownHeaderMap &headers,
                             const QVariantMap &rawHeaders,
                             QString *error,
                             int transferTimeoutMs)
{
    QNetworkReply *reply = _createReplyBase(verb, url, param, headers, rawHeaders, error, transferTimeoutMs);
    _processReply(reply, callbackFunc);
}
#endif

QNetworkReply *QRequest2::_createFileReplyBase(const QString &filepath,
                                               const QString &url,
                                               const QString &filename,
                                               const KnownHeaderMap &headers,
                                               const QVariantMap &rawHeaders,
                                               QString *error,
                                               int transferTimeoutMs)
{
    const static QByteArray verb = "POST";
    qDebug() << "--------------------Restful Request(Post file)>>>>>>>>>>>>>>>>>>>";
    qDebug() << QString("Method: %1").arg(verb.data());
    qDebug() << QString("Url: %1").arg(url);
    qDebug() << "Headers: " << headers;
    qDebug() << "File Name: " << filename;
    qDebug() << "File Path: " << filepath << "Size: " << QFileInfo(filepath).size();
    qDebug() << "RawHeaders: " << rawHeaders;
    qDebug() << "--------------------Restful Request End---------------------------";
    if (!QFile::exists(filepath))
    {
        const QString msg = u8"文件路径无效！";
        if (error)
        {
            *error = msg;
        }
        qDebug() << msg;
        return nullptr;
    }
    QFile *file = new QFile(filepath);
    if (!file->open(QIODevice::ReadOnly))
    {
        const QString msg = QString(u8"打开文件失败：") + file->errorString();
        if (error)
        {
            *error = msg;
        }
        qDebug() << msg;
        return nullptr;
    }

    QString _filename = filename;
    if (_filename.isEmpty())
    {
        _filename = QFileInfo(filepath).fileName();
    }
    // 创建多部分表单数据
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    file->setParent(multiPart); // 确保文件在多部分数据删除时被删除

    // 创建文件部分
    QHttpPart filePart;
    filePart.setBodyDevice(file);
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QString("form-data; name=\"file\"; filename=\"%1\"")
                                    .arg(_filename)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("text/csv"));

    // 将文件部分添加到多部分数据中
    multiPart->append(filePart);

    QNetworkRequest request(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(transferTimeoutMs);
#endif
    _addHeaderContent(request, headers, rawHeaders);
    QNetworkReply *reply = QRequest2::network()->post(request, multiPart);
    multiPart->setParent(reply);

    return reply;
}

void QRequest2::_createFileReply(const QString &filepath,
                                 const QString &url,
                                 QRequest2CallBack callbackFunc,
                                 const QString &filename,
                                 const KnownHeaderMap &headers,
                                 const QVariantMap &rawHeaders,
                                 QString *error,
                                 int transferTimeoutMs)
{
    QNetworkReply *reply = _createFileReplyBase(filepath, url, filename, headers, rawHeaders, error, transferTimeoutMs);
    _processReply(reply, callbackFunc);
}

#ifdef ENABLED_QML
void QRequest2::_createFileReply(const QString &filepath,
                                 const QString &url,
                                 QJSValue callbackFunc,
                                 const QString &filename,
                                 const KnownHeaderMap &headers,
                                 const QVariantMap &rawHeaders,
                                 QString *error,
                                 int transferTimeoutMs)
{
    QNetworkReply *reply = _createFileReplyBase(filepath, url, filename, headers, rawHeaders, error, transferTimeoutMs);
    _processReply(reply, callbackFunc);
}
#endif

void QRequest2::_post(const QString &url,
                      QRequest2CallBack callbackFunc,
                      const QVariant &param,
                      const KnownHeaderMap &headers,
                      const QVariantMap &rawHeaders,
                      QString *error,
                      int transferTimeoutMs)
{
    _createReply("POST", url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

#ifdef ENABLED_QML
void QRequest2::_post(const QString &url,
                      QJSValue callbackFunc,
                      const QVariant &param,
                      const KnownHeaderMap &headers,
                      const QVariantMap &rawHeaders,
                      QString *error)
{
    _createReply("POST", url, callbackFunc, param, headers, rawHeaders, error);
}
#endif

void QRequest2::_postFile(const QString &filepath,
                          const QString &url,
                          QRequest2CallBack callbackFunc,
                          const QString &filename,
                          const KnownHeaderMap &headers,
                          const QVariantMap &rawHeaders,
                          QString *error,
                          int transferTimeoutMs)
{
    QString errorStr;
    QNetworkReply *reply = _createFileReplyBase(filepath, url, filename, headers, rawHeaders, &errorStr, transferTimeoutMs);
    _processReply(reply, callbackFunc);
}

#ifdef ENABLED_QML
void QRequest2::_postFile(const QString &filepath,
                          const QString &url,
                          QJSValue callbackFunc,
                          const QString &filename,
                          const KnownHeaderMap &headers,
                          const QVariantMap &rawHeaders,
                          QString *error)
{
    QString errorStr;
    QNetworkReply *reply = _createFileReplyBase(filepath, url, filename, headers, rawHeaders, &errorStr);
    _processReply(reply, callbackFunc);
}

void QRequest2::_get(const QString &url,
                     QJSValue callbackFunc,
                     const QVariant &param,
                     const KnownHeaderMap &headers,
                     const QVariantMap &rawHeaders,
                     QString *error)
{
    _createReply("GET", url, callbackFunc, param, headers, rawHeaders, error);
}

void QRequest2::_put(const QString &url,
                     QJSValue callbackFunc,
                     const QVariant &param,
                     const KnownHeaderMap &headers,
                     const QVariantMap &rawHeaders,
                     QString *error)
{
    _createReply("PUT", url, callbackFunc, param, headers, rawHeaders, error);
}

void QRequest2::_remove(const QString &url,
                        QJSValue callbackFunc,
                        const QVariant &param,
                        const KnownHeaderMap &headers,
                        const QVariantMap &rawHeaders,
                        QString *error)
{
    _createReply("DELETE", url, callbackFunc, param, headers, rawHeaders, error);
}

void QRequest2::_patch(const QString &url,
                       QJSValue callbackFunc,
                       const QVariant &param,
                       const KnownHeaderMap &headers,
                       const QVariantMap &rawHeaders,
                       QString *error)
{
    _createReply("PATCH", url, callbackFunc, param, headers, rawHeaders, error);
}
#endif

void QRequest2::_get(const QString &url,
                     QRequest2CallBack callbackFunc,
                     const QVariant &param,
                     const KnownHeaderMap &headers,
                     const QVariantMap &rawHeaders,
                     QString *error,
                     int transferTimeoutMs)
{
    _createReply("GET", url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

void QRequest2::_put(const QString &url,
                     QRequest2CallBack callbackFunc,
                     const QVariant &param,
                     const KnownHeaderMap &headers,
                     const QVariantMap &rawHeaders,
                     QString *error,
                     int transferTimeoutMs)
{
    _createReply("PUT", url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

void QRequest2::_remove(const QString &url,
                        QRequest2CallBack callbackFunc,
                        const QVariant &param,
                        const KnownHeaderMap &headers,
                        const QVariantMap &rawHeaders,
                        QString *error,
                        int transferTimeoutMs)
{
    _createReply("DELETE", url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

void QRequest2::_patch(const QString &url,
                       QRequest2CallBack callbackFunc,
                       const QVariant &param,
                       const KnownHeaderMap &headers,
                       const QVariantMap &rawHeaders,
                       QString *error,
                       int transferTimeoutMs)
{
    _createReply("PATCH", url, callbackFunc, param, headers, rawHeaders, error, transferTimeoutMs);
}

QNetworkAccessManager *QRequest2::network()
{
    static thread_local QNetworkAccessManager manager;
    return &manager;
}

QString QRequest2::createGetParamString(const QVariantMap &params)
{
    QStringList paramStrings;
    QVariantMap::const_iterator it = params.begin();
    for (it; it != params.end(); ++it)
    {
        paramStrings.append(it.key() + "=" + it.value().toString());
    }

    qDebug() << "paramStrings:" << paramStrings.join("&");
    return paramStrings.join("&");
}

class stHttpResponsePrivate
{
public:
    stHttpResponsePrivate(bool s, const QString &m, const QByteArray &d)
        : suc(s), msg(m), data(d)
    {
    }

public:
    bool suc = false;
    QString msg;
    QByteArray data;
};

QRequest2::stHttpResponse::stHttpResponse(bool s, const QString &m, const QByteArray &d)
    : d_p(new stHttpResponsePrivate(s, m, d))
{
}

QRequest2::stHttpResponse::~stHttpResponse()
{
    if (d_p)
    {
        delete d_p;
        d_p = nullptr;
    }
}

bool QRequest2::stHttpResponse::suc() const
{
    return d_p->suc;
}

void QRequest2::stHttpResponse::setSuc(bool s)
{
    d_p->suc = s;
}

QString QRequest2::stHttpResponse::msg() const
{
    return d_p->msg;
}

void QRequest2::stHttpResponse::setMsg(const QString &msg)
{
    d_p->msg = msg;
}

QByteArray QRequest2::stHttpResponse::data() const
{
    return d_p->data;
}

void QRequest2::stHttpResponse::setData(const QByteArray &d)
{
    d_p->data = d;
}

QRequest2::stHttpResponse::stHttpResponse()
    : d_p(new stHttpResponsePrivate(false, "", ""))
{
}

QRequest2::stHttpResponse::stHttpResponse(const QRequest2::stHttpResponse &other)
    : d_p(new stHttpResponsePrivate(other.d_p->suc, other.d_p->msg, other.d_p->data))
{
}
