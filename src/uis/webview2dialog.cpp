#include "webview2dialog.h"

#include "../utils/filedownloader.h"
#include "../publicctrls/messagedialog.h"
#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstring>

#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
#  include <Windows.h>
#  include <wrl.h>
#  include <wrl/client.h>
#  include <WebView2.h>
using Microsoft::WRL::ComPtr;

namespace {
QByteArray toNullTerminatedUtf16Le(const QString& text)
{
    QByteArray bytes;
    bytes.resize((text.size() + 1) * static_cast<int>(sizeof(wchar_t)));
    if (text.size() > 0) {
        std::memcpy(bytes.data(), text.utf16(), static_cast<size_t>(text.size()) * sizeof(wchar_t));
    }
    auto* const wide = reinterpret_cast<wchar_t*>(bytes.data());
    wide[text.size()] = L'\0';
    return bytes;
}

void pumpHostedWebView2Messages(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        MSG msg = {};
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
    }
}
} // namespace
#endif

WebView2Dialog::WebView2Dialog(const QString& titleBase, QWidget* parent)
    : FramelessDialog(parent)
    , mTitleBase(titleBase)
{
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1200, 800);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());

    const QString initialTitle =
        titleBase.trimmed().isEmpty() ? QString(tr("Knowledge Base - TransNetPro")) : titleBase;
    setTitleText(initialTitle);

    initBrowserUi();
    QTimer::singleShot(0, this, [this]() { initWebView2(); });
}

WebView2Dialog::~WebView2Dialog()
{
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    shutdownWebView2();
#endif
}

void WebView2Dialog::closeEvent(QCloseEvent* e)
{
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    if (!mCloseAfterShutdown) {
        e->ignore();
        shutdownWebView2();
        mCloseAfterShutdown = true;
        QTimer::singleShot(0, this, [this]() {
            FramelessDialog::close();
        });
        return;
    }
#endif
    FramelessDialog::closeEvent(e);
}

void WebView2Dialog::initBrowserUi()
{
    QVBoxLayout* layout = contentLayout();
    if (!layout) {
        return;
    }

    QWidget* host = layout->parentWidget();

    QFrame* toolbar = new QFrame(host);
    toolbar->setObjectName(QStringLiteral("webView2Toolbar"));
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(QStringLiteral(
        "QFrame#webView2Toolbar {"
        "   background: #083777;"
        "   border: none;"
        "   border-bottom: 1px solid rgba(255,255,255,0.18);"
        "}"));
    QHBoxLayout* toolBarLayout = new QHBoxLayout(toolbar);
    toolBarLayout->setContentsMargins(8, 4, 8, 4);
    toolBarLayout->setSpacing(6);

    mUrlEdit = new QLineEdit(host);
    mUrlEdit->setText(QString());
    mUrlEdit->setReadOnly(false);
    Phase2Ui::applyFieldStyle(mUrlEdit, Phase2Ui::InputFieldTheme::LightCompact);
    QToolButton* refreshBtn = new QToolButton(host);
    refreshBtn->setFixedSize(25, 25);
    refreshBtn->setIconSize(QSize(20, 20));
    refreshBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/refresh.png")));
    refreshBtn->setToolTip(u8"刷新");
    QToolButton* homeBtn = new QToolButton(host);
    homeBtn->setFixedSize(25, 25);
    homeBtn->setIconSize(QSize(20, 20));
    homeBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/home.png")));
    homeBtn->setToolTip(u8"主页");

    toolBarLayout->addWidget(mUrlEdit);
    toolBarLayout->addWidget(homeBtn);
    toolBarLayout->addWidget(refreshBtn);
    toolBarLayout->addSpacing(6);

    mContainer = new QWidget(host);
    mContainer->setAttribute(Qt::WA_NativeWindow, true);
    mContainer->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

    layout->addWidget(toolbar);
    layout->addWidget(mContainer, 1);

    connect(refreshBtn, &QToolButton::clicked, [this]() {
        const QString s = mUrlEdit->text();
        QMetaObject::invokeMethod(this, [this, s]() {
            this->loadUrl(s);
        }, Qt::QueuedConnection);
    });

    connect(homeBtn, &QToolButton::clicked, [this]() {
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
        QMetaObject::invokeMethod(this, [this]() {
            this->loadUrl(mHomeUrl);
        }, Qt::QueuedConnection);
#endif
    });

    connect(mUrlEdit, &QLineEdit::returnPressed, [this]() {
        const QString raw = mUrlEdit->text().trimmed();
        if (raw.isEmpty()) return;
        QUrl target = QUrl::fromUserInput(raw);
        if (target.scheme().isEmpty()) target.setScheme("http");
        QMetaObject::invokeMethod(this, [this, target]() {
            this->loadUrl(target);
        }, Qt::QueuedConnection);
    });
}

void WebView2Dialog::showEvent(QShowEvent* e)
{
    FramelessDialog::showEvent(e);
    updateControllerBounds();
}

void WebView2Dialog::resizeEvent(QResizeEvent* e)
{
    FramelessDialog::resizeEvent(e);
    updateControllerBounds();
}

void WebView2Dialog::updateControllerBounds()
{
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    if (mController) {
        HWND hwnd = reinterpret_cast<HWND>(mContainer->winId());
        RECT rc = {0, 0, 0, 0};
        if (hwnd) {
            ::GetClientRect(hwnd, &rc);
            mController->put_Bounds(rc);
            mController->put_IsVisible(TRUE);
        }
    }
#endif
}

void WebView2Dialog::setTitleBase(const QString& titleBase)
{
    mTitleBase = titleBase;
    if (!titleBase.trimmed().isEmpty()) {
        setTitleText(titleBase);
    }
}

void WebView2Dialog::loadUrl(const QString& url)
{
    if (url.isEmpty()) return;
    QUrl target = QUrl::fromUserInput(url);
    if (target.scheme().isEmpty()) target.setScheme("http");
    loadUrl(target);
}

void WebView2Dialog::loadUrl(const QUrl& url)
{
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    if (mWebView) {
        const QString s = url.toString();
        std::wstring wurl;
        wurl.assign(reinterpret_cast<const wchar_t*>(s.utf16()), s.size());
        const HRESULT hr = mWebView->Navigate(wurl.c_str());
        mUrlEdit->setText(s);
        if (FAILED(hr)) {
            // Optional: log navigation failure for diagnostics
            // qWarning() << "WebView2 Navigate failed" << hex << hr;
        }
    } else {
        mPendingUrl = url;
    }
#else
    Q_UNUSED(url)
#endif
}

void WebView2Dialog::setHomeUrl(const QString& url)
{
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    mHomeUrl = url;
#endif
}

void WebView2Dialog::shutdownWebView2()
{
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    if (mShuttingDown) {
        return;
    }
    mShuttingDown = true;
    disconnect(this);

    if (mController) {
        mController->put_IsVisible(FALSE);
        mController->Close();
        pumpHostedWebView2Messages(400);
    }

    mWebView.Reset();
    mController.Reset();
    mEnv.Reset();
    pumpHostedWebView2Messages(200);

    mBrowserExecutableDirWide.clear();
    mUserDataFolderWide.clear();
#endif
}

void WebView2Dialog::initWebView2()
{
#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    if (mShuttingDown) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(mContainer->winId());
    QPointer<WebView2Dialog> safeThis(this);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString localBrowserDir = QDir::toNativeSeparators(appDir + QStringLiteral("/WebView2"));
    const QString browserDir = QDir(localBrowserDir).exists() ? localBrowserDir : QString();
    const QString userDataDir = QDir::toNativeSeparators(appDir + QStringLiteral("/tnp2_app.exe.WebView2"));
    QDir().mkpath(userDataDir);
    mBrowserExecutableDir = browserDir;
    mUserDataFolder = userDataDir;
    mBrowserExecutableDirWide = toNullTerminatedUtf16Le(browserDir);
    mUserDataFolderWide = toNullTerminatedUtf16Le(userDataDir);
    qInfo() << "WebView2 runtime folder:" << (browserDir.isEmpty() ? QStringLiteral("<default>") : browserDir);
    qInfo() << "WebView2 user data folder:" << userDataDir;
    mWebViewEnvironmentCallbackReceived = false;
    QTimer::singleShot(8000, this, [this, browserDir, userDataDir]() {
        if (!mWebViewEnvironmentCallbackReceived && !mEnv) {
            qWarning() << "WebView2 environment callback timeout. runtime folder:" << browserDir
                       << "user data folder:" << userDataDir;
        }
    });

    auto environmentCompleted =
        [safeThis, hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (!safeThis || safeThis->mShuttingDown) {
                return S_OK;
            }
            safeThis->mWebViewEnvironmentCallbackReceived = true;
            if (FAILED(result) || !env) {
                qWarning() << "CreateCoreWebView2EnvironmentWithOptions failed, HRESULT:"
                           << QStringLiteral("0x%1").arg(static_cast<qulonglong>(result), 8, 16, QLatin1Char('0'));
                return S_OK;
            }
            qInfo() << "CreateCoreWebView2EnvironmentWithOptions succeeded.";
            safeThis->mEnv = env;
            env->CreateCoreWebView2Controller(
                hwnd,
                Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [safeThis](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                        if (!safeThis || safeThis->mShuttingDown) {
                            return S_OK;
                        }
                        if (FAILED(result) || !controller) {
                            qWarning() << "CreateCoreWebView2Controller failed, HRESULT:"
                                       << QStringLiteral("0x%1").arg(static_cast<qulonglong>(result), 8, 16, QLatin1Char('0'));
                            return S_OK;
                        }
                        qInfo() << "CreateCoreWebView2Controller succeeded.";
                        safeThis->mController = controller;
                        ICoreWebView2* webview = nullptr;
                        controller->get_CoreWebView2(&webview);
                        safeThis->mWebView = webview;

                            // Settings
                            ComPtr<ICoreWebView2Settings> settings;
                            webview->get_Settings(&settings);
                            if (settings) {
                                settings->put_IsScriptEnabled(TRUE);
                                settings->put_IsWebMessageEnabled(TRUE);
                                settings->put_AreDefaultContextMenusEnabled(TRUE);
                                settings->put_AreDevToolsEnabled(TRUE);
                            }

                            // Title changed
                            webview->add_DocumentTitleChanged(
                                Microsoft::WRL::Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                                    [safeThis](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
                                        if (!safeThis) {
                                            return S_OK;
                                        }
                                        LPWSTR wtitle = nullptr;
                                        sender->get_DocumentTitle(&wtitle);
                                        if (wtitle) {
                                            const QString title = QString::fromWCharArray(wtitle);
                                            QMetaObject::invokeMethod(safeThis.data(), [safeThis, title]() {
                                                if (!safeThis || safeThis->mShuttingDown) {
                                                    return;
                                                }
                                                const QString base = safeThis->mTitleBase.trimmed().isEmpty()
                                                                         ? QString(tr("Knowledge Base - TransNetPro"))
                                                                         : safeThis->mTitleBase;
                                                safeThis->setTitleText(QStringLiteral("%1 - %2").arg(base, title));
                                            }, Qt::QueuedConnection);
                                            CoTaskMemFree(wtitle);
                                        }
                                        return S_OK;
                                    }
                                ).Get(), nullptr);

                            // URL changed: use NavigationStarting to reflect input
                            webview->add_NavigationStarting(
                                Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
                                    [safeThis](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                        if (!safeThis) {
                                            return S_OK;
                                        }
                                        LPWSTR wuri = nullptr;
                                        args->get_Uri(&wuri);
                                        if (wuri) {
                                            const QString quri = QString::fromWCharArray(wuri);
                                            QMetaObject::invokeMethod(safeThis.data(), [safeThis, quri]() {
                                                if (!safeThis || safeThis->mShuttingDown || !safeThis->mUrlEdit) {
                                                    return;
                                                }
                                                safeThis->mUrlEdit->setText(quri);
                                            }, Qt::QueuedConnection);
                                            CoTaskMemFree(wuri);
                                        }
                                        return S_OK;
                                    }
                                ).Get(), nullptr);

                            // New window requests handled in same view
                            webview->add_NewWindowRequested(
                                Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                    [safeThis](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                        if (!safeThis) {
                                            return S_OK;
                                        }
                                        LPWSTR wuri = nullptr;
                                        args->get_Uri(&wuri);
                                        if (wuri) {
                                            // Copy URI to local storage before freeing COM buffer
                                            const QString quri = QString::fromWCharArray(wuri);
                                            CoTaskMemFree(wuri);
                                            QMetaObject::invokeMethod(safeThis.data(), [safeThis, quri]() {
                                                if (safeThis && !safeThis->mShuttingDown && safeThis->mWebView) {
                                                    safeThis->loadUrl(quri);
                                                }
                                            }, Qt::QueuedConnection);
                                        }
                                        args->put_Handled(TRUE);
                                        return S_OK;
                                    }
                                ).Get(), nullptr);

                            safeThis->updateControllerBounds();

                            // Defer initial navigation to Qt event loop to avoid reentrancy issues
                            const QUrl initial = safeThis->mPendingUrl.isEmpty() ? QUrl("https://www.baidu.com") : safeThis->mPendingUrl;
                            QMetaObject::invokeMethod(safeThis.data(), [safeThis, initial]() {
                                if (safeThis && !safeThis->mShuttingDown) {
                                    safeThis->loadUrl(initial);
                                }
                            }, Qt::QueuedConnection);

                        return S_OK;
                    }
                ).Get());
            return S_OK;
        };

    auto createEnvironment = [this, environmentCompleted](const wchar_t* browserExecutableFolder,
                                                          const QString& sourceLabel) -> HRESULT {
        qInfo() << "Creating WebView2 environment from:" << sourceLabel;
        mWebViewEnvironmentCallbackReceived = false;
        return CreateCoreWebView2EnvironmentWithOptions(
            browserExecutableFolder,
            reinterpret_cast<const wchar_t*>(mUserDataFolderWide.constData()),
            nullptr,
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                environmentCompleted
            ).Get());
    };

    // Try the packaged fixed runtime first. If the folder is not a valid fixed runtime, fall back to
    // the system Evergreen runtime, which is the same path VS debugging normally uses.
    const wchar_t* const browserExecutableFolder =
        mBrowserExecutableDirWide.isEmpty()
            ? nullptr
            : reinterpret_cast<const wchar_t*>(mBrowserExecutableDirWide.constData());
    const HRESULT createEnvironmentHr = createEnvironment(
        browserExecutableFolder,
        browserDir.isEmpty() ? QStringLiteral("system default") : browserDir);
    if (FAILED(createEnvironmentHr)) {
        mWebViewEnvironmentCallbackReceived = true;
        qWarning() << "CreateCoreWebView2EnvironmentWithOptions call failed immediately, HRESULT:"
                   << QStringLiteral("0x%1").arg(static_cast<qulonglong>(createEnvironmentHr), 8, 16, QLatin1Char('0'));
        if (!browserDir.isEmpty()) {
            qInfo() << "Falling back to system WebView2 runtime.";
            const HRESULT fallbackHr = createEnvironment(nullptr, QStringLiteral("system default"));
            if (FAILED(fallbackHr)) {
                mWebViewEnvironmentCallbackReceived = true;
                qWarning() << "System WebView2 fallback failed immediately, HRESULT:"
                           << QStringLiteral("0x%1").arg(static_cast<qulonglong>(fallbackHr), 8, 16, QLatin1Char('0'));
            }
        }
    }
#endif
}

void openWebViewUrlOrWarn(QWidget* parent, const QString& titleBase, const QString& url)
{
    if (url.trimmed().isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
                             QString::fromUtf8(u8"链接未配置或为空，无法打开。"));
        return;
    }

    if (tryOpenWebViewUrl(parent, titleBase, url)) {
        return;
    }

    TnUi::showWarning(QString::fromUtf8(u8"提示"),
                         QString::fromUtf8(u8"当前构建未启用 WebView2，无法打开网页。"));
}

bool tryOpenWebViewUrl(QWidget* parent, const QString& titleBase, const QString& url)
{
    if (url.trimmed().isEmpty()) {
        return false;
    }

#if defined(WEBVIEW2_AVAILABLE) && defined(_WIN32)
    WebView2Dialog* dialog = new WebView2Dialog(titleBase, parent);
    dialog->setTitleBase(titleBase);
    dialog->loadUrl(url);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return true;
#else
    Q_UNUSED(parent)
    Q_UNUSED(titleBase)
    Q_UNUSED(url)
    return false;
#endif
}

namespace {

QString htmlReportSavePath(const QString& url, const QString& defaultFileName)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) {
        dir = QDir::tempPath();
    }
    QDir().mkpath(dir);

    QString fileName = QFileInfo(QUrl(url).path()).fileName();
    if (fileName.isEmpty()) {
        fileName = defaultFileName;
    }
    if (!fileName.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive)
        && !fileName.endsWith(QStringLiteral(".htm"), Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".html");
    }

    const QFileInfo info(fileName);
    return QDir(dir).filePath(QStringLiteral("%1_%2.%3")
                                  .arg(info.completeBaseName(),
                                       QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")),
                                       info.suffix().isEmpty() ? QStringLiteral("html") : info.suffix()));
}

} // namespace

void openHtmlReportWithFallback(QWidget* parent,
                                const QString& title,
                                const QString& reportUrl,
                                const QString& defaultFileName)
{
    const QString trimmedUrl = reportUrl.trimmed();
    if (trimmedUrl.isEmpty()) {
        TnUi::showWarning(title, QString::fromUtf8(u8"接口未返回 HTML 内容"));
        return;
    }

    const QUrl url = QUrl::fromUserInput(trimmedUrl);
    if (!url.isValid() || url.scheme().isEmpty()) {
        TnUi::showWarning(title, QString::fromUtf8(u8"接口返回的 HTML 地址无效。"));
        return;
    }

    QWidget* host = parent ? parent->window() : nullptr;
    if (tryOpenWebViewUrl(host, title, url.toString())) {
        return;
    }

    const QString savePath = htmlReportSavePath(url.toString(), defaultFileName);
    auto* downloader = new FileDownloader(qApp);
    QObject::connect(downloader, &FileDownloader::finished, downloader,
                     [downloader, title](bool success, const QString& filePath, const QString& downloadErr) {
                         if (!success) {
                             const QString message = downloadErr.trimmed().isEmpty()
                                                         ? QString::fromUtf8(u8"下载 HTML 文件失败")
                                                         : downloadErr.trimmed();
                             TnUi::showWarning(title, message);
                             downloader->deleteLater();
                             return;
                         }
                         if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
                             TnUi::showWarning(title,
                                               QString::fromUtf8(u8"HTML 文件已下载，但无法自动打开：\n%1").arg(filePath));
                         }
                         downloader->deleteLater();
                     });
    downloader->download(url.toString(), savePath, QFileInfo(savePath).fileName(), host);
}
