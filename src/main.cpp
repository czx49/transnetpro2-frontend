/*****************************************************************//**
 * \file   main.cpp
 * \brief  Phase2程序入口
 * \details 初始化Qt、QGIS、日志系统，启动主窗口
 *
 * \author zhangmg
 * \date   April 2026
 * \History 初始版本
 *********************************************************************/
#include "appwindow.h"
#include "httpapis/backendproxyfactory.h"
#include "httpapis/httpserverconfig.h"
#include "casectrls/casewfsmapcontroller.h"
#include "qgsapplication.h"
#include "qgseditorwidgetregistry.h"
#include "qgsgui.h"
#include "qgsmessagelog.h"
#include "qgslogger.h"
#include "qgsproject.h"
#include "qgsnetworkaccessmanager.h"

#include <QApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QDir>
#include <QLoggingCategory>
#include <QProcessEnvironment>

// log4qt
#include "logprinter_ex.h"
#include "log4qt/logger.h"
#include <log4qt/logmanager.h>
#include <log4qt/propertyconfigurator.h>

#ifdef WIN32
#include <Windows.h>
#endif // WIN32

void setupEnvironment();
void setupQGISlog();
void setupQtLogFilters();
void setupApplicationFont();

 QStringList g_fontfamilies;
int main(int argc, char* argv[])
{
#ifdef WIN32
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    SetConsoleOutputCP(65001); // utf-8 code page
    for(const char* key:{"PROJ_LIB","PROJ_DATA","GDAL_DATA"}) {const auto value=qEnvironmentVariable(key);if(!value.isEmpty())qputenv(key,value.toUtf8());}
#endif // !WIN32

    // Enable Qt high DPI support before creating application
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // Avoid unexpected rounding on scale factors
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

#ifdef WIN32
    // Try to set process DPI awareness to Per-Monitor V2 if available (Windows 10+), fallback to SetProcessDPIAware
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContext_t)(HANDLE);
        SetProcessDpiAwarenessContext_t pSetDpiAwareness = reinterpret_cast<SetProcessDpiAwarenessContext_t>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (pSetDpiAwareness) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is defined as (HANDLE)-4
            pSetDpiAwareness((HANDLE)-4);
        } else {
            // Older Windows -- mark process as system DPI aware
            SetProcessDPIAware();
        }
    }
#endif

    QgsApplication a(argc, argv, true);
    setupEnvironment();
    QgsApplication::initQgis();
    const QUrl backendUrl(HttpServerConfig::instance().hostName());
    QNetworkProxyFactory::setApplicationProxyFactory(new BackendProxyFactory(backendUrl));
    // QGIS has a separate proxy chain. Return no decision for unrelated destinations.
    QgsNetworkAccessManager::instance()->insertProxyFactory(
        new BackendProxyFactory(backendUrl, [](const QNetworkProxyQuery&) {
            return QList<QNetworkProxy>();
        }));
    // 属性表内联编辑依赖编辑器工厂；未 initEditors 时 createEditor 会空指针崩溃
    QgsGui::editorWidgetRegistry()->initEditors(nullptr);
    CaseWfsMap::setupWfsProviderCachingPolicy();
    setupQGISlog();
    const QIcon appIcon(QStringLiteral(":/icons/icon.ico"));
    QApplication::setWindowIcon(appIcon);
    a.setWindowIcon(appIcon);

    // 将当前工作目录切换到可执行程序所在目录
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    const bool suc = Log4Qt::PropertyConfigurator::configure(QCoreApplication::applicationDirPath() + "/log4qt.properties");
    if (!suc) {
        qFatal("Log4Qt configuration failed");
    }
    setupQtLogFilters();
    setupApplicationFont();

    // 设置坐标系为 WGS84 (EPSG:4326)
    QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem("EPSG:4326"));

    int result=0;
    {
        AppWindow appWindow;
        appWindow.setWindowIcon(appIcon);
        appWindow.showMaximized();
        result=a.exec();
    }
    CaseWfsMap::prepareForShutdown();
    QgsProject::instance()->removeAllMapLayers();
    // 清理 QGIS 资源
    QgsApplication::exitQgis();
    return result;
}

void setupEnvironment()
{
    const QString prefix=qEnvironmentVariable("TNP_QGIS_PREFIX");
    if(!prefix.isEmpty()) {
        QgsApplication::setPrefixPath(prefix,true);QgsApplication::setPluginPath(prefix+"/plugins");QgsApplication::setPkgDataPath(prefix);
        const QString auth = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("data/qgis_auth");
        QDir().mkpath(auth);QgsApplication::setAuthDatabaseDirPath(auth);return;
    }
    QString appDir = QApplication::applicationDirPath();
    // 兼容启动器模式：若运行在 depends 目录下，向上修正基准目录
    QDir dir(appDir);
    if (dir.dirName() == "depends") {
        dir.cdUp();
        appDir = dir.absolutePath();
    }

    const QString imagesDir = appDir + QStringLiteral("/images");
    if (!QDir().mkpath(imagesDir)) {
        qWarning() << QString(u8"创建 images 目录失败：%1").arg(imagesDir);
    }

    QString projDir = appDir + "/../share/proj";
    if (QDir(projDir).exists())
    {
        qunsetenv("PROJ_LIB");
        qputenv("PROJ_LIB", projDir.toUtf8());
    }

    // 设置 Qt 插件搜索路径，确保能找到 crypto/qca-ossl 等插件
    // 优先搜索应用程序目录下的 plugins 文件夹
    QString localPlugins = appDir + "/depends";
    if (QDir(localPlugins).exists()) {
        QCoreApplication::addLibraryPath(localPlugins);
    }

    // 包根 plugins：Qt 插件（platforms/imageformats/crypto 等）
    QString packageQtPlugins = appDir + "/../plugins";
    if (QDir(packageQtPlugins).exists()) {
        QCoreApplication::addLibraryPath(packageQtPlugins);
    }

    // Layout A：QGIS providers/resources 与 exe 同级（bin/）。
    // 禁止 setPrefixPath(appDir, true)：Windows 上会 chop 掉尾部 /bin，误指到包根。
    QgsApplication::setPrefixPath(appDir, false);
    QgsApplication::setPluginPath(appDir + QStringLiteral("/plugins"));
    QgsApplication::setPkgDataPath(appDir);

#ifdef WIN32
#ifdef WEBVIEW2_AVAILABLE
    // 添加子目录 lib 到 PATH
    QString libPath = appDir + "/WebView2";
    QString newPath = libPath + ";" + QString::fromUtf8(qgetenv("PATH"));

    // 设置新的 PATH 环境变量
    qputenv("PATH", newPath.toUtf8());
#endif
#endif
}

void setupQGISlog()
{
    static auto convertLevel = [](Qgis::MessageLevel qgsLevel) -> LogStreamBuffer::Level{
        switch (qgsLevel)
        {
        case Qgis::Info:
            return LogStreamBuffer::Level::LogLevelInfo;
        case Qgis::Warning:
            return LogStreamBuffer::Level::LogLevelWarn;
        case Qgis::Critical:
            return LogStreamBuffer::Level::LogLevelError;
        case Qgis::Success:
        case Qgis::NoLevel:
        default:
            return LogStreamBuffer::Level::LogLevelTrace;
        }
    };

    QObject::connect(
        QgsApplication::messageLog(),
        static_cast<void (QgsMessageLog::*)(const QString&, const QString&, Qgis::MessageLevel)>(&QgsMessageLog::messageReceived),
        QgsApplication::instance(),
        [=](const QString& message, const QString& tag, Qgis::MessageLevel level) {
            auto l = convertLevel(level);
            LogStreamBuffer logStream("Qgis", l);
            logStream << message;
        }
    );
}

void setupQtLogFilters()
{
    // Qt 5 在 Windows 上监控部分虚拟/断开的网卡时可能输出该告警，
    // 不影响 QNetworkAccessManager 的实际请求，过滤掉避免误导用户。
    QLoggingCategory::setFilterRules(QStringLiteral("qt.network.monitor.warning=false"));
}

void setupApplicationFont()
{
    const QString fontPath = QStringLiteral(":/fonts/fonts/AlibabaPuHuiTi-3-55-Regular.ttf");
    // 第一步：先检查资源文件是否存在
    QFile fontFile(fontPath);
    if (!fontFile.exists())
    {
        qWarning() << u8"资源文件不存在！路径：" << fontPath;
        return;
    }

    int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId < 0)
    {
        qWarning() << u8"加载阿里巴巴普惠体失败, fontId:" << fontId;
        // 打印系统已加载字体，对比字体家族名
        QFontDatabase db;
        QStringList allFonts = db.families();// applicationFontFamilies();
        qDebug() << u8"当前系统可用字体：" << allFonts;
        return;
    }

    g_fontfamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (g_fontfamilies.isEmpty()) {
        qWarning() << u8"阿里巴巴普惠体未包含可用字体族";
        return;
    }

    // 获取加载成功后的字体家族名称（关键！设置字体要用这个名字）
    QString familyName = g_fontfamilies.first();
    qDebug() << u8"字体加载成功，字体家族名：" << familyName;

    QFont globalFont(familyName);
    globalFont.setPixelSize(14);
    qApp->setFont(globalFont);

    qApp->setStyleSheet(QStringLiteral(
        "* {\n"
        "    font-family: \"%1\";\n"
        "}\n"
        "QWidget {\n"
        "    font-family: \"%1\";\n"
        "}\n").arg(familyName));
}
