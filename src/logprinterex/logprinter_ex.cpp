#include "logprinter_ex.h"
class LogStreamBufferPrivate : public QStringList
{
public:
    LogStreamBufferPrivate(const QString &logger, LogStreamBuffer::Level level, bool nh = true)
        : level(level), logger(logger), no_head(nh)
    {
    }

public:
    QString logger;
    bool no_head = true;
    LogStreamBuffer::Flag flag = LogStreamBuffer::Flag::ToDecimal;
    LogStreamBuffer::Level level = LogStreamBuffer::LogLevelInfo;
};

LogStreamBuffer::LogStreamBuffer(const QString &logger, Level level, bool no_head)
    : d_p(new LogStreamBufferPrivate(logger, level, no_head))
{
}

LogStreamBuffer::~LogStreamBuffer()
{
    QString message;
    if (!d_p->no_head)
    {
        message = QString("[%1][%2] %3").arg(d_p->logger, 12, ' ').arg(QDateTime::currentDateTime().toString("MM-dd hh:mm:ss")).arg(d_p->join(" "));
    }
    else
    {
        message = d_p->join(" ");
    }
    if (!message.isEmpty())
    {
        Log4Qt::Logger *logger = Log4Qt::LogManager::logger(d_p->logger);

        switch (d_p->level)
        {
        case Level::LogLevelTrace:
        {
            logger->trace(message);
        }
        break;
        case Level::LogLevelDebug:
        {
            logger->debug(message);
        }
        break;
        case Level::LogLevelInfo:
        {
            logger->info(message);
        }
        break;
        case Level::LogLevelWarn:
        {
            logger->warn(message);
        }
        break;
        case Level::LogLevelError:
        {
            logger->error(message);
        }
        break;
        case Level::LogLevelFatal:
        {
            logger->fatal(message);
        }
        break;
        default:
        {
            logger->info(message);
        }
        break;
        }
    }

    if (d_p)
    {
        delete d_p;
        d_p = nullptr;
    }
}

void LogStreamBuffer::init(const QString &cfgfile)
{
#ifndef NO_LOG4QT
    Log4Qt::PropertyConfigurator::configureAndWatch(cfgfile);
    LOG_INFOEX("Logger") << "Init log4qt with config file: " << cfgfile;
#endif
}

LogStreamBuffer &LogStreamBuffer::operator<<(const Flag value)
{
    d_p->flag = value;
    return *this;
}

int LogStreamBuffer::currentBase() const
{
    switch (d_p->flag)
    {
    case Flag::ToBinary:
        return 2;
    case Flag::ToOctal:
        return 8;
    case Flag::ToHex:
        return 16;
    }
    return 10;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const char *value)
{
    d_p->append(QString::fromUtf8(value));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const std::string &value)
{
    d_p->append(QString::fromStdString(value));
    return *this;
}

LogStreamBuffer &LogStreamBuffer::operator<<(const QByteArray &value)
{
    switch (d_p->flag)
    {
    case Flag::ToHex:
        d_p->append(value.toHex().data());
        break;
    default:
        d_p->append(value.data());
        break;
    }
    return *this;
}

LogStreamBuffer &LogStreamBuffer::operator<<(const QString &value)
{
    d_p->append(value);
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const qint8 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const quint8 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const qint16 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const quint16 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const qint32 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const quint32 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const qint64 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const quint64 &value)
{
    d_p->append(QString::number(value, currentBase()));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const double &value)
{
    d_p->append(QString::number(value));
    return *this;
}
LogStreamBuffer &LogStreamBuffer::operator<<(const float &value)
{
    d_p->append(QString::number(value));
    return *this;
}
