#include "themechange.h"
#include <QApplication>
#include <QtCore>
#include <QtGui>

#define IMAGES_DIR_NAME "images"
#define STYLES_DIR_NAME "styles"
#define FONTS_DIR_NAME "fonts"
#define CONFIG_FILE_NAME "themes.json"


/**
 * @brief 加载指定主题（QSS），支持本地调试与资源加载
 * @param themename 主题名
 *
 * 优先从本地themes目录加载（调试模式），否则从资源文件加载。
 * 加载后自动设置全局样式表。
 */
void ThemeChange::loadTheme(const QString& themename)
{
    QString qsscontent;

#ifdef THEMES_DEBUG_MODE
    const QString styleDir = THEMES_PATH + QString("/themes/") + STYLES_DIR_NAME + "/" + themename;

    if (QDir().exists(styleDir)) {
        qsscontent = loadQSSContentFromDir(styleDir);
    }
#else
    qsscontent = loadQSSContentFromQRC(themename);
#endif
    qApp->setStyleSheet(qsscontent);
}


/**
 * @brief 从本地目录递归加载QSS内容，支持多文件合并
 * @param dirStr 目录路径
 * @return QSS内容
 */
QString ThemeChange::loadQSSContentFromDir(const QString& dirStr)
{
    QDir dir(dirStr);

    qDebug() << QString(u8"从目录加载 QSS 文件：%1").arg(dirStr);

    QString strStyleCont;
    QFileInfoList infoList = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    foreach (QFileInfo fileInfo, infoList) {
        if (fileInfo.isFile()) {
            if (fileInfo.suffix().toLower() == "qss") {
                strStyleCont += readStyleFile(fileInfo.absoluteFilePath());
                qDebug() << QString(u8"加载 QSS 文件：%1").arg(fileInfo.absoluteFilePath());
            }
        } else if (fileInfo.isDir()) {
            if (fileInfo.fileName().toLower() == "backup") {
                continue;
            }
            strStyleCont += loadQSSContentFromDir(fileInfo.absoluteFilePath());
        }
    }
    return strStyleCont;
}


/**
 * @brief 从Qt资源文件递归加载QSS内容，支持多文件合并
 * @param stylename 主题名
 * @return QSS内容
 */
QString ThemeChange::loadQSSContentFromQRC(const QString& stylename)
{
    QString strStyleCont;
    if (!stylename.isEmpty()) {
        // 获取根路径
        QString qrcPath = QString(":/");

        // 创建 QDirIterator 以遍历 qrc 文件
        QDirIterator it(qrcPath, QDirIterator::Subdirectories);

        // 遍历目录，合并所有qss文件
        while (it.hasNext()) {
            QString filePath = it.next();
            if (filePath.startsWith(qrcPath + STYLES_DIR_NAME + "/" + stylename) && filePath.endsWith("qss")) {
                qDebug() << "Resource Path: " << filePath;
                strStyleCont += readStyleFile(filePath);
            }
        }
    }
    return strStyleCont;
}


/**
 * @brief 初始化主题环境，自动加载字体
 */
void ThemeChange::init()
{
    loadFonts();
}


/**
 * @brief 获取所有可用主题信息，解析themes.json配置
 * @return 主题信息列表
 */
ThemeChange::ThemeInfoList ThemeChange::styles()
{
    ThemeChange::ThemeInfoList infos;
#ifdef THEMES_DEBUG_MODE
    QFile file(QString("%1/themes/%2").arg(THEMES_PATH).arg(CONFIG_FILE_NAME));
#else
    QFile file(QString(":/%1").arg(CONFIG_FILE_NAME));
#endif

    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);

        if (QJsonParseError::NoError == error.error) {
            const QJsonArray array = doc.array();
            for (int i = 0; i < array.size(); i++) {
                const QJsonObject tempobj = array[i].toObject();
                const QStringList& keys = tempobj.keys();
                if (!keys.isEmpty()) {
                    infos << ThemeChange::ThemeInfo(keys[0], tempobj.value(keys[0]).toString());
                }
            }
        } else {
            qWarning() << QString(u8"解析配置文件失败：%1").arg(file.fileName());
        }
    } else {
        qWarning() << QString(u8"读取配置文件失败：%1").arg(file.fileName());
    }
    return infos;
}


/**
 * @brief 读取QSS文件内容
 * @param qssfile 文件路径
 * @return QSS内容
 */
QString ThemeChange::readStyleFile(const QString& qssfile)
{
    QFile rfile(qssfile);
    if (!rfile.open(QIODevice::ReadOnly)) {
        qWarning() << QString(u8"加载 QSS 文件失败：%1！%2").arg(qssfile, rfile.errorString());
        return "";
    }
    return rfile.readAll();
}


/**
 * @brief 加载主题所需字体，支持本地和资源两种方式
 *
 * 自动遍历themes/fonts目录或资源，批量注册ttc/ttf/woff字体。
 */
void ThemeChange::loadFonts()
{

#ifdef THEMES_DEBUG_MODE
    QStringList filters;
    filters << "*.ttc"
            << "*.ttf"
            << "*.woff";

    const QString fontsPath = THEMES_PATH + QString("/themes/") + FONTS_DIR_NAME;
    QFileInfoList files = QDir(fontsPath).entryInfoList(filters, QDir::Files);

    Q_FOREACH (QFileInfo info, files) {
        const QString filePath = info.absoluteFilePath();
#else
    // 获取根路径
    QString qrcPath = QString(":/");

    // 创建 QDirIterator 以遍历 qrc 文件
    QDirIterator it(qrcPath, QDirIterator::Subdirectories);

    // 遍历目录，注册所有字体文件
    while (it.hasNext()) {
        const QString filePath = it.next();

        if (!filePath.endsWith("ttc") && !filePath.endsWith("ttf")&& !filePath.endsWith("woff")) {
            continue;
        }
#endif
        const int fontId = QFontDatabase::addApplicationFont(filePath);
        if (-1 == fontId) {
            qWarning() << QString(u8"加载字体文件失败：%1").arg(filePath);
        } else {
            const QString msyh = QFontDatabase::applicationFontFamilies(fontId).at(0);
            qDebug() << QString(u8"字体文件已加载：%1").arg(msyh);
        }
    }
}
