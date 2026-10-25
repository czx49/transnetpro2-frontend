#include "appsession.h"

#include "singleton.h"

#include <QJsonObject>

AppSession& AppSession::instance()
{
    return Singleton<AppSession>::instance();
}

AppSession::AppSession(QObject* parent)
    : QObject(parent)
{
}

void AppSession::login(const QString& username,
                       const QString& password,
                       const HttpApis::ResponseCallBack<LoginResponseJson>& callback)
{
    HttpApis::login(username, password, [this, callback](bool suc, const QSharedPointer<LoginResponseJson>& response, const QString& errMsg) {
        if (suc && !response.isNull()) {
            // 登录成功后统一写入会话缓存，避免业务层重复维护 token / user 信息。
            mLoginInfo = response;
            mToken = response->j_token();
            emit loginStateChanged();
        }

        callback(suc, response, errMsg);
    });
}

void AppSession::fetchBaseConfig(const HttpApis::ResponseCallBack<BaseConfigJson>& callback)
{
    HttpApis::getBaseConfig([this, callback](bool suc, const QSharedPointer<BaseConfigJson>& response, const QString& errMsg) {
        if (suc && !response.isNull()) {
            // 基础配置是多处会复用的全局只读数据，成功后直接覆盖缓存。
            mBaseConfig = response;
            emit baseConfigChanged();
        }

        callback(suc, response, errMsg);
    });
}

void AppSession::fetchVersion(const HttpApis::ResponseCallBack<VersionResponseJson>& callback)
{
    HttpApis::getVersion([this, callback](bool suc, const QSharedPointer<VersionResponseJson>& response, const QString& errMsg) {
        if (suc && !response.isNull()) {
            mVersionInfo = response;
            emit versionInfoChanged();
        }

        callback(suc, response, errMsg);
    });
}

void AppSession::fetchFieldChinese(const HttpApis::ResponseCallBack<PropertyJson>& callback)
{
    HttpApis::getFieldChinese([this, callback](bool suc, const QSharedPointer<PropertyJson>& response, const QString& errMsg) {
        if (suc && !response.isNull()) {
            QHash<QString, QString> nextMap;
            const QJsonObject obj = response->jsonObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const QString cn = it.value().toString().trimmed();
                if (!cn.isEmpty()) {
                    nextMap.insert(it.key(), cn);
                }
            }
            if (!nextMap.isEmpty()) {
                mFieldChineseMap = std::move(nextMap);
                emit fieldChineseChanged();
            }
        }

        if (callback) {
            callback(suc, response, errMsg);
        }
    });
}

void AppSession::logout()
{
    clearSession();
}

void AppSession::clearSession()
{
    const bool hadLogin = isLoggedIn();
    const bool hadBaseConfig = hasBaseConfig();
    const bool hadVersionInfo = hasVersionInfo();

    // 清空顺序不影响行为，但先记录旧状态，便于只在必要时发送差异通知。
    mToken.clear();
    mLoginInfo.clear();
    mBaseConfig.clear();
    mVersionInfo.clear();

    if (hadLogin) {
        emit loginStateChanged();
    }

    if (hadBaseConfig) {
        emit baseConfigChanged();
    }

    if (hadVersionInfo) {
        emit versionInfoChanged();
    }

    emit sessionCleared();
}

const QString& AppSession::token() const
{
    return mToken;
}

bool AppSession::isLoggedIn() const
{
    return !mToken.isEmpty() && !mLoginInfo.isNull();
}

QSharedPointer<LoginResponseJson> AppSession::loginInfo() const
{
    return mLoginInfo;
}

UserJson AppSession::userInfo() const
{
    if (mLoginInfo.isNull()) {
        return UserJson();
    }

    return mLoginInfo->j_user();
}

QSharedPointer<BaseConfigJson> AppSession::baseConfig() const
{
    return mBaseConfig;
}

bool AppSession::hasBaseConfig() const
{
    return !mBaseConfig.isNull();
}

QSharedPointer<VersionResponseJson> AppSession::versionInfo() const
{
    return mVersionInfo;
}

bool AppSession::hasVersionInfo() const
{
    return !mVersionInfo.isNull();
}

QString AppSession::fieldChineseDisplayName(const QString& fieldName) const
{
    return mFieldChineseMap.value(fieldName);
}

bool AppSession::hasFieldChinese() const
{
    return !mFieldChineseMap.isEmpty();
}
