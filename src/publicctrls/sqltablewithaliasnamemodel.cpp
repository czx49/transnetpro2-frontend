#include "sqltablewithaliasnamemodel.h"

#include "numberdisplayutil.h"

#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>

SqlTableWithAliasNameModel::SqlTableWithAliasNameModel(QObject* parent, QSqlDatabase db)
    : QSqlTableModel(parent, db)
{
}

void SqlTableWithAliasNameModel::setTable(const QString& tableName)
{
    QSqlTableModel::setTable(tableName);
    readAliasName();
}

QVariant SqlTableWithAliasNameModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole && index.isValid()) {
        const QSqlRecord rec = record();
        if (index.column() >= 0 && index.column() < rec.count()) {
            const int fieldType = static_cast<int>(rec.field(index.column()).type());
            if (NumberDisplayUtil::isFloatingPointType(fieldType)) {
                const QVariant raw = QSqlTableModel::data(index, Qt::EditRole);
                if (raw.isValid() && !raw.isNull()) {
                    return NumberDisplayUtil::formatAttributeDisplay(raw, fieldType);
                }
            }
        }
    }
    return QSqlTableModel::data(index, role);
}

QVariant SqlTableWithAliasNameModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        const QString fieldName = QSqlTableModel::headerData(section, orientation, Qt::DisplayRole).toString();
        if (m_aliasNameMap.contains(fieldName)) {
            return m_aliasNameMap.value(fieldName);
        }
    }
    return QSqlTableModel::headerData(section, orientation, role);
}

void SqlTableWithAliasNameModel::readAliasName()
{
    if (!m_aliasNameMap.isEmpty() || tableName().isEmpty() || !database().isOpen()) {
        return;
    }

    QString bareTableName = tableName();
    const QStringList sections = bareTableName.split(QLatin1Char('.'));
    if (sections.size() > 1) {
        bareTableName = sections.last();
    }

    QSqlQuery query(database());
    const QString aliasesSql = QString(
        "SELECT a.attname, d.description "
        "FROM pg_class c "
        "JOIN pg_attribute a ON a.attrelid = c.oid "
        "LEFT JOIN pg_description d ON d.objoid = a.attrelid AND d.objsubid = a.attnum "
        "WHERE c.relname = '%1' AND a.attnum > 0 AND NOT a.attisdropped")
                                 .arg(bareTableName);
    if (!query.exec(aliasesSql)) {
        return;
    }

    while (query.next()) {
        const QString fieldName = query.value(0).toString();
        const QString alias = query.value(1).toString();
        if (!alias.isEmpty()) {
            m_aliasNameMap.insert(fieldName, alias);
        }
    }
}
