/*****************************************************************//**
 * \file   updatedialog.cpp
 * \brief  产品升级对话框实现
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "updatedialog.h"

#include "filedownloader.h"
#include "ui_updatedialog.h"
#include "uiglobalstate.h"
#include "version.h"

#include "../publicctrls/messagedialog.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>

UpdateDialog::UpdateDialog(QWidget* parent)
    : FramelessDialog(parent)
    , m_contentWidget(new QWidget(this))
    , ui(new Ui::UpdateDialog)
{
    ui->setupUi(m_contentWidget);
    setModal(true);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    setTitleText(QString::fromUtf8(u8"发现新版本"));
    resize(520, 420);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + Phase2Ui::dialogLightContentPanelStyleSheet(QStringLiteral("updateContentEdit")));
    ui->verticalLayout->setContentsMargins(20, 12, 20, 20);
    contentLayout()->addWidget(m_contentWidget);

    ui->label_title->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffffff; font-size: 18px; font-weight: 700; background: transparent; }"));
    ui->label_info->setStyleSheet(QStringLiteral(
        "QLabel { color: #cbd5e1; font-size: 14px; background: transparent; }"));
    ui->label->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffffff; font-size: 14px; font-weight: 700; background: transparent; }"));
    ui->textEdit_content->setObjectName(QStringLiteral("updateContentEdit"));
    ui->btn_update->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    ui->btn_ignore->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());

    setUpdateInfo();

    connect(ui->btn_update, &QPushButton::clicked, this, &UpdateDialog::slotBtn_update_clicked);
    connect(ui->btn_ignore, &QPushButton::clicked, this, &UpdateDialog::slotBtn_ignore_clicked);
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}

void UpdateDialog::slotBtn_update_clicked()
{
    const QSharedPointer<VersionResponseJson> versionInfo = UIG().versionInfo();
    if (versionInfo.isNull()) {
        TnUi::showWarning(u8"警告", u8"版本信息无效。");
        return;
    }

    const QString downloadUrl = versionInfo->j_url();
    if (downloadUrl.isEmpty()) {
        TnUi::showWarning(u8"警告", u8"服务器返回的下载链接为空。");
        return;
    }

    QString fileName = versionInfo->j_fileName();
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("TransNetPro2-%1.msi").arg(versionInfo->j_version());
    }

    const QString savePath = QDir::toNativeSeparators(QDir::tempPath() + QDir::separator() + fileName);
    setUpdating(true);

    FileDownloader* downloader = new FileDownloader(this);
    connect(downloader, &FileDownloader::finished, this, [this, downloader, savePath, versionInfo](bool success, const QString& filePath, const QString& errMsg) {
        downloader->deleteLater();

        if (!success) {
            setUpdating(false);
            TnUi::showWarning(u8"错误", QString(u8"下载失败：%1").arg(errMsg));
            return;
        }

        if (!checkFileMd5(savePath, versionInfo->j_md5())) {
            setUpdating(false);
            TnUi::showCritical(u8"错误", u8"文件 MD5 校验失败，文件可能已损坏。");
            return;
        }

        TnUi::showInformation(u8"下载", QString(u8"已下载到 %1").arg(filePath));

        QStringList arguments;
        arguments << QStringLiteral("/i") << savePath;
        if (!QProcess::startDetached(QStringLiteral("msiexec"), arguments)) {
            setUpdating(false);
            TnUi::showCritical(u8"错误", u8"启动安装程序失败。");
            return;
        }

        QApplication::quit();
    });

    downloader->download(downloadUrl, savePath, fileName);
}

void UpdateDialog::slotBtn_ignore_clicked()
{
    reject();
}

void UpdateDialog::setUpdateInfo()
{
    const QSharedPointer<VersionResponseJson> versionInfo = UIG().versionInfo();
    const QString newVersion = versionInfo.isNull() ? QString() : versionInfo->j_version();
    const QString fileSize = versionInfo.isNull() ? QStringLiteral("0B") : formatSize(versionInfo->j_size());
    const QString updateContent = versionInfo.isNull() ? QString() : versionInfo->j_desc();

    setTitleText(QString::fromUtf8(u8"发现新版本 %1").arg(newVersion));
    ui->label_title->setText(QString::fromUtf8(u8"发现新版本 %1").arg(newVersion));
    ui->label_info->setText(QString(u8"当前版本：%1\n最新版本：%2\n更新大小：%3")
                                .arg(QString::fromLatin1(APP_VERSION))
                                .arg(newVersion)
                                .arg(fileSize));
    ui->textEdit_content->setPlainText(updateContent);
}

void UpdateDialog::setUpdating(bool updating)
{
    ui->btn_update->setEnabled(!updating);
    ui->btn_ignore->setEnabled(!updating);
    ui->btn_update->setText(updating ? u8"正在更新..." : u8"立即更新");
}

QString UpdateDialog::formatSize(qint64 bytes) const
{
    if (bytes <= 0) {
        return QStringLiteral("0B");
    }

    const QStringList units = {
        QStringLiteral("B"),
        QStringLiteral("KB"),
        QStringLiteral("MB"),
        QStringLiteral("GB"),
        QStringLiteral("TB")
    };
    int level = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && level < units.size() - 1) {
        size /= 1024.0;
        ++level;
    }

    return QStringLiteral("%1%2").arg(QString::number(size, 'f', level == 0 ? 0 : 1), units.at(level));
}

QString UpdateDialog::getFileMd5(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!hash.addData(&file)) {
        return QString();
    }

    return QString::fromLatin1(hash.result().toHex().toUpper());
}

bool UpdateDialog::checkFileMd5(const QString& filePath, const QString& targetMd5) const
{
    if (targetMd5.isEmpty()) {
        return true;
    }

    return getFileMd5(filePath) == targetMd5.toUpper();
}
