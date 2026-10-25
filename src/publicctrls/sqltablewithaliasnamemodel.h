#ifndef SQLTABLEWITHALIASNAMEMODEL_H
#define SQLTABLEWITHALIASNAMEMODEL_H

#include <QSqlTableModel>
#include <QVariantMap>

class SqlTableWithAliasNameModel : public QSqlTableModel {
    Q_OBJECT

public:
    explicit SqlTableWithAliasNameModel(QObject* parent = nullptr, QSqlDatabase db = QSqlDatabase());

    void setTable(const QString& tableName) override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    void readAliasName();

    QVariantMap m_aliasNameMap;
};

#endif // SQLTABLEWITHALIASNAMEMODEL_H
