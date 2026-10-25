/**
 * @file qrequest2.h
 * @brief QRequest2 - 线程安全的RESTful HTTP网络请求封装库
 * @version 2.0
 * @date 2025-09-16
 *
 * @details
 * QRequest2是一个基于Qt的HTTP网络请求封装库，提供了：
 * - 线程安全的单例设计模式
 * - 异步和同步两种请求方式
 * - 支持GET、POST、PUT、DELETE、PATCH等RESTful操作
 * - 文件上传功能
 * - QML JavaScript集成支持
 * - 自动管理多线程环境下的QNetworkAccessManager实例
 *
 * @warning 线程安全注意事项：
 * - 每个线程都有独立的QNetworkAccessManager实例
 * - 回调函数在网络请求完成的线程中执行
 * - 同步方法会阻塞当前线程直到请求完成
 *
 * @author QRequest Team
 */
#ifndef QREQUESTEX2_H
#define QREQUESTEX2_H

#include "qrequest_global.h"
#include <QtCore>
#include <QtNetwork>
#ifdef ENABLED_QML
#include <QJSValue>
#endif

// 前向声明
class QRequestPrivate;
class stHttpResponsePrivate;

/**
 * @brief 线程安全的RESTful网络请求封装类
 *
 * @details
 * QRequest2采用单例模式设计，为多线程环境提供安全的HTTP请求功能。
 *
 * 主要特性：
 * - 单例模式确保全局唯一实例
 * - 每个线程维护独立的QNetworkAccessManager实例
 * - 支持异步回调和同步阻塞两种调用方式
 * - 提供完整的RESTful API支持
 * - 支持文件上传功能
 * - 兼容QML JavaScript环境
 *
 * 线程安全性：
 * - ✅ 多线程并发调用安全
 * - ✅ 静态单例实例线程安全
 * - ✅ 网络管理器按线程隔离
 * - ⚠️  回调函数需注意线程上下文
 *
 * 使用示例：
 * @code
 * // 异步POST请求
 * QRequest2::instance().post("http://api.example.com/data",
 *     [](bool success, const QString& msg, const QByteArray& data) {
 *         if (success) {
 *             qDebug() << "Response:" << data;
 *         } else {
 *             qDebug() << "Error:" << msg;
 *         }
 *     },
 *     QJsonObject{{"key", "value"}});
 *
 * // 同步GET请求
 * auto response = QRequest2::instance().getSync("http://api.example.com/info");
 * if (response.suc()) {
 *     qDebug() << "Data:" << response.data();
 * }
 * @endcode
 */
class QREQUEST_EXPORT QRequest2 : public QObject
{
    Q_OBJECT

public:
    /** @brief 普通 REST 请求默认传输超时（毫秒） */
    static constexpr int kDefaultTransferTimeoutMs = 15000;
    /** @brief 长耗时同步 REST 请求传输超时（毫秒） */
    static constexpr int kLongRunningTransferTimeoutMs = 90000;

    /**
     * @brief HTTP请求头部映射类型
     * @details 用于设置标准HTTP头部字段，如Content-Type、Accept等
     */
    typedef QMap<QNetworkRequest::KnownHeaders, QVariant> KnownHeaderMap;

    /**
     * @brief HTTP响应数据封装类
     *
     * @details
     * 封装HTTP请求的响应结果，包括成功状态、错误消息和响应数据。
     * 使用了Pimpl模式来隐藏实现细节并提供二进制兼容性。
     *
     * 线程安全性：
     * - ✅ 值语义，可安全地在线程间传递
     * - ✅ 深拷贝构造，无共享状态
     * - ✅ 支持赋值和拷贝操作
     */
    class QREQUEST_EXPORT stHttpResponse
    {
    public:
        /**
         * @brief 默认构造函数
         * @details 创建一个表示失败状态的响应对象
         */
        stHttpResponse();

        /**
         * @brief 拷贝构造函数
         * @param other 要拷贝的响应对象
         * @details 执行深拷贝，确保线程安全
         */
        stHttpResponse(const stHttpResponse &other);

        /**
         * @brief 参数化构造函数
         * @param s 请求是否成功
         * @param m 错误消息（如果失败）
         * @param d 响应数据
         */
        stHttpResponse(bool s, const QString &m, const QByteArray &d);

        /**
         * @brief 析构函数
         * @details 自动释放私有数据
         */
        ~stHttpResponse();

        /**
         * @brief 获取请求是否成功
         * @return true表示请求成功，false表示失败
         * @note 线程安全的只读操作
         */
        bool suc() const;

        /**
         * @brief 设置请求成功状态
         * @param s 成功状态
         */
        void setSuc(bool s);

        /**
         * @brief 获取错误消息
         * @return 错误消息字符串，成功时为空
         * @note 线程安全的只读操作
         */
        QString msg() const;

        /**
         * @brief 设置错误消息
         * @param msg 错误消息
         */
        void setMsg(const QString &msg);

        /**
         * @brief 获取响应数据
         * @return HTTP响应的原始字节数据
         * @note 线程安全的只读操作
         */
        QByteArray data() const;

        /**
         * @brief 设置响应数据
         * @param d 响应数据
         */
        void setData(const QByteArray &d);

    private:
        stHttpResponsePrivate *d_p = nullptr; ///< Pimpl指针，隐藏实现细节
    };

public:
    /**
     * @brief HTTP请求回调函数类型定义
     *
     * @param suc 请求是否成功
     * @param msg 错误消息（失败时）或成功消息
     * @param data 响应数据
     *
     * @warning 回调函数注意事项：
     * - 回调在网络请求完成的线程中执行
     * - 如需更新UI，请使用Qt::QueuedConnection或信号槽机制
     * - 避免在回调中执行耗时操作
     * - 确保回调函数本身是线程安全的
     */
    using QRequest2CallBack = std::function<void(bool suc, const QString &msg, const QByteArray &data)>;

private:
    /**
     * @brief 私有构造函数，实现单例模式
     * @param parent 父对象指针，通常为nullptr
     */
    QRequest2(QObject *parent = nullptr);

    /**
     * @brief 私有析构函数
     */
    virtual ~QRequest2();

public:
    /**
     * @brief 获取单例实例
     * @return QRequest2的全局唯一实例引用
     *
     * @note 线程安全性：
     * - 使用C++11的静态局部变量保证线程安全的单例初始化
     * - 多线程环境下第一次调用时会安全地初始化实例
     * - 后续调用直接返回已创建的实例
     */
    static QRequest2 &instance();

public:
    //=============================================================================
    // POST 请求方法组
    //=============================================================================

    /**
     * @brief POST请求（QML/JavaScript版本）
     *
     * @param url 请求URL地址
     * @param callbackFunc JavaScript回调函数
     * @param param 请求参数（支持QVariant类型）
     * @param headers 标准HTTP头部映射
     * @param rowHeaders 自定义原始头部映射
     *
     * @details
     * 此版本专为QML环境设计，支持JavaScript回调函数。
     * 参数会自动序列化为JSON格式发送。
     *
     * @note QML使用示例：
     * @code{.qml}
     * QRequest2.post("http://api.example.com/data", function(success, msg, data) {
     *     if (success) {
     *         console.log("Success:", data)
     *     } else {
     *         console.log("Error:", msg)
     *     }
     * }, {"key": "value"})
     * @endcode
     */
#ifdef ENABLED_QML
    Q_INVOKABLE void post(const QString &url,
                          QJSValue callbackFunc,
                          const QVariant &param,
                          const QVariantMap &headers = QVariantMap(),
                          const QVariantMap &rowHeaders = QVariantMap());
#endif

    /**
     * @brief POST请求（C++版本）
     *
     * @param url 请求URL地址
     * @param callbackFunc C++回调函数
     * @param param 请求参数（支持QVariant类型，推荐QJsonObject）
     * @param headers 标准HTTP头部映射
     * @param rawHeaders 自定义原始头部映射
     * @param error 错误信息输出指针（可选）
     *
     * @details
     * 异步POST请求，不会阻塞当前线程。
     * 请求完成后会在网络线程中调用回调函数。
     *
     * @warning 线程安全注意事项：
     * - 此方法本身是线程安全的
     * - 可以从任何线程调用
     * - 回调函数在网络操作完成的线程中执行
     *
     * @note C++使用示例：
     * @code{.cpp}
     * QJsonObject data{{"username", "test"}, {"password", "123"}};
     * QRequest2::instance().post("http://api.example.com/login",
     *     [](bool success, const QString& msg, const QByteArray& response) {
     *         // 处理响应
     *     }, data);
     * @endcode
     */
    void post(const QString &url,
              QRequest2CallBack callbackFunc,
              const QVariant &param = QVariant(),
              const KnownHeaderMap &headers = KnownHeaderMap(),
              const QVariantMap &rawHeaders = QVariantMap(),
              QString *error = nullptr,
              int transferTimeoutMs = kDefaultTransferTimeoutMs);

    /**
     * @brief POST请求（同步版本）
     *
     * @param url 请求URL地址
     * @param param 请求参数
     * @param headers 标准HTTP头部映射
     * @param rawHeaders 自定义原始头部映射
     * @param error 错误信息输出指针（可选）
     * @return HTTP响应结构体
     *
     * @details
     * 同步POST请求，会阻塞当前线程直到请求完成。
     * 内部使用QEventLoop实现同步等待。
     *
     * @warning 阻塞性操作注意事项：
     * - 会阻塞调用线程直到请求完成
     * - 不要在主线程中调用，避免界面冻结
     * - 适合在工作线程中使用
     * - 可能会超时，注意异常处理
     *
     * @note 使用示例：
     * @code{.cpp}
     * // 在工作线程中使用
     * auto response = QRequest2::instance().postSync("http://api.example.com/data", data);
     * if (response.suc()) {
     *     // 处理成功响应
     *     QJsonDocument doc = QJsonDocument::fromJson(response.data());
     * } else {
     *     // 处理错误
     *     qDebug() << "Error:" << response.msg();
     * }
     * @endcode
     */
    QRequest2::stHttpResponse postSync(const QString &url,
                                       const QVariant &param = QVariant(),
                                       const KnownHeaderMap &headers = KnownHeaderMap(),
                                       const QVariantMap &rawHeaders = QVariantMap(),
                                       QString *error = nullptr);

    //=============================================================================
    // 文件上传方法组
    //=============================================================================

    /**
     * @brief POST文件上传请求（QML/JavaScript版本）
     *
     * @param filepath 本地文件路径
     * @param url 上传目标URL
     * @param callbackFunc JavaScript回调函数
     * @param param 额外的表单参数
     * @param filename 服务器端文件名（可选，默认使用本地文件名）
     * @param headers 标准HTTP头部
     * @param rawHeaders 自定义原始头部
     *
     * @details
     * 使用multipart/form-data格式上传文件，支持额外的表单字段。
     * 自动设置Content-Type和boundary。
     */
#ifdef ENABLED_QML
    Q_INVOKABLE void postFile(const QString &filepath,
                              const QString &url,
                              QJSValue callbackFunc,
                              const QVariant &param = QVariant(),
                              const QString &filename = "",
                              const QVariantMap &headers = QVariantMap(),
                              const QVariantMap &rawHeaders = QVariantMap());
#endif

    /**
     * @brief POST文件上传请求（C++版本）
     *
     * @details 异步文件上传，支持大文件上传。
     * 文件读取和上传过程不会阻塞调用线程。
     *
     * @warning 注意事项：
     * - 确保文件路径有效且可读
     * - 大文件上传可能需要较长时间
     * - 上传过程中文件不应被修改或删除
     */
    void postFile(const QString &filepath,
                  const QString &url,
                  QRequest2CallBack callbackFunc,
                  const QVariant &param = QVariant(),
                  const QString &filename = "",
                  const KnownHeaderMap &headers = KnownHeaderMap(),
                  const QVariantMap &rawHeaders = QVariantMap(),
                  QString *error = nullptr,
                  int transferTimeoutMs = kDefaultTransferTimeoutMs);

    /**
     * @brief POST文件上传请求（同步版本）
     *
     * @warning 阻塞性文件上传，适合小文件或后台线程使用
     */
    QRequest2::stHttpResponse postFileSync(const QString &filepath,
                                           const QString &url,
                                           const QString &filename,
                                           const KnownHeaderMap &headers = KnownHeaderMap(),
                                           const QVariantMap &rawHeaders = QVariantMap(),
                                           QString *error = nullptr);

    //=============================================================================
    // RESTful API 方法组（GET、PUT、DELETE、PATCH）
    //=============================================================================
    // 注：以下方法遵循与POST相同的设计模式：
    // - 提供QML和C++两个版本
    // - 支持异步回调和同步阻塞两种调用方式
    // - 线程安全设计，可从任何线程调用
    // - 参数格式和错误处理机制一致
    //=============================================================================

    /**
     * @brief GET请求
     * @param url 请求URL
     * @param callbackFunc 回调函数
     * @param param 请求参数
     * @param headers 已知头部键值对映射
     * @param rawHeaders 原始头部键值对映射
     * @param error 错误消息（如果有错误）
     */
#ifdef ENABLED_QML
    Q_INVOKABLE void get(const QString &url,
                         QJSValue callbackFunc,
                         const QVariant &param = QVariant(),
                         const QVariantMap &headers = QVariantMap(),
                         const QVariantMap &rawHeaders = QVariantMap());
#endif
    void get(const QString &url,
             QRequest2CallBack callbackFunc,
             const QVariant &param = QVariant(),
             const KnownHeaderMap &headers = KnownHeaderMap(),
             const QVariantMap &rawHeaders = QVariantMap(),
             QString *error = nullptr,
             int transferTimeoutMs = kDefaultTransferTimeoutMs);
    QRequest2::stHttpResponse getSync(const QString &url,
                                      const QVariant &param = QVariant(),
                                      const KnownHeaderMap &headers = KnownHeaderMap(),
                                      const QVariantMap &rawHeaders = QVariantMap(),
                                      QString *error = nullptr);

    /**
     * @brief PUT请求
     * @param url 请求URL
     * @param callbackFunc 回调函数
     * @param param 请求参数
     * @param headers 已知头部键值对映射
     * @param rawHeaders 原始头部键值对映射
     * @param error 错误消息（如果有错误）
     */
#ifdef ENABLED_QML
    Q_INVOKABLE void put(const QString &url,
                         QJSValue callbackFunc,
                         const QVariant &param = QVariant(),
                         const QVariantMap &headers = QVariantMap(),
                         const QVariantMap &rawHeaders = QVariantMap());
#endif
    void put(const QString &url,
             QRequest2CallBack callbackFunc,
             const QVariant &param = QVariant(),
             const KnownHeaderMap &headers = KnownHeaderMap(),
             const QVariantMap &rawHeaders = QVariantMap(),
             QString *error = nullptr,
             int transferTimeoutMs = kDefaultTransferTimeoutMs);
    QRequest2::stHttpResponse putSync(const QString &url,
                                      const QVariant &param = QVariant(),
                                      const KnownHeaderMap &headers = KnownHeaderMap(),
                                      const QVariantMap &rawHeaders = QVariantMap(),
                                      QString *error = nullptr);

    /**
     * @brief DELETE请求
     * @param url 请求URL
     * @param callbackFunc 回调函数
     * @param param 请求参数
     * @param headers 已知头部键值对映射
     * @param rawHeaders 原始头部键值对映射
     * @param error 错误消息（如果有错误）
     */
#ifdef ENABLED_QML
    Q_INVOKABLE void remove(const QString &url,
                            QJSValue callbackFunc,
                            const QVariant &param = QVariant(),
                            const QVariantMap &headers = QVariantMap(),
                            const QVariantMap &rawHeaders = QVariantMap());
#endif
    void remove(const QString &url,
                QRequest2CallBack callbackFunc,
                const QVariant &param = QVariant(),
                const KnownHeaderMap &headers = KnownHeaderMap(),
                const QVariantMap &rawHeaders = QVariantMap(),
                QString *error = nullptr,
                int transferTimeoutMs = kDefaultTransferTimeoutMs);
    QRequest2::stHttpResponse removeSync(const QString &url,
                                         const QVariant &param = QVariant(),
                                         const KnownHeaderMap &headers = KnownHeaderMap(),
                                         const QVariantMap &rawHeaders = QVariantMap(),
                                         QString *error = nullptr);

    /**
     * @brief PATCH请求
     * @param url 请求URL
     * @param callbackFunc 回调函数
     * @param param 请求参数
     * @param headers 已知头部键值对映射
     * @param rawHeaders 原始头部键值对映射
     * @param error 错误消息（如果有错误）
     */
#ifdef ENABLED_QML
    Q_INVOKABLE void patch(const QString &url,
                           QJSValue callbackFunc,
                           const QVariant &param = QVariant(),
                           const QVariantMap &headers = QVariantMap(),
                           const QVariantMap &rawHeaders = QVariantMap());
#endif
    void patch(const QString &url,
               QRequest2CallBack callbackFunc,
               const QVariant &param = QVariant(),
               const KnownHeaderMap &headers = KnownHeaderMap(),
               const QVariantMap &rawHeaders = QVariantMap(),
               QString *error = nullptr,
               int transferTimeoutMs = kDefaultTransferTimeoutMs);
    QRequest2::stHttpResponse patchSync(const QString &url,
                                        const QVariant &param = QVariant(),
                                        const KnownHeaderMap &headers = KnownHeaderMap(),
                                        const QVariantMap &rawHeaders = QVariantMap(),
                                        QString *error = nullptr);

    /**
     * @brief 创建GET请求的查询参数字符串
     * @param params 参数映射
     * @return URL编码的查询字符串
     *
     * @details 将QVariantMap转换为"key1=value1&key2=value2"格式的查询字符串
     *
     * @note 工具方法，线程安全
     */
    static QString createGetParamString(const QVariantMap &params);

    /**
     * @brief 获取当前线程的网络访问管理器
     * @return 当前线程专用的QNetworkAccessManager指针
     *
     * @details
     * 线程安全的核心方法，确保每个线程都有独立的网络管理器实例。
     * 使用线程ID作为键值，维护线程到管理器的映射关系。
     *
     * 线程安全机制：
     * - 使用QMutex保护全局映射表的访问
     * - 每个线程维护独立的QNetworkAccessManager实例
     * - 网络管理器绑定到对应线程，确保信号槽机制正常工作
     *
     * @warning 关键的线程安全实现，修改时需谨慎
     * @note 此方法主要用于测试目的，允许验证线程安全实现
     */
    static QNetworkAccessManager *network();

protected:
    //=============================================================================
    // 内部实现方法组 - 线程安全的底层实现
    //=============================================================================

    /**
     * @brief 添加HTTP头部内容到请求中
     * @param request 网络请求对象引用
     * @param headers 标准头部映射
     * @param rawHeaders 原始头部映射
     *
     * @details 内部方法，负责设置HTTP请求头
     * @note 线程安全，可在任何线程中调用
     */
    void _addHeaderContent(QNetworkRequest &request,
                           const KnownHeaderMap &headers,
                           const QVariantMap &rawHeaders);

    void _processReply(QNetworkReply *reply, QRequest2CallBack callbackFunc);

#ifdef ENABLED_QML
    void _processReply(QNetworkReply *reply, QJSValue callbackFunc);
#endif

    QNetworkReply *_createReplyBase(const QByteArray &verb,
                                    const QString &url,
                                    const QVariant &param = QVariant(),
                                    const KnownHeaderMap &headers = KnownHeaderMap(),
                                    const QVariantMap &rawHeaders = QVariantMap(),
                                    QString *error = nullptr,
                                    int transferTimeoutMs = kDefaultTransferTimeoutMs);
    /**
     * @brief 创建通用HTTP请求回复对象
     * @param verb HTTP动词（GET、POST、PUT、DELETE、PATCH）
     * @param url 请求URL
     * @param param 请求参数
     * @param headers 标准头部
     * @param rawHeaders 原始头部
     * @param error 错误信息输出
     * @return QNetworkReply指针
     *
     * @details
     * 核心方法，创建并配置网络请求。
     * 自动选择当前线程对应的QNetworkAccessManager实例。
     *
     * @warning 返回的QNetworkReply需要适当管理生命周期
     */
    void _createReply(const QByteArray &verb,
                      const QString &url,
                      QRequest2CallBack callbackFunc,
                      const QVariant &param = QVariant(),
                      const KnownHeaderMap &headers = KnownHeaderMap(),
                      const QVariantMap &rawHeaders = QVariantMap(),
                      QString *error = nullptr,
                      int transferTimeoutMs = kDefaultTransferTimeoutMs);

#ifdef ENABLED_QML
    void _createReply(const QByteArray &verb,
                      const QString &url,
                      QJSValue callbackFunc,
                      const QVariant &param = QVariant(),
                      const KnownHeaderMap &headers = KnownHeaderMap(),
                      const QVariantMap &rawHeaders = QVariantMap(),
                      QString *error = nullptr,
                      int transferTimeoutMs = kDefaultTransferTimeoutMs);
#endif

    /**
     * @brief 创建文件上传请求回复对象
     * @param filepath 文件路径
     * @param url 上传URL
     * @param filename 服务器端文件名
     * @param headers 标准头部
     * @param rawHeaders 原始头部
     * @param error 错误信息输出
     * @return QNetworkReply指针
     *
     * @details
     * 专用于文件上传的请求创建方法。
     * 自动处理multipart/form-data格式。
     * 文件会绑定到QNetworkReply的生命周期。
     */
    QNetworkReply *_createFileReplyBase(const QString &filepath,
                                        const QString &url,
                                        const QString &filename = "",
                                        const KnownHeaderMap &headers = KnownHeaderMap(),
                                        const QVariantMap &rawHeaders = QVariantMap(),
                                        QString *error = nullptr,
                                        int transferTimeoutMs = kDefaultTransferTimeoutMs);

    void _createFileReply(const QString &filepath,
                          const QString &url,
                          QRequest2CallBack callbackFunc,
                          const QString &filename = "",
                          const KnownHeaderMap &headers = KnownHeaderMap(),
                          const QVariantMap &rawHeaders = QVariantMap(),
                          QString *error = nullptr,
                          int transferTimeoutMs = kDefaultTransferTimeoutMs);

#ifdef ENABLED_QML
    void _createFileReply(const QString &filepath,
                          const QString &url,
                          QJSValue callbackFunc,
                          const QString &filename = "",
                          const KnownHeaderMap &headers = KnownHeaderMap(),
                          const QVariantMap &rawHeaders = QVariantMap(),
                          QString *error = nullptr,
                          int transferTimeoutMs = kDefaultTransferTimeoutMs);
#endif

    //=============================================================================
    // 私有请求处理方法组 - C++回调版本
    //=============================================================================
    void _post(const QString &url,
               QRequest2CallBack callbackFunc,
               const QVariant &param = QVariant(),
               const KnownHeaderMap &headers = KnownHeaderMap(),
               const QVariantMap &rawHeaders = QVariantMap(),
               QString *error = nullptr,
               int transferTimeoutMs = kDefaultTransferTimeoutMs);

#ifdef ENABLED_QML
    void _post(const QString &url,
               QJSValue callbackFunc,
               const QVariant &param = QVariant(),
               const KnownHeaderMap &headers = KnownHeaderMap(),
               const QVariantMap &rawHeaders = QVariantMap(),
               QString *error = nullptr);
    void _postFile(const QString &filepath,
                   const QString &url,
                   QJSValue callbackFunc,
                   const QString &filename,
                   const KnownHeaderMap &headers = KnownHeaderMap(),
                   const QVariantMap &rawHeaders = QVariantMap(),
                   QString *error = nullptr);
    void _get(const QString &url,
              QJSValue callbackFunc,
              const QVariant &param = QVariant(),
              const KnownHeaderMap &headers = KnownHeaderMap(),
              const QVariantMap &rawHeaders = QVariantMap(),
              QString *error = nullptr);
    void _put(const QString &url,
              QJSValue callbackFunc,
              const QVariant &param = QVariant(),
              const KnownHeaderMap &headers = KnownHeaderMap(),
              const QVariantMap &rawHeaders = QVariantMap(),
              QString *error = nullptr);
    void _remove(const QString &url,
                 QJSValue callbackFunc,
                 const QVariant &param = QVariant(),
                 const KnownHeaderMap &headers = KnownHeaderMap(),
                 const QVariantMap &rawHeaders = QVariantMap(),
                 QString *error = nullptr);
    void _patch(const QString &url,
                QJSValue callbackFunc,
                const QVariant &param = QVariant(),
                const KnownHeaderMap &headers = KnownHeaderMap(),
                const QVariantMap &rawHeaders = QVariantMap(),
                QString *error = nullptr);
#endif
    void _postFile(const QString &filepath,
                   const QString &url,
                   QRequest2CallBack callbackFunc,
                   const QString &filename,
                   const KnownHeaderMap &headers = KnownHeaderMap(),
                   const QVariantMap &rawHeaders = QVariantMap(),
                   QString *error = nullptr,
                   int transferTimeoutMs = kDefaultTransferTimeoutMs);
    void _get(const QString &url,
              QRequest2CallBack callbackFunc,
              const QVariant &param = QVariant(),
              const KnownHeaderMap &headers = KnownHeaderMap(),
              const QVariantMap &rawHeaders = QVariantMap(),
              QString *error = nullptr,
              int transferTimeoutMs = kDefaultTransferTimeoutMs);
    void _put(const QString &url,
              QRequest2CallBack callbackFunc,
              const QVariant &param = QVariant(),
              const KnownHeaderMap &headers = KnownHeaderMap(),
              const QVariantMap &rawHeaders = QVariantMap(),
              QString *error = nullptr,
              int transferTimeoutMs = kDefaultTransferTimeoutMs);
    void _remove(const QString &url,
                 QRequest2CallBack callbackFunc,
                 const QVariant &param = QVariant(),
                 const KnownHeaderMap &headers = KnownHeaderMap(),
                 const QVariantMap &rawHeaders = QVariantMap(),
                 QString *error = nullptr,
                 int transferTimeoutMs = kDefaultTransferTimeoutMs);
    void _patch(const QString &url,
                QRequest2CallBack callbackFunc,
                const QVariant &param = QVariant(),
                const KnownHeaderMap &headers = KnownHeaderMap(),
                const QVariantMap &rawHeaders = QVariantMap(),
                QString *error = nullptr,
                int transferTimeoutMs = kDefaultTransferTimeoutMs);

private:
};

/**
 * @brief QRequest2的类型别名
 * @details 提供更直观的命名，强调RESTful API特性
 */
typedef QRequest2 QRestfulRequest;

#endif // QREQUESTEX2_H
