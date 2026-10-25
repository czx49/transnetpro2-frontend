/*****************************************************************//**
 * \file   appwindow.h
 * \brief  应用壳窗口
 * \details 承载登录页和主工作区，并根据 UI 全局状态切换页面
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef APPWINDOW_H
#define APPWINDOW_H

#include <QStackedWidget>
#include <QVariantList>

class LoginPanel;
class MainWindow;

class AppWindow : public QStackedWidget {
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);
    ~AppWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onUiEventNotify(int uiEvent, const QVariantList& params);

private:
    void showLoginPage();
    void showMainPage();

private:
    LoginPanel* mLoginPanel;
    MainWindow* mMainWindow;
};

#endif // APPWINDOW_H
