#ifndef _logprinter_ex_h_
#define _logprinter_ex_h_

#include <QtCore>
#include <iostream>
#include <log4qt/logger.h>
#include <log4qt/logmanager.h>
#include <log4qt/propertyconfigurator.h>
#include <sstream>

// 自定义的日志流缓冲类
class LogStreamBufferPrivate;
class LogStreamBuffer
{
public:
    enum Level
    {
        LogLevelTrace = 0,
        LogLevelDebug,
        LogLevelInfo,
        LogLevelWarn,
        LogLevelError,
        LogLevelFatal,
    };

    enum class Flag
    {
        ToHex,
        ToDecimal,
        ToOctal,
        ToBinary,
    };

public:
    LogStreamBuffer(const QString &logger = "", Level level = LogLevelInfo, bool no_head = true);
    ~LogStreamBuffer();

    static void init(const QString &cfgfile);
    LogStreamBuffer &operator<<(const Flag value);
    LogStreamBuffer &operator<<(const char *value);
    LogStreamBuffer &operator<<(const std::string &value);
    LogStreamBuffer &operator<<(const QByteArray &value);
    LogStreamBuffer &operator<<(const QString &value);
    LogStreamBuffer &operator<<(const qint8 &value);
    LogStreamBuffer &operator<<(const quint8 &value);
    LogStreamBuffer &operator<<(const qint16 &value);
    LogStreamBuffer &operator<<(const quint16 &value);
    LogStreamBuffer &operator<<(const qint32 &value);
    LogStreamBuffer &operator<<(const quint32 &value);
    LogStreamBuffer &operator<<(const qint64 &value);
    LogStreamBuffer &operator<<(const quint64 &value);
    LogStreamBuffer &operator<<(const double &value);
    LogStreamBuffer &operator<<(const float &value);

    int currentBase() const;

private:
    LogStreamBufferPrivate *d_p = nullptr;
};

#define LOG_TRACE() LogStreamBuffer("", LogStreamBuffer::LogLevelTrace)
#define LOG_TRACEEX(logger) LogStreamBuffer(logger, LogStreamBuffer::LogLevelTrace)

#define LOG_DEBUG() LogStreamBuffer("", LogStreamBuffer::LogLevelDebug)
#define LOG_DEBUGEX(logger) LogStreamBuffer(logger, LogStreamBuffer::LogLevelDebug)

#define LOG_INFO() LogStreamBuffer("", LogStreamBuffer::LogLevelInfo)
#define LOG_INFOEX(logger) LogStreamBuffer(logger, LogStreamBuffer::LogLevelInfo)

#define LOG_WARN() LogStreamBuffer("", LogStreamBuffer::LogLevelWarn)
#define LOG_WARNEX(logger) LogStreamBuffer(logger, LogStreamBuffer::LogLevelWarn)

#define LOG_ERROR() LogStreamBuffer("", LogStreamBuffer::LogLevelError)
#define LOG_ERROREX(logger) LogStreamBuffer(logger, LogStreamBuffer::LogLevelError)

#define LOG_FATAL() LogStreamBuffer("", LogStreamBuffer::LogLevelFatal)
#define LOG_FATALEX(logger) LogStreamBuffer(logger, LogStreamBuffer::LogLevelFatal)

#endif
