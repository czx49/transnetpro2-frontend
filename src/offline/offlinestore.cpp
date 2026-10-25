#include "offlinestore.h"
#include <windows.h>
#include <wincrypt.h>
#include <limits>

QString OfflineStore::quoteIdentifier(const QString& value) { return '"' + QString(value).replace('"', "\"\"") + '"'; }
QString OfflineTable::qualified() const {
    return (schema.isEmpty() ? QString() : OfflineStore::quoteIdentifier(schema) + '.') + OfflineStore::quoteIdentifier(name);
}
QString OfflineStore::modeFor(const QString& name) {
    if (name.startsWith("road_")) return u8"机动车";
    if (name.startsWith("bus_")) return u8"公交";
    if (name.startsWith("slow_") || name.startsWith("walk_") || name.startsWith("bike_")) return u8"慢行";
    return u8"公共数据与方案";
}
OfflineStore::~OfflineStore() { close(); }
void OfflineStore::close() {
    const QString old = connectionName;
    db.close(); db = QSqlDatabase(); connectionName.clear(); source.clear(); password.clear(); config = {};
    if (!old.isEmpty()) QSqlDatabase::removeDatabase(old);
}
bool OfflineStore::activate(const QString& name, const QString& label) {
    QSqlDatabase candidate=QSqlDatabase::database(name,false);
    if (!candidate.open()) {
        lastError = candidate.lastError().text(); candidate = QSqlDatabase(); QSqlDatabase::removeDatabase(name); return false;
    }
    close(); db = candidate; connectionName = name; source = label; lastError.clear(); return true;
}
bool OfflineStore::openFile(const QString& path) {
    const QFileInfo info(path);
    if (!info.isFile()) { lastError = u8"本地数据库文件不存在。"; return false; }
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly) || file.read(16) != QByteArray("SQLite format 3\0", 16)) {
        lastError = u8"请选择 SQLite / GeoPackage 数据库文件；PostgreSQL 的 .sql/.backup 文件需要先恢复到数据库。"; return false;
    }
    const QString name = "offline_file_" + QUuid::createUuid().toString(QUuid::Id128);
    QSqlDatabase candidate = QSqlDatabase::addDatabase("QSQLITE", name);
    candidate.setDatabaseName(info.absoluteFilePath()); candidate.setConnectOptions("QSQLITE_OPEN_READONLY");
    candidate=QSqlDatabase();
    if (!activate(name, info.absoluteFilePath())) return false;
    QSqlQuery query(db); query.exec("PRAGMA query_only=ON"); return true;
}
bool OfflineStore::openLocal(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { lastError = u8"本地数据库尚未初始化，请运行“初始化离线数据库.cmd”。"; return false; }
    QJsonParseError jsonError;
    const QJsonObject settings = QJsonDocument::fromJson(file.readAll(), &jsonError).object();
    const QString host = settings.value("host").toString();
    if (jsonError.error != QJsonParseError::NoError || !QStringList{"127.0.0.1", "localhost", "::1"}.contains(host)) {
        lastError = u8"离线版只允许连接本机 PostgreSQL，配置文件无效。"; return false;
    }
    QByteArray encrypted = QByteArray::fromBase64(settings.value("password_dpapi").toString().toLatin1());
    DATA_BLOB input{static_cast<DWORD>(encrypted.size()), reinterpret_cast<BYTE*>(encrypted.data())}, output{};
    if (encrypted.isEmpty() || !CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        lastError = u8"无法解密本机数据库凭据，请使用初始化时的 Windows 账号启动。"; return false;
    }
    const QString decoded = QString::fromUtf8(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    const QString name = "offline_pg_" + QUuid::createUuid().toString(QUuid::Id128);
    QSqlDatabase candidate = QSqlDatabase::addDatabase("QPSQL", name);
    candidate.setHostName(host); candidate.setPort(settings.value("port").toInt(5432));
    candidate.setDatabaseName(settings.value("database").toString()); candidate.setUserName(settings.value("user").toString());
    candidate.setPassword(decoded); candidate.setConnectOptions("connect_timeout=4;application_name=TransNetOffline");
    candidate=QSqlDatabase();
    if (!activate(name, settings.value("database").toString())) return false;
    config = settings; config.remove("password_dpapi"); password = decoded;
    QSqlQuery query(db); query.exec("SET statement_timeout='15s'"); return true;
}
QList<OfflineTable> OfflineStore::tables() {
    QList<OfflineTable> result;
    if (!isOpen()) return result;
    QSqlQuery query(db);
    const bool pg = isLocalPostgres();
    const QString sql = pg
        ? "SELECT table_schema,table_name FROM information_schema.tables WHERE table_schema='tnp2_data' ORDER BY table_name"
        : "SELECT '',name FROM sqlite_master WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' AND name NOT LIKE 'gpkg_%' AND name NOT LIKE 'rtree_%' ORDER BY name";
    if (!query.exec(sql)) { lastError = query.lastError().text(); return result; }
    while (query.next()) {
        OfflineTable table{query.value(0).toString(),query.value(1).toString(),QString(),QString()};
        if (table.name == "offline_meta") continue;
        table.mode = modeFor(table.name);
        QSqlQuery geometry(db);
        if (pg) {
            geometry.prepare("SELECT f_geometry_column FROM public.geometry_columns WHERE f_table_schema=:schema AND f_table_name=:table");
            geometry.bindValue(":schema",table.schema); geometry.bindValue(":table",table.name);
        } else {
            geometry.prepare("SELECT column_name FROM gpkg_geometry_columns WHERE table_name=:table");
            geometry.bindValue(":table",table.name);
        }
        if (geometry.exec() && geometry.next()) table.geometry = geometry.value(0).toString();
        if(!pg && table.geometry.isEmpty()) {
            geometry.prepare("SELECT f_geometry_column FROM geometry_columns WHERE f_table_name=:table");geometry.bindValue(":table",table.name);
            if(geometry.exec()&&geometry.next())table.geometry=geometry.value(0).toString();
        }
        result << table;
    }
    return result;
}
QStringList OfflineStore::fields(const OfflineTable& table) {
    QSqlQuery query(db); QStringList names;
    if (!query.exec("SELECT * FROM " + table.qualified() + " LIMIT 0")) { lastError=query.lastError().text(); return names; }
    const QSqlRecord record=query.record();
    for (int i=0;i<record.count();++i) names << record.fieldName(i);
    return names;
}
QString OfflineStore::where(const QStringList& names, const QString& filter) const {
    if (filter.isEmpty()) return QString();
    QStringList parts;
    for (const QString& name:names) parts << "CAST("+quoteIdentifier(name)+" AS TEXT) LIKE :needle ESCAPE '!'";
    return " WHERE ("+parts.join(" OR ")+")";
}
void OfflineStore::bindFilter(QSqlQuery& query,const QString& filter) const {
    if (filter.isEmpty()) return;
    QString text=filter; text.replace("!","!!").replace("%","!%").replace("_","!_");
    query.bindValue(":needle", "%"+text+"%");
}
bool OfflineStore::page(const OfflineTable& table,const QString& filter,int number,int size,OfflinePage& output) {
    if (!isOpen() || number<1 || size<1 || size>1000) { lastError=u8"数据源或分页参数无效。"; return false; }
    OfflinePage candidate; candidate.columns=fields(table);
    if (candidate.columns.isEmpty()) return false;
    QStringList searchable=candidate.columns; searchable.removeAll(table.geometry);
    const QString condition=where(searchable,filter);
    QSqlQuery count(db); count.prepare("SELECT count(*) FROM "+table.qualified()+condition); bindFilter(count,filter);
    if (!count.exec() || !count.next()) { lastError=count.lastError().text(); return false; }
    candidate.total=count.value(0).toLongLong();
    const QString order=candidate.columns.contains("id") ? "id" : candidate.columns.first();
    QSqlQuery rows(db);
    rows.prepare("SELECT * FROM "+table.qualified()+condition+" ORDER BY "+quoteIdentifier(order)+" LIMIT :limit OFFSET :offset");
    bindFilter(rows,filter); rows.bindValue(":limit",size); rows.bindValue(":offset",qint64(number-1)*size);
    if (!rows.exec()) { lastError=rows.lastError().text(); return false; }
    while(rows.next()) { QVariantList values; for(int i=0;i<candidate.columns.size();++i) values << rows.value(i); candidate.rows << values; }
    output=candidate; lastError.clear(); return true;
}
QJsonObject OfflineStore::metrics(const OfflineTable& table,const QString& filter) {
    const QStringList names=fields(table); if(names.isEmpty()) return {};
    QStringList search=names; search.removeAll(table.geometry);
    QStringList expressions{"count(*) AS rows"};
    if(names.contains("demand")) expressions << "sum(demand) AS total_demand";
    if(names.contains("pop")) expressions << "sum(pop) AS population";
    if(names.contains("volume")) expressions << "sum(volume) AS total_volume";
    if(names.contains("capacity")) expressions << "sum(capacity) AS total_capacity";
    if(names.contains("volume") && names.contains("capacity"))
        expressions << "avg(CAST(volume AS DOUBLE PRECISION)/NULLIF(CAST(capacity AS DOUBLE PRECISION),0)) AS mean_vc";
    QSqlQuery query(db); query.prepare("SELECT "+expressions.join(',')+" FROM "+table.qualified()+where(search,filter)); bindFilter(query,filter);
    if(!query.exec() || !query.next()) { lastError=query.lastError().text(); return {}; }
    QJsonObject result;
    for(int i=0;i<query.record().count();++i) result[query.record().fieldName(i)]=query.value(i).isNull()?QJsonValue(QJsonValue::Null):QJsonValue::fromVariant(query.value(i));
    result["field_count"]=names.size(); result["filter"]=filter; return result;
}
bool OfflineStore::saveScenario(const QString& name,const QString& mode,const QString& input,const QString& table,
                               const QString& notes,const QJsonObject& metrics) {
    if(!isLocalPostgres() || name.trimmed().isEmpty()) { lastError=u8"请连接本机离线库，并填写方案名称。"; return false; }
    QSqlQuery query(db);
    query.prepare("INSERT INTO tnp2_data.planning_scenarios(name,mode,source_name,source_table,notes,metrics_json) VALUES(:name,:mode,:source,:table,:notes,CAST(:metrics AS jsonb))");
    query.bindValue(":name",name.trimmed()); query.bindValue(":mode",mode); query.bindValue(":source",input);
    query.bindValue(":table",table); query.bindValue(":notes",notes);
    query.bindValue(":metrics",QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact)));
    if(!query.exec()) { lastError=query.lastError().text(); return false; } return true;
}
bool OfflineStore::exportPage(const QString& path,const OfflinePage& page,QString& error) {
    QSaveFile file(path); if(!file.open(QIODevice::WriteOnly)) { error=file.errorString(); return false; }
    QTextStream stream(&file); stream.setCodec("UTF-8"); stream.setGenerateByteOrderMark(true);
    auto escaped=[](QString value) { value.replace('"',"\"\""); return '"'+value+'"'; };
    QStringList header; for(const auto& column:page.columns) header<<escaped(column); stream<<header.join(',')<<"\r\n";
    for(const auto& row:page.rows) {
        QStringList values; for(const auto& value:row) values<<escaped(value.type()==QVariant::ByteArray ? "0x"+QString(value.toByteArray().toHex()) : value.toString());
        stream<<values.join(',')<<"\r\n";
    }
    stream.flush(); if(stream.status()!=QTextStream::Ok || !file.commit()) { error=file.errorString(); return false; } return true;
}
