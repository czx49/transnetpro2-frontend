#pragma once
#include <QtCore>
#include <QtSql>

struct OfflineTable {
    QString schema, name, geometry, mode;
    QString qualified() const;
};
struct OfflinePage {
    QStringList columns;
    QVector<QVariantList> rows;
    qint64 total = 0;
};
class OfflineStore {
public:
    OfflineStore() = default;
    ~OfflineStore();
    bool openFile(const QString& path);
    bool openLocal(const QString& configPath);
    void close();
    bool isOpen() const { return db.isOpen(); }
    bool isLocalPostgres() const { return db.isOpen() && db.driverName() == "QPSQL"; }
    QString error() const { return lastError; }
    QString sourceName() const { return source; }
    QJsonObject connectionConfig() const { return config; }
    QString databasePassword() const { return password; }
    QSqlDatabase database() const { return db; }
    QList<OfflineTable> tables();
    bool page(const OfflineTable& table, const QString& filter, int pageNumber, int pageSize, OfflinePage& result);
    QJsonObject metrics(const OfflineTable& table, const QString& filter);
    bool saveScenario(const QString& name, const QString& mode, const QString& sourceName,
                      const QString& table, const QString& notes, const QJsonObject& metrics);
    static bool exportPage(const QString& path, const OfflinePage& page, QString& error);
    static QString quoteIdentifier(const QString& name);
    static QString modeFor(const QString& table);
private:
    bool activate(const QString& name, const QString& label);
    QStringList fields(const OfflineTable& table);
    QString where(const QStringList& fields, const QString& filter) const;
    void bindFilter(QSqlQuery& query, const QString& filter) const;
    QSqlDatabase db;
    QString connectionName, lastError, source, password;
    QJsonObject config;
};
