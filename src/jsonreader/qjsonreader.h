#ifndef QJsonReader_H
#define QJsonReader_H

#include <QtCore>

#define PROPERTY_DEFINE(type, name)       \
    type j_##name() const                 \
    {                                     \
        return readProperty<type>(#name); \
    }

#define PROPERTY_DEFINE2(type, name)  \
    type j_##name() const             \
    {                                 \
        return readProperty2<type>(); \
    }

class QJsonReader {
public:
    /**
     * @brief 默认构造函数
     */
    QJsonReader();

    /**
     * @brief 构造函数，指定层级和父节点
     * @param level 当前层级（递归/嵌套时使用）
     * @param parent 父JsonReader指针，默认为nullptr
     */
    QJsonReader(const QJsonValue& json);
    QJsonReader(const QByteArray& jsondata);

    // 拷贝构造与赋值
    QJsonReader(const QJsonReader& other);
    QJsonReader& operator=(const QJsonReader& other);

    /**
     * @brief 析构函数
     */
    virtual ~QJsonReader() = default;

public:
    /**
     * @brief 加载json对象
     * @param json nlohmann::json对象
     * @return 是否加载成功
     */
    virtual bool loadByJson(const QJsonValue& json);

    /**
     * @brief 从字符串数据加载json
     * @param json std::string格式的json数据
     * @return 是否加载成功
     */
    virtual bool loadByJsonData(const QByteArray& json);

    QJsonObject jsonObject() const;

    // 判断类型是否为 QList<U>
    template <typename T>
    struct is_qt_qtlist : std::false_type {
    };
    template <typename U>
    struct is_qt_qtlist<QList<U>> : std::true_type {
    };
    template <class T>
    T readProperty(const QString& name) const
    {
        // Q_ASSERT(m_jsonValue.toObject().contains(name));
        if (!m_jsonValue.toObject().contains(name)) {
            qWarning() << "QJsonReader::readProperty: not contains key:" << name;
            return T();
        }
        QJsonValue v = m_jsonValue.toObject().value(name);
        if constexpr (is_qt_qtlist<T>::value) {
            using U = typename T::value_type;
            QList<U> result;
            const QJsonArray arr = v.toArray();
            for (const auto& item : arr) {
                result << converValue<U>(item);
            }
            return result;
        } else {
            return converValue<T>(v);
        }
    }
    template <class T>
    T readProperty2() const
    {
        if constexpr (is_qt_qtlist<T>::value) {
            using U = typename T::value_type;
            QList<U> result;
            const QJsonArray arr = m_jsonValue.toArray();
            for (const auto& item : arr) {
                result << converValue<U>(item);
            }
            return result;
        } else {
            return converValue<T>(m_jsonValue);
        }
    }

    template <class T>
    T converValue(const QJsonValue& v) const
    {
        if constexpr (std::is_base_of<QJsonReader, T>::value) {
            T obj(v.toObject());
            return obj;
        } else if constexpr (std::is_same<T, int>::value) {
            return v.toInt();
        } else if constexpr (std::is_same<T, double>::value) {
            return v.toDouble();
        } else if constexpr (std::is_same<T, bool>::value) {
            return v.toBool();
        } else if constexpr (std::is_same<T, QString>::value) {
            return v.toString();
        } else if constexpr (std::is_same<T, QByteArray>::value) {
            return v.toString().toUtf8();
        } else if constexpr (std::is_same<T, QJsonObject>::value) {
            return v.toObject();
        } else if constexpr (std::is_same<T, QJsonArray>::value) {
            return v.toArray();
        } else {
            return v.toVariant().value<T>();
        }
    }

    template <class T>
    static QSharedPointer<T> dataAsObjectPtr(const QJsonValue& obj)
    {
        return QSharedPointer<T>::create(obj);
    }

    template <class T>
    static T dataAsObject(const QJsonObject& obj)
    {
        return T(obj);
    }

    template <class T>
    static QList<T> dataAsObject(const QList<QJsonObject>& objs)
    {
        QList<T> ts;
        for (int i = 0; i < objs.size(); i++) {
            if (!objs[i].isEmpty())
                ts << dataAsObject<T>(objs[i]);
        }
        return ts;
    }

protected:
    QJsonValue m_jsonValue;
};
#endif // QJsonReader_H
