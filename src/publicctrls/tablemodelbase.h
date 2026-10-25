#ifndef TABLEMODELBASE_H
#define TABLEMODELBASE_H

#include <QAbstractTableModel>
#include <QtCore>

template <class T>
class TableModelBase : public QAbstractTableModel {
public:
    explicit TableModelBase(QObject* parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }
    ~TableModelBase()
    {
    }

public:
    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        Q_UNUSED(parent);

        return m_datas.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        Q_UNUSED(parent);
        return headers().size(); // name, path, type
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= m_datas.size() || index.row() < 0)
            return QVariant();
        const T& info = m_datas.at(index.row());
        if (role == Qt::DisplayRole) {
            return colValue(info, index.column());
        }
        return QVariant();
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole)
            return QVariant();
        if (orientation == Qt::Horizontal) {
            const QStringList hs = headers();
            if (section < hs.size()) {
                return hs[section];
            }
        }
        return QVariant();
    }

    bool rowData(int row, T& data) const
    {
        if (row < 0 || row >= m_datas.size())
            return false;

        data = m_datas[row];
        return true;
    }

    virtual const QStringList headers() const = 0;
    virtual QVariant colValue(const T& data, int col) const = 0;

    QList<T> datas() const
    {
        return m_datas;
    }

    void setDatas(const QList<T>& newDatas)
    {
        beginResetModel();
        m_datas = newDatas;
        endResetModel();
    }

private:
    QList<T> m_datas;
};
#endif // TABLEMODELBASE_H
