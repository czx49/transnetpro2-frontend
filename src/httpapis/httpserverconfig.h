#ifndef HTTPSERVERCONFIG_H
#define HTTPSERVERCONFIG_H

#include <QSettings>
#include <QtCore>

#define HTTPSERVER_LOGGER "HttpServerConfig"

class HttpServerConfig {
public:
    static HttpServerConfig& instance()
    {
        static HttpServerConfig instance;
        return instance;
    }

    /**
     * @brief 获取主机名 (HostName)
     * @return 主机名字符串
     */
    QString hostName() const
    {
        return m_settings.value("Config/HostName").toString();
    }

private:
    HttpServerConfig()
        : m_settings(QCoreApplication::applicationDirPath() + "/httpserver.ini", QSettings::IniFormat)
    {
        // Reading HostName from the configuration file
        QString hostName = m_settings.value("Config/HostName").toString();
    }

    HttpServerConfig(const HttpServerConfig&) = delete;
    HttpServerConfig& operator=(const HttpServerConfig&) = delete;

    QSettings m_settings;
};

#endif // HTTPSERVERCONFIG_H
