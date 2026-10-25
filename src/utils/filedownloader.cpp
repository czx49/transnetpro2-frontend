#include "filedownloader.h"
#include "../uis/loadingdialog.h"
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QTemporaryFile>
#include <QFileDialog>

FileDownloader::FileDownloader(QObject* parent)
    : QObject(parent), m_manager(new QNetworkAccessManager(this)), m_reply(nullptr), m_file(nullptr), m_loading(nullptr)
{
}

FileDownloader::~FileDownloader()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        if (m_file->isOpen()) m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void FileDownloader::download(const QString& url,
                              const QString& savePath,
                              const QString& suggestedFileName,
                              QWidget* dialogParent)
{
    m_dialogParent = dialogParent;
    m_loading = new LoadingDialog(u8"下载中...", m_dialogParent);
    m_loading->setWindowTitle(u8"文件下载");
    m_loading->showNonBlocking();

    m_savePath = savePath;
    m_url = url;
    m_suggestedFileName = suggestedFileName;

    QUrl qurl(url);
    QNetworkRequest req(qurl);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(0);
#endif
    m_reply = m_manager->get(req);

    // temp file
    QTemporaryFile* tf = new QTemporaryFile(QDir::tempPath() + QDir::separator() + "download-XXXXXX");
    if (!tf->open()) {
        emit finished(false, QString(), u8"创建临时文件失败");
        m_loading->closeDialog();
        delete tf;
        return;
    }
    m_tempFilePath = tf->fileName();
    tf->setAutoRemove(false);
    tf->close();
    delete tf;

    m_file = new QFile(m_tempFilePath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit finished(false, QString(), u8"写入临时文件失败");
        m_loading->closeDialog();
        return;
    }

    connect(m_reply, &QNetworkReply::readyRead, this, &FileDownloader::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &FileDownloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &FileDownloader::onFinished);
    connect(m_reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), this, &FileDownloader::onError);
}

void FileDownloader::onReadyRead()
{
    if (m_file && m_reply) {
        m_file->write(m_reply->readAll());
    }
}

void FileDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_loading) {
        m_loading->setProgress(bytesReceived, bytesTotal);
    }
}

void FileDownloader::onError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code)
    QString err = m_reply ? m_reply->errorString() : u8"未知的网络错误";
    if (m_file) {
        m_file->close();
        m_file->remove();
        delete m_file;
        m_file = nullptr;
    }
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_loading) {
        m_loading->closeDialog();
        m_loading->deleteLater();
        m_loading = nullptr;
    }
    emit finished(false, QString(), err);
}

void FileDownloader::onFinished()
{
    if (!m_reply) return;

    // write remaining data
    if (m_file) {
        m_file->write(m_reply->readAll());
        m_file->flush();
        m_file->close();
    }

    bool ok = true;
    QString finalPath;

    // decide save location
    if (!m_savePath.isEmpty()) {
        // move temp file to savePath
        ok = QFile::remove(m_savePath) ? true : true; // ignore remove errors
        ok = QFile::rename(m_tempFilePath, m_savePath);
        finalPath = m_savePath;
    } else {
        // ask user where to save using suggested filename
        QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QFileInfo fi(m_tempFilePath);
        QString suggested;
        if (!m_suggestedFileName.isEmpty()) {
            suggested = m_suggestedFileName;
        } else {
            // derive filename from URL path (default behavior)
            QUrl u(m_url);
            suggested = QFileInfo(u.path()).fileName();

            if (suggested.isEmpty()) suggested = fi.fileName();
        }
        // show save dialog
        QString saveTo =
            QFileDialog::getSaveFileName(m_dialogParent, u8"保存文件", downloads + QDir::separator() + suggested);
        if (!saveTo.isEmpty()) {
            ok = QFile::rename(m_tempFilePath, saveTo);
            finalPath = saveTo;
        } else {
            // user cancelled, keep temp file and treat as cancelled
            ok = false;
            finalPath = m_tempFilePath;
        }
    }

    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_loading) {
        m_loading->closeDialog();
        m_loading->deleteLater();
        m_loading = nullptr;
    }

    if (ok) {
        emit finished(true, finalPath, QString());
    } else {
        emit finished(false, QString(), u8"保存下载的文件失败");
    }
}
