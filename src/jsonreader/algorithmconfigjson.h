#ifndef algorithmconfigjson_h
#define algorithmconfigjson_h

#include "qjsonreader.h"
#include <QtCore>

class ParameterJson : public QJsonReader
{
public:
    ParameterJson() = default;
    ParameterJson(const ParameterJson &other) = default;
    explicit ParameterJson(const QJsonValue &json)
        : QJsonReader(json)
    {
    }
    explicit ParameterJson(const QByteArray &jsonData)
        : QJsonReader(jsonData)
    {
    }

    PROPERTY_DEFINE(QString, fieldName)
    PROPERTY_DEFINE(QString, dataType)
    PROPERTY_DEFINE(QString, desc)
    PROPERTY_DEFINE(QString, remarks)
    PROPERTY_DEFINE(QString, tranlateName)
    PROPERTY_DEFINE(QVariant, default)
};
Q_DECLARE_METATYPE(ParameterJson);

class InputOutputDataJson : public QJsonReader
{
public:
    InputOutputDataJson() = default;
    InputOutputDataJson(const InputOutputDataJson &other) = default;
    explicit InputOutputDataJson(const QJsonValue &json)
        : QJsonReader(json)
    {
    }
    explicit InputOutputDataJson(const QByteArray &jsonData)
        : QJsonReader(jsonData)
    {
    }

    PROPERTY_DEFINE(QString, displayName)
    PROPERTY_DEFINE(QString, tableName)
};
Q_DECLARE_METATYPE(InputOutputDataJson);

class AlgorithmNameJson : public QJsonReader
{
public:
    AlgorithmNameJson() = default;
    AlgorithmNameJson(const AlgorithmNameJson &other) = default;
    explicit AlgorithmNameJson(const QJsonValue &json)
        : QJsonReader(json)
    {
    }
    explicit AlgorithmNameJson(const QByteArray &jsonData)
        : QJsonReader(jsonData)
    {
    }

    PROPERTY_DEFINE(QString, algorithmName)
    PROPERTY_DEFINE(QString, displayName)
    PROPERTY_DEFINE(QString, type)
    PROPERTY_DEFINE(QList<QString>, dependentLib)
    PROPERTY_DEFINE(QList<InputOutputDataJson>, inputData)
    PROPERTY_DEFINE(QList<InputOutputDataJson>, outputData)
    PROPERTY_DEFINE(QList<ParameterJson>, parameters)
};
Q_DECLARE_METATYPE(AlgorithmNameJson);

#endif // algorithmconfigjson_h
