// QJsonReader 实现文件
// 1. 实现多种构造方式，支持QJsonObject、QByteArray等
// 2. 支持Qt原生JSON处理和类型转换
// 3. 便于派生类扩展和多层级递归解析
// 4. 基于Qt原生JSON的实现

#include "qjsonreader.h"
#include <QJsonDocument>
#include <QJsonParseError>

/**
 * @brief 默认构造函数
 */
QJsonReader::QJsonReader()
    : m_jsonValue()
{
}

/**
 * @brief 通过QJsonValue构造
 * @param json QJsonValue对象
 */
QJsonReader::QJsonReader(const QJsonValue &json)
{
    loadByJson(json);
}

/**
 * @brief 通过QByteArray构造
 * @param jsondata QByteArray格式的json数据
 * @param level 当前层级
 * @param parent 父QJsonReader指针
 */
QJsonReader::QJsonReader(const QByteArray &jsondata)
{
    loadByJsonData(jsondata);
}

/**
 * @brief 加载json对象
 * @param json QJsonObject对象
 * @return 是否加载成功
 */
bool QJsonReader::loadByJson(const QJsonValue &json)
{
    m_jsonValue = json;
    return true;
}

/**
 * @brief 从QByteArray数据加载json
 * @param json QByteArray格式的json数据
 * @return 是否加载成功
 */
bool QJsonReader::loadByJsonData(const QByteArray &json)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError)
    {
        return false;
    }

    return loadByJson(doc.object());
}

/**
 * @brief 获取内部json对象
 * @return QJsonObject
 */
QJsonObject QJsonReader::jsonObject() const
{
    return m_jsonValue.toObject();
}

// 拷贝构造
QJsonReader::QJsonReader(const QJsonReader &other)
    : m_jsonValue(other.m_jsonValue)
{
}

// 拷贝赋值
QJsonReader &QJsonReader::operator=(const QJsonReader &other)
{
    if (this != &other)
    {
        m_jsonValue = other.m_jsonValue;
    }
    return *this;
}
