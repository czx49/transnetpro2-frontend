#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QString>

class QWidget;
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class LoadingDialog;

class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader(QObject* parent = nullptr);
    ~FileDownloader() override;

    // 开始下载，savePath 可为空，表示使用默认临时目录并在完成后弹出保存对话框或直接放到下载目录
    void download(const QString& url,
                  const QString& savePath = QString(),
                  const QString& suggestedFileName = QString(),
                  QWidget* dialogParent = nullptr);

signals:
    void finished(bool success, const QString& filePath, const QString& errorMsg);

private slots:
    void onReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onFinished();
    void onError(QNetworkReply::NetworkError code);

private:
    QNetworkAccessManager* m_manager = nullptr;
    QNetworkReply* m_reply = nullptr;
    QFile* m_file = nullptr;
    QString m_tempFilePath;
    LoadingDialog* m_loading = nullptr;
    QString m_savePath;
    QString m_url;
    QString m_suggestedFileName;
    QWidget* m_dialogParent = nullptr;
};

#endif // FILEDOWNLOADER_H
