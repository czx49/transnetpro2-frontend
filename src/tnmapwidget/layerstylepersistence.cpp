/*****************************************************************//**
 * \file   layerstylepersistence.cpp
 * \brief  图层字段符号化规则本地持久化
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#include "layerstylepersistence.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

#include <qgsdatasourceuri.h>
#include <qgsvectorlayer.h>

namespace {

const QString kDbConnectionName = QStringLiteral("TnpLayerStyleConnection");
const QString kTableName = QStringLiteral("SymbolColors");

QJsonObject colorRuleToJson(const FieldColorRuleItem& item, FieldStyleCategory category)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("color"), item.color);

    switch (category) {
    case FieldStyleCategory::Number:
        obj.insert(QStringLiteral("min"), item.min);
        obj.insert(QStringLiteral("max"), item.max);
        break;
    case FieldStyleCategory::Bool:
        obj.insert(QStringLiteral("boolValue"), item.boolValue);
        break;
    case FieldStyleCategory::String:
        obj.insert(QStringLiteral("strValue"), item.stringValue);
        break;
    case FieldStyleCategory::Invalid:
        break;
    }

    return obj;
}

FieldColorRuleItem colorRuleFromJson(const QJsonObject& obj)
{
    FieldColorRuleItem item;
    item.color = obj.value(QStringLiteral("color")).toString();
    item.min = obj.value(QStringLiteral("min")).toDouble();
    item.max = obj.value(QStringLiteral("max")).toDouble();
    item.boolValue = obj.value(QStringLiteral("boolValue")).toBool();
    item.stringValue = obj.value(QStringLiteral("strValue")).toString();
    return item;
}

} // namespace

QString FieldColorRuleItem::label(FieldStyleCategory category) const
{
    switch (category) {
    case FieldStyleCategory::Number:
        if (qAbs(min - max) <= 1e-9) {
            return QString::number(min, 'f', 6).remove(QRegularExpression(QStringLiteral("\\.?0+$")));
        }
        return QStringLiteral("%1 - %2")
            .arg(QString::number(min, 'f', 6).remove(QRegularExpression(QStringLiteral("\\.?0+$"))),
                 QString::number(max, 'f', 6).remove(QRegularExpression(QStringLiteral("\\.?0+$"))));
    case FieldStyleCategory::Bool:
        return boolValue ? QStringLiteral("true") : QStringLiteral("false");
    case FieldStyleCategory::String:
        return stringValue;
    case FieldStyleCategory::Invalid:
        return QString();
    }
    return QString();
}

QVariant FieldColorRuleItem::value(FieldStyleCategory category) const
{
    switch (category) {
    case FieldStyleCategory::Number:
        return QVariant(min);
    case FieldStyleCategory::Bool:
        return QVariant(boolValue);
    case FieldStyleCategory::String:
        return QVariant(stringValue);
    case FieldStyleCategory::Invalid:
        return QVariant();
    }
    return QVariant();
}

bool FieldColorRule::isValid() const
{
    return category != FieldStyleCategory::Invalid && !colors.isEmpty();
}

void FieldColorRule::reset()
{
    category = FieldStyleCategory::Invalid;
    opacity = 100.0;
    lineWidth = 0.8;
    classLevel = 5;
    colorRampName = QStringLiteral("RdYlGn");
    rampInverted = false;
    colors.clear();
}

QString FieldColorRule::toJsonString() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("category"), static_cast<int>(category));
    obj.insert(QStringLiteral("opacity"), opacity);
    obj.insert(QStringLiteral("lineWidth"), lineWidth);
    obj.insert(QStringLiteral("classLevel"), classLevel);
    obj.insert(QStringLiteral("colorRampName"),
               colorRampName.trimmed().isEmpty() ? QStringLiteral("RdYlGn") : colorRampName);
    obj.insert(QStringLiteral("rampInverted"), rampInverted);

    QJsonArray colorArray;
    for (const FieldColorRuleItem& item : colors) {
        colorArray.append(colorRuleToJson(item, category));
    }
    obj.insert(QStringLiteral("colors"), colorArray);

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

FieldColorRule FieldColorRule::fromJsonString(const QString& jsonString, QString* errorMessage)
{
    FieldColorRule rule;
    rule.reset();

    if (jsonString.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"字段样式数据为空。");
        }
        return rule;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"字段样式 JSON 解析失败：%1").arg(parseError.errorString());
        }
        return rule;
    }

    const QJsonObject obj = doc.object();
    rule.category = static_cast<FieldStyleCategory>(obj.value(QStringLiteral("category")).toInt(-1));
    rule.opacity = obj.value(QStringLiteral("opacity")).toDouble(100.0);
    rule.lineWidth = obj.value(QStringLiteral("lineWidth")).toDouble(0.8);
    rule.classLevel = obj.value(QStringLiteral("classLevel")).toInt(5);
    const QString rampName = obj.value(QStringLiteral("colorRampName")).toString().trimmed();
    rule.colorRampName = rampName.isEmpty() ? QStringLiteral("RdYlGn") : rampName;
    rule.rampInverted = obj.value(QStringLiteral("rampInverted")).toBool(false);

    const QJsonArray colorArray = obj.value(QStringLiteral("colors")).toArray();
    for (const QJsonValue& value : colorArray) {
        if (value.isObject()) {
            rule.colors.append(colorRuleFromJson(value.toObject()));
        }
    }

    if (!rule.isValid() && errorMessage) {
        *errorMessage = QString::fromUtf8(u8"字段样式规则无效。");
    }
    return rule;
}

LayerStylePersistence& LayerStylePersistence::instance()
{
    static LayerStylePersistence store;
    return store;
}

QString LayerStylePersistence::layerStorageKey(QgsVectorLayer* layer)
{
    if (!layer || !layer->dataProvider()) {
        return QString();
    }

    QgsDataSourceUri uri(layer->dataProvider()->dataSourceUri());
    QString key = uri.table().trimmed();
    if (!key.isEmpty()) {
        return key;
    }

    key = uri.param(QStringLiteral("typename")).trimmed();
    if (!key.isEmpty()) {
        return key;
    }

    key = uri.param(QStringLiteral("typeName")).trimmed();
    if (!key.isEmpty()) {
        return key;
    }

    return layer->name().trimmed();
}

LayerStylePersistence::LayerStylePersistence(QObject* parent)
    : QObject(parent)
    , m_database(new QSqlDatabase())
{
}

LayerStylePersistence::~LayerStylePersistence()
{
    if (m_database) {
        if (m_database->isOpen()) {
            m_database->close();
        }
        *m_database = QSqlDatabase();
        delete m_database;
        m_database = nullptr;
    }
    if (QSqlDatabase::contains(kDbConnectionName)) {
        QSqlDatabase::removeDatabase(kDbConnectionName);
    }
}

bool LayerStylePersistence::initDatabase(const QString& dbPath, QString* errorMessage)
{
    if (m_initialized && m_database && m_database->isOpen() && (dbPath.isEmpty() || dbPath == m_dbPath)) {
        return true;
    }

    if (!m_database) {
        m_database = new QSqlDatabase();
    }

    if (dbPath.isEmpty()) {
        const QString dataDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data"));
        if (!QDir().mkpath(dataDir)) {
            setError(QString::fromUtf8(u8"创建样式数据目录失败：%1").arg(dataDir), errorMessage);
            return false;
        }
        m_dbPath = QDir(dataDir).filePath(QStringLiteral("layerstyles.db"));
    } else {
        m_dbPath = dbPath;
        const QFileInfo info(m_dbPath);
        if (!QDir().mkpath(info.absolutePath())) {
            setError(QString::fromUtf8(u8"创建样式数据库目录失败：%1").arg(info.absolutePath()), errorMessage);
            return false;
        }
    }

    if (QSqlDatabase::contains(kDbConnectionName)) {
        *m_database = QSqlDatabase::database(kDbConnectionName);
    } else {
        *m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kDbConnectionName);
    }
    m_database->setDatabaseName(m_dbPath);

    if (!m_database->open()) {
        setError(QString::fromUtf8(u8"打开样式数据库失败：%1").arg(m_database->lastError().text()), errorMessage);
        return false;
    }

    if (!executeSchema(errorMessage)) {
        return false;
    }

    m_initialized = true;
    return true;
}

bool LayerStylePersistence::saveFieldColorRule(int projectId,
                                               int caseId,
                                               const QString& layerKey,
                                               const QString& fieldName,
                                               const FieldColorRule& rule,
                                               QString* errorMessage)
{
    if (projectId <= 0 || caseId < 0 || layerKey.trimmed().isEmpty() || fieldName.trimmed().isEmpty()) {
        setError(QString::fromUtf8(u8"项目、方案、图层或字段信息无效，无法保存字段样式。"), errorMessage);
        return false;
    }
    if (!rule.isValid()) {
        setError(QString::fromUtf8(u8"字段样式规则无效，无法保存。"), errorMessage);
        return false;
    }
    if (!ensureDatabase(errorMessage)) {
        return false;
    }

    QSqlQuery query(*m_database);
    const QString sql = QStringLiteral(
        "INSERT OR REPLACE INTO %1 "
        "(project_id, case_id, layer_key, field_name, color_value, updated_time) "
        "VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)").arg(kTableName);
    if (!query.prepare(sql)) {
        setError(QString::fromUtf8(u8"准备保存字段样式 SQL 失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }

    query.addBindValue(projectId);
    query.addBindValue(caseId);
    query.addBindValue(layerKey.trimmed());
    query.addBindValue(fieldName.trimmed());
    query.addBindValue(rule.toJsonString());

    if (!query.exec()) {
        setError(QString::fromUtf8(u8"保存字段样式失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }

    return true;
}

bool LayerStylePersistence::loadFieldColorRule(int projectId,
                                               int caseId,
                                               const QString& layerKey,
                                               const QString& fieldName,
                                               FieldColorRule* rule,
                                               QString* errorMessage)
{
    if (rule) {
        rule->reset();
    }
    if (projectId <= 0 || caseId < 0 || layerKey.trimmed().isEmpty() || fieldName.trimmed().isEmpty()) {
        return false;
    }
    if (!ensureDatabase(errorMessage)) {
        return false;
    }

    QSqlQuery query(*m_database);
    const QString sql = QStringLiteral(
        "SELECT color_value FROM %1 "
        "WHERE project_id = ? AND case_id = ? AND layer_key = ? AND field_name = ?").arg(kTableName);
    if (!query.prepare(sql)) {
        setError(QString::fromUtf8(u8"准备读取字段样式 SQL 失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }

    query.addBindValue(projectId);
    query.addBindValue(caseId);
    query.addBindValue(layerKey.trimmed());
    query.addBindValue(fieldName.trimmed());

    if (!query.exec()) {
        setError(QString::fromUtf8(u8"读取字段样式失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }
    if (!query.next()) {
        return false;
    }

    QString parseError;
    FieldColorRule loaded = FieldColorRule::fromJsonString(query.value(0).toString(), &parseError);
    if (!loaded.isValid()) {
        setError(parseError, errorMessage);
        return false;
    }
    if (rule) {
        *rule = loaded;
    }
    return true;
}

bool LayerStylePersistence::loadLatestFieldColorRule(int projectId,
                                                     int caseId,
                                                     const QString& layerKey,
                                                     QString* fieldName,
                                                     FieldColorRule* rule,
                                                     QString* errorMessage)
{
    if (fieldName) {
        fieldName->clear();
    }
    if (rule) {
        rule->reset();
    }
    if (projectId <= 0 || caseId < 0 || layerKey.trimmed().isEmpty()) {
        return false;
    }
    if (!ensureDatabase(errorMessage)) {
        return false;
    }

    QSqlQuery query(*m_database);
    const QString sql = QStringLiteral(
        "SELECT field_name, color_value FROM %1 "
        "WHERE project_id = ? AND case_id = ? AND layer_key = ? "
        "ORDER BY updated_time DESC, id DESC LIMIT 1").arg(kTableName);
    if (!query.prepare(sql)) {
        setError(QString::fromUtf8(u8"准备读取最新字段样式 SQL 失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }

    query.addBindValue(projectId);
    query.addBindValue(caseId);
    query.addBindValue(layerKey.trimmed());

    if (!query.exec()) {
        setError(QString::fromUtf8(u8"读取最新字段样式失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }
    if (!query.next()) {
        return false;
    }

    QString parseError;
    FieldColorRule loaded = FieldColorRule::fromJsonString(query.value(1).toString(), &parseError);
    if (!loaded.isValid()) {
        setError(parseError, errorMessage);
        return false;
    }

    if (fieldName) {
        *fieldName = query.value(0).toString();
    }
    if (rule) {
        *rule = loaded;
    }
    return true;
}

bool LayerStylePersistence::deleteFieldColorRule(int projectId,
                                                 int caseId,
                                                 const QString& layerKey,
                                                 const QString& fieldName,
                                                 QString* errorMessage)
{
    if (projectId <= 0 || caseId < 0 || layerKey.trimmed().isEmpty() || fieldName.trimmed().isEmpty()) {
        setError(QString::fromUtf8(u8"项目、方案、图层或字段信息无效，无法删除字段样式。"), errorMessage);
        return false;
    }
    if (!ensureDatabase(errorMessage)) {
        return false;
    }

    QSqlQuery query(*m_database);
    const QString sql = QStringLiteral(
        "DELETE FROM %1 WHERE project_id = ? AND case_id = ? AND layer_key = ? AND field_name = ?").arg(kTableName);
    if (!query.prepare(sql)) {
        setError(QString::fromUtf8(u8"准备删除字段样式 SQL 失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }

    query.addBindValue(projectId);
    query.addBindValue(caseId);
    query.addBindValue(layerKey.trimmed());
    query.addBindValue(fieldName.trimmed());

    if (!query.exec()) {
        setError(QString::fromUtf8(u8"删除字段样式失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }
    return true;
}

bool LayerStylePersistence::deleteLayerFieldColorRules(int projectId,
                                                       int caseId,
                                                       const QString& layerKey,
                                                       QString* errorMessage)
{
    if (projectId <= 0 || caseId < 0 || layerKey.trimmed().isEmpty()) {
        setError(QString::fromUtf8(u8"项目、方案或图层信息无效，无法删除字段样式。"), errorMessage);
        return false;
    }
    if (!ensureDatabase(errorMessage)) {
        return false;
    }

    QSqlQuery query(*m_database);
    const QString sql = QStringLiteral(
        "DELETE FROM %1 WHERE project_id = ? AND case_id = ? AND layer_key = ?").arg(kTableName);
    if (!query.prepare(sql)) {
        setError(QString::fromUtf8(u8"准备删除图层字段样式 SQL 失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }

    query.addBindValue(projectId);
    query.addBindValue(caseId);
    query.addBindValue(layerKey.trimmed());

    if (!query.exec()) {
        setError(QString::fromUtf8(u8"删除图层字段样式失败：%1").arg(query.lastError().text()), errorMessage);
        return false;
    }
    return true;
}

bool LayerStylePersistence::ensureDatabase(QString* errorMessage)
{
    return initDatabase(QString(), errorMessage);
}

bool LayerStylePersistence::executeSchema(QString* errorMessage)
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS %1 ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "project_id INTEGER NOT NULL,"
            "case_id INTEGER NOT NULL DEFAULT 0,"
            "layer_key TEXT NOT NULL,"
            "field_name TEXT NOT NULL,"
            "color_value TEXT NOT NULL,"
            "created_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "updated_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "UNIQUE(project_id, case_id, layer_key, field_name))").arg(kTableName),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_symbolcolors_project_case ON %1(project_id, case_id)").arg(kTableName),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_symbolcolors_layer ON %1(layer_key)").arg(kTableName),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_symbolcolors_field ON %1(field_name)").arg(kTableName)
    };

    QSqlQuery query(*m_database);
    for (const QString& sql : statements) {
        if (!query.exec(sql)) {
            setError(QString::fromUtf8(u8"初始化字段样式数据库失败：%1").arg(query.lastError().text()), errorMessage);
            return false;
        }
    }
    return true;
}

void LayerStylePersistence::setError(const QString& error, QString* errorMessage) const
{
    if (errorMessage) {
        *errorMessage = error;
    }
    if (!error.trimmed().isEmpty()) {
        qWarning() << error;
    }
}
