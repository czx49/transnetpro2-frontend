/*****************************************************************//**
 * \file   appwindow.cpp
 * \brief  应用壳窗口实现
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "appwindow.h"

#include "appsession.h"
#include "loginpanel.h"
#include "mainwindow.h"
#include "uiglobalstate.h"

#include <QApplication>
#include <QCloseEvent>
#include <QVariantList>

AppWindow::AppWindow(QWidget* parent)
    : QStackedWidget(parent)
    , mLoginPanel(new LoginPanel(this))
    , mMainWindow(nullptr)
{
    setObjectName(QStringLiteral("appWindow"));
    setWindowTitle(QString::fromUtf8(u8"城市道路交通网络预测与分析平台"));
    addWidget(mLoginPanel);
    setCurrentWidget(mLoginPanel);

    AppSession::instance().fetchFieldChinese();

    connect(&UIG(), &UIGlobalState::uiEventNotify, this, &AppWindow::onUiEventNotify);
    UIG().setCurrentPage(UIGlobalState::PageLogin);
}

void AppWindow::closeEvent(QCloseEvent* event)
{
    if (mMainWindow) {
        mMainWindow->flushRecentProjects();
    }
    event->accept();
    QApplication::quit();
}

void AppWindow::onUiEventNotify(int uiEvent, const QVariantList& params)
{
    Q_UNUSED(params)

    switch (uiEvent) {
    case UIGlobalState::LoginStateChanged:
        if (UIG().isLogin()) {
            UIG().setCurrentPage(UIGlobalState::PageMain);
        } else {
            if (AppSession::instance().isLoggedIn()) {
                AppSession::instance().clearSession();
            }
            UIG().setCurrentPage(UIGlobalState::PageLogin);
        }
        break;
    case UIGlobalState::CurrentPageChanged:
        if (UIG().currentPage() == UIGlobalState::PageMain) {
            showMainPage();
        } else {
            showLoginPage();
        }
        break;
    default:
        break;
    }
}

void AppWindow::showLoginPage()
{
    setCurrentWidget(mLoginPanel);
}

void AppWindow::showMainPage()
{
    if (!mMainWindow) {
        mMainWindow = new MainWindow(this);
        addWidget(mMainWindow);
    }

    setCurrentWidget(mMainWindow);
    mMainWindow->applyWindowTitleFromBaseConfig();
}
