/*****************************************************************//**
 * \file   aiclientconfig.cpp
 * \brief  AI 客户端配置读取实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "aiclientconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace {

constexpr double kMinCompactRatio = 0.2;
constexpr double kMaxCompactRatio = 0.95;

} // namespace

AIClientConfig AIClientConfig::loadFromAppDir()
{
    AIClientConfig config;
    const QString appDir = QCoreApplication::applicationDirPath();
    config.knowledgeDir = QDir(appDir).filePath(QStringLiteral("knowledge"));

    QSettings settings(QDir(appDir).filePath(QStringLiteral("ai_client.ini")), QSettings::IniFormat);
    config.baseUrl = normalizedBaseUrl(settings.value(QStringLiteral("AI/BaseUrl")).toString());
    config.apiKey = settings.value(QStringLiteral("AI/ApiKey")).toString();
    config.modelId = settings.value(QStringLiteral("AI/ModelId"), config.modelId).toString().trimmed();
    config.knowledgeDir = settings.value(QStringLiteral("AI/KnowledgeDir"), config.knowledgeDir).toString();
    config.contextLimit = settings.value(QStringLiteral("AI/ContextLimit"), config.contextLimit).toInt();
    config.autoCompactRatio = settings.value(QStringLiteral("AI/AutoCompactRatio"), config.autoCompactRatio).toDouble();
    config.requestTimeoutMs = settings.value(QStringLiteral("AI/RequestTimeoutMs"), config.requestTimeoutMs).toInt();
    config.streamIdleTimeoutMs = settings.value(QStringLiteral("AI/StreamIdleTimeoutMs"), config.streamIdleTimeoutMs).toInt();
    config.streamEnabled = settings.value(QStringLiteral("AI/StreamEnabled"), config.streamEnabled).toBool();

    if (config.modelId.isEmpty()) {
        config.modelId = QStringLiteral("gpt-5.5");
    }
    if (config.contextLimit <= 0) {
        config.contextLimit = DefaultContextLimit;
    }
    if (config.autoCompactRatio < kMinCompactRatio || config.autoCompactRatio > kMaxCompactRatio) {
        config.autoCompactRatio = 0.75;
    }
    if (config.requestTimeoutMs <= 0) {
        config.requestTimeoutMs = 120000;
    }
    if (config.streamIdleTimeoutMs <= 0) {
        config.streamIdleTimeoutMs = 60000;
    }
    return config;
}

bool AIClientConfig::isUsable() const
{
    return !baseUrl.isEmpty() && !apiKey.isEmpty() && !modelId.isEmpty();
}

QString AIClientConfig::chatCompletionsUrl() const
{
    if (baseUrl.isEmpty()) {
        return QString();
    }
    return baseUrl + QStringLiteral("/chat/completions");
}

QString AIClientConfig::modelDetailUrl() const
{
    if (baseUrl.isEmpty() || modelId.isEmpty()) {
        return QString();
    }
    return baseUrl + QStringLiteral("/models/") + modelId;
}

QString AIClientConfig::normalizedBaseUrl(const QString& rawUrl)
{
    QString url = rawUrl.trimmed();
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    return url;
}
