/*****************************************************************//**
 * \file   loginpanel.cpp
 * \brief  登录窗体实现
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "loginpanel.h"

#include "appsession.h"
#include "updatedialog.h"
#include "uiglobalstate.h"
#include "version.h"
#include "../publicctrls/messagedialog.h"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QStyleOption>
#include <QVBoxLayout>

LoginPanel::LoginPanel(QWidget* parent)
    : QWidget(parent)
    , mUserNameEdit(nullptr)
    , mPasswordEdit(nullptr)
    , mLoginButton(nullptr)
    , mStatusLabel(nullptr)
{
    initUi();
}

void LoginPanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    const QPixmap background(QStringLiteral(":/icons/login-background"));
    if (!background.isNull()) {
        const QPixmap scaledBackground = background.scaled(
            size(),
            Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation);
        const QPoint topLeft((width() - scaledBackground.width()) / 2, (height() - scaledBackground.height()) / 2);
        painter.drawPixmap(topLeft, scaledBackground);
        return;
    }

    QStyleOption option;
    option.init(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
}

void LoginPanel::onLoginButtonClicked()
{
    if (mUserNameEdit->text().trimmed().isEmpty()) {
        TnUi::showWarning(u8"警告", u8"请输入用户名。");
        mUserNameEdit->setFocus();
        return;
    }

    if (mPasswordEdit->text().isEmpty()) {
        TnUi::showWarning(u8"警告", u8"请输入密码。");
        mPasswordEdit->setFocus();
        return;
    }

    checkUpdate();

    // 不可删除
    qDebug() << "From Haoyue: hmgyqxxhgxo";
}

void LoginPanel::initUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("loginPanel"));
    setWindowTitle(u8"登录");
    setMinimumWidth(960);
    setMinimumHeight(0);
    setStyleSheet(
        "#loginPanel {"
        "   background-color: #071326;"
        "}"
        "QFrame#loginCard {"
        "   background-color: #ffffff;"
        "   border: none;"
        "   border-radius: 6px;"
        "}"
        "QLabel#logoIcon {"
        "   background-color: #f8fbff;"
        "   border: 1px solid #cfe0fa;"
        "   border-radius: 10px;"
        "}"
        "QLabel#titleLabel {"
        "   color: #1f1f1f;"
        "   font-size: 18px;"
        "   font-weight: 500;"
        "}"
        "QLineEdit {"
        "   min-height: 40px;"
        "   max-height: 40px;"
        "   padding: 0 14px;"
        "   border: none;"
        "   border-radius: 4px;"
        "   color: #333333;"
        "   font-size: 14px;"
        "   background-color: #eaf5ff;"
        "}"
        "QLineEdit:focus {"
        "   background-color: #e5f2ff;"
        "}"
        "QPushButton#loginButton {"
        "   min-height: 40px;"
        "   max-height: 40px;"
        "   border: none;"
        "   border-radius: 4px;"
        "   color: #ffffff;"
        "   background-color: #1687f5;"
        "   font-size: 15px;"
        "   font-weight: 500;"
        "}"
        "QPushButton#loginButton:hover {"
        "   background-color: #0f7fe8;"
        "}"
        "QPushButton#loginButton:disabled {"
        "   background-color: #8ebbe6;"
        "}"
        "QLabel#statusLabel {"
        "   color: #6d7c8f;"
        "   font-size: 12px;"
        "}"
        "QLabel#footerLabel {"
        "   color: #888888;"
        "   font-size: 14px;"
        "}"
    );

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QFrame* card = new QFrame(this);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setFixedSize(420, 440);
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(30, 30, 30, 26);
    cardLayout->setSpacing(0);

    QLabel* logoIcon = new QLabel(card);
    logoIcon->setObjectName(QStringLiteral("logoIcon"));
    logoIcon->setFixedSize(50, 50);
    logoIcon->setAlignment(Qt::AlignCenter);
    logoIcon->setPixmap(QIcon(QStringLiteral(":/icons/icon.ico")).pixmap(32, 32));

    QLabel* titleLabel = new QLabel(u8"城市道路交通网络预测与分析平台", card);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFixedHeight(24);

    mUserNameEdit = new QLineEdit(card);
    mUserNameEdit->setPlaceholderText(u8"请输入账号");
    mUserNameEdit->setText(QStringLiteral("a123"));
    QAction* userAction = new QAction(mUserNameEdit);
    userAction->setIcon(QIcon(QStringLiteral(":/icons/icons/login_user.png")));
    mUserNameEdit->addAction(userAction, QLineEdit::LeadingPosition);

    mPasswordEdit = new QLineEdit(card);
    mPasswordEdit->setPlaceholderText(u8"请输入密码");
    mPasswordEdit->setText(QStringLiteral("123456"));
    mPasswordEdit->setEchoMode(QLineEdit::Password);
    QAction* passwordAction = new QAction(mPasswordEdit);
    passwordAction->setIcon(QIcon(QStringLiteral(":/icons/icons/login_password.png")));
    mPasswordEdit->addAction(passwordAction, QLineEdit::LeadingPosition);

    mLoginButton = new QPushButton(u8"登录", card);
    mLoginButton->setObjectName(QStringLiteral("loginButton"));
    mLoginButton->setCursor(Qt::PointingHandCursor);

    mStatusLabel = new QLabel(card);
    mStatusLabel->setObjectName(QStringLiteral("statusLabel"));
    mStatusLabel->setAlignment(Qt::AlignCenter);
    mStatusLabel->setFixedHeight(16);

    QLabel* footerLabel = new QLabel(u8"中国市政工程西南设计研究总院有限公司", card);
    footerLabel->setObjectName(QStringLiteral("footerLabel"));
    footerLabel->setAlignment(Qt::AlignCenter);
    footerLabel->setFixedHeight(20);

    cardLayout->addWidget(logoIcon, 0, Qt::AlignHCenter);
    cardLayout->addSpacing(18);
    cardLayout->addWidget(titleLabel);
    cardLayout->addSpacing(43);
    cardLayout->addWidget(mUserNameEdit);
    cardLayout->addSpacing(20);
    cardLayout->addWidget(mPasswordEdit);
    cardLayout->addSpacing(30);
    cardLayout->addWidget(mLoginButton);
    cardLayout->addSpacing(15);
    cardLayout->addWidget(mStatusLabel);
    cardLayout->addStretch();
    cardLayout->addWidget(footerLabel);

    rootLayout->addWidget(card, 0, Qt::AlignCenter);

    connect(mLoginButton, &QPushButton::clicked, this, &LoginPanel::onLoginButtonClicked);
    connect(mPasswordEdit, &QLineEdit::returnPressed, this, &LoginPanel::onLoginButtonClicked);
}

void LoginPanel::setBusy(bool busy, const QString& text)
{
    mUserNameEdit->setEnabled(!busy);
    mPasswordEdit->setEnabled(!busy);
    mLoginButton->setEnabled(!busy);
    mLoginButton->setText(busy ? u8"请稍候..." : u8"登录");
    mStatusLabel->setText(text);
}

void LoginPanel::checkUpdate()
{
    setBusy(true, u8"正在检查更新...");

    QPointer<LoginPanel> self(this);
    AppSession::instance().fetchVersion([self](bool suc, const QSharedPointer<VersionResponseJson>& response, const QString& errMsg) {
        if (self.isNull()) {
            return;
        }

        self->setBusy(false);
        if (suc && !response.isNull()) {
            UIG().setVersionInfo(response);
            if (response->j_version().compare(QString::fromLatin1(APP_VERSION)) != 0) {
                self->showUpdateDialog();
            }
            self->login();
            return;
        }

        if (!suc) {
            TnUi::showWarning(u8"获取版本信息失败", errMsg);
        } else {
            TnUi::showWarning(u8"警告", u8"版本信息无效。");
        }
        self->login();
    });
}

void LoginPanel::login()
{
    const QString username = mUserNameEdit->text().trimmed();
    const QString password = mPasswordEdit->text();

    setBusy(true, u8"正在登录...");

    QPointer<LoginPanel> self(this);
    AppSession::instance().login(username, password, [self](bool suc, const QSharedPointer<LoginResponseJson>& response, const QString& errMsg) {
        if (self.isNull()) {
            return;
        }

        if (!suc || response.isNull()) {
            self->setBusy(false);
            TnUi::showWarning(u8"登录失败", suc ? u8"登录信息无效。" : errMsg);
            return;
        }

        self->setBusy(true, u8"正在加载基础配置...");
        AppSession::instance().fetchBaseConfig([self, response](bool cfgSuc,
                                                                  const QSharedPointer<BaseConfigJson>& /*cfg*/,
                                                                  const QString& cfgErr) {
            if (self.isNull()) {
                return;
            }

            self->setBusy(false);
            if (!cfgSuc) {
                TnUi::showWarning(u8"提示",
                                     QString(u8"基础配置加载失败，已使用默认展示。\n%1").arg(cfgErr));
            }

            UIG().setLoginInfo(response);
        });
    });
}

void LoginPanel::showUpdateDialog()
{
    UpdateDialog updateDialog(this);
    updateDialog.exec();
}
