/*****************************************************************//**
 * \file   aimessage.h
 * \brief  AI 对话消息数据结构
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef AIMESSAGE_H
#define AIMESSAGE_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

enum class AIMessageRole {
    System = 0,
    User,
    Assistant,
    Log,
    Error
};

struct AIChatMessage {
    QString id;
    AIMessageRole role = AIMessageRole::System;
    QString content;
    QString downloadUrl;
    QDateTime createdAt;
    bool inContext = false;
};

Q_DECLARE_METATYPE(AIChatMessage)

#endif // AIMESSAGE_H
