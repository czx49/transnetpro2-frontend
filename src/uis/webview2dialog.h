#pragma once

#include "framelessdialog.h"
#include <QString>
#include <QUrl>
#include <QWidget>
#include <QLineEdit>

/** URL 非空则打开 WebView2 对话框；为空则提示，不创建对话框 */
void openWebViewUrlOrWarn(QWidget* parent, const QString& titleBase, const QString& url);
/** URL 和 WebView2 可用时打开弹窗；返回 false 表示调用方可执行下载等兜底逻辑 */
bool tryOpenWebViewUrl(QWidget* parent, const QString& titleBase, const QString& url);
/** 将 content URL 优先用 WebView2 打开；不可用时下载 HTML 后用系统浏览器打开 */
void openHtmlReportWithFallback(QWidget* parent,
                                const QString& title,
                                const QString& reportUrl,
                                const QString& defaultFileName);

#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
#  include <QByteArray>
#  include <wrl/client.h>
// Forward declare WebView2 interfaces in global namespace to avoid nested types
struct ICoreWebView2Environment;
struct ICoreWebView2Controller;
struct ICoreWebView2;
#endif

class QToolButton;
class QFrame;

class WebView2Dialog : public FramelessDialog
{
    Q_OBJECT
public:
    explicit WebView2Dialog(const QString& titleBase = QString(), QWidget* parent = nullptr);
    ~WebView2Dialog() override;

    void loadUrl(const QString& url);
    void loadUrl(const QUrl& url);
    void setHomeUrl(const QString& url);
    void setTitleBase(const QString& titleBase);

private:
    void initBrowserUi();
    void initWebView2();
    void shutdownWebView2();
    void updateControllerBounds();

protected:
    void closeEvent(QCloseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    QString mTitleBase;
    QLineEdit* mUrlEdit = nullptr;
    QWidget* mContainer = nullptr; // Native child to host WebView2 via HWND

#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
private:
    // Use ComPtr to manage WebView2 COM interfaces safely
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> mEnv;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> mController;
    Microsoft::WRL::ComPtr<ICoreWebView2> mWebView;
    QUrl mPendingUrl;
    QString mHomeUrl;
    bool mWebViewEnvironmentCallbackReceived = false;
    bool mShuttingDown = false;
    bool mCloseAfterShutdown = false;
    QString mBrowserExecutableDir;
    QString mUserDataFolder;
    /** WebView2 异步创建环境时须保持 UTF-16 路径缓冲区有效直至 mEnv 释放 */
    QByteArray mBrowserExecutableDirWide;
    QByteArray mUserDataFolderWide;
#endif
};
