/*****************************************************************//**
 * \file   loginpanel.h
 * \brief  登录窗体
 * \details 登录前检查产品升级，登录成功后写入 UI 全局登录态
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef LOGINPANEL_H
#define LOGINPANEL_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

class LoginPanel : public QWidget {
    Q_OBJECT

public:
    explicit LoginPanel(QWidget* parent = nullptr);
    ~LoginPanel() override = default;

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onLoginButtonClicked();

private:
    void initUi();
    void setBusy(bool busy, const QString& text = QString());
    void checkUpdate();
    void login();
    void showUpdateDialog();

private:
    QLineEdit* mUserNameEdit;
    QLineEdit* mPasswordEdit;
    QPushButton* mLoginButton;
    QLabel* mStatusLabel;
};

#endif // LOGINPANEL_H
