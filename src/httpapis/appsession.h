#ifndef APPSESSION_H
#define APPSESSION_H

#include "httpapis.h"

#include <QHash>
#include <QObject>
#include <QSharedPointer>
#include <QString>

template <class T>
class Singleton;

/**
 * @brief 全局会话单例，统一管理运行期登录态与基础配置缓存
 * @details
 * - 对外提供登录、拉取基础配置等高频入口
 * - 统一缓存 token、登录返回信息、基础配置
 * - 通过信号通知 UI 或其他模块会话状态变化
 */
class AppSession : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取全局唯一会话实例
     */
    static AppSession& instance();
    ~AppSession() override = default;

    /**
     * @brief 执行登录并在成功后缓存 token 与用户信息
     */
    void login(const QString& username,
               const QString& password,
               const HttpApis::ResponseCallBack<LoginResponseJson>& callback);

    /**
     * @brief 拉取基础配置并在成功后写入本地缓存
     */
    void fetchBaseConfig(const HttpApis::ResponseCallBack<BaseConfigJson>& callback);

    /**
     * @brief 拉取产品版本信息并在成功后写入本地缓存
     */
    void fetchVersion(const HttpApis::ResponseCallBack<VersionResponseJson>& callback);

    /**
     * @brief 拉取图层字段中文翻译并在成功后写入本地缓存
     */
    void fetchFieldChinese(const HttpApis::ResponseCallBack<PropertyJson>& callback = {});

    /**
     * @brief 登出当前会话
     * @details 当前仅清空本地内存态，不发起后端登出请求
     */
    void logout();

    /**
     * @brief 清空 token、登录信息和基础配置缓存
     */
    void clearSession();

    /**
     * @brief 获取当前 Bearer Token
     */
    const QString& token() const;

    /**
     * @brief 当前是否已持有有效登录态缓存
     */
    bool isLoggedIn() const;

    /**
     * @brief 获取完整登录返回对象
     */
    QSharedPointer<LoginResponseJson> loginInfo() const;

    /**
     * @brief 获取当前登录用户信息
     * @return 若未登录，返回默认构造的空 UserJson
     */
    UserJson userInfo() const;

    /**
     * @brief 获取基础配置缓存对象
     */
    QSharedPointer<BaseConfigJson> baseConfig() const;

    /**
     * @brief 是否已成功拉取过基础配置
     */
    bool hasBaseConfig() const;

    /**
     * @brief 获取产品版本信息缓存对象
     */
    QSharedPointer<VersionResponseJson> versionInfo() const;

    /**
     * @brief 是否已成功拉取过产品版本信息
     */
    bool hasVersionInfo() const;

    /**
     * @brief 查询字段中文展示名，无命中返回空串
     */
    QString fieldChineseDisplayName(const QString& fieldName) const;

    /**
     * @brief 是否已成功拉取过字段中文翻译
     */
    bool hasFieldChinese() const;

signals:
    /**
     * @brief 登录态发生变化时发出
     */
    void loginStateChanged();

    /**
     * @brief 基础配置缓存更新或被清空时发出
     */
    void baseConfigChanged();

    /**
     * @brief 产品版本信息缓存更新或被清空时发出
     */
    void versionInfoChanged();

    /**
     * @brief 字段中文翻译缓存更新时发出
     */
    void fieldChineseChanged();

    /**
     * @brief 会话被整体清空时发出
     */
    void sessionCleared();

private:
    friend class Singleton<AppSession>;

    explicit AppSession(QObject* parent = nullptr);

    Q_DISABLE_COPY(AppSession)

private:
    //! 当前登录 token，供其他业务接口直接复用
    QString mToken;
    //! 登录接口原始返回，保留完整 user/token 信息
    QSharedPointer<LoginResponseJson> mLoginInfo;
    //! 基础配置接口缓存，供首页、地图等模块复用
    QSharedPointer<BaseConfigJson> mBaseConfig;
    //! 产品版本信息接口缓存，供登录升级流程复用
    QSharedPointer<VersionResponseJson> mVersionInfo;
    //! 字段英文名到中文展示名的映射，供属性表等 UI 复用
    QHash<QString, QString> mFieldChineseMap;
};

#endif // APPSESSION_H
