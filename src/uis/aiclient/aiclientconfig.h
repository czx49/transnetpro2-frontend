/*****************************************************************//**
 * \file   aiclientconfig.h
 * \brief  AI 客户端配置读取
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef AICLIENTCONFIG_H
#define AICLIENTCONFIG_H

#include <QString>

class AIClientConfig {
public:
    static constexpr int DefaultContextLimit = 200000;

    static AIClientConfig loadFromAppDir();

    QString baseUrl;
    QString apiKey;
    QString modelId = QStringLiteral("gpt-5.5");
    QString knowledgeDir;
    int contextLimit = DefaultContextLimit;
    double autoCompactRatio = 0.75;
    int requestTimeoutMs = 120000;
    int streamIdleTimeoutMs = 60000;
    bool streamEnabled = true;

    bool isUsable() const;
    QString chatCompletionsUrl() const;
    QString modelDetailUrl() const;

private:
    static QString normalizedBaseUrl(const QString& rawUrl);
};

#endif // AICLIENTCONFIG_H
