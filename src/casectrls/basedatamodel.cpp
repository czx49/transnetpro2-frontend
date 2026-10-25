/*****************************************************************//**
 * \file   basedatamodel.cpp
 * \brief  基础数据模型实现
 * \details 管理基础数据的列表，支持复选框
 *
 * \author zhangmg
 * \date   April 2026
 * \History 初始版本
 *********************************************************************/
#include "basedatamodel.h"

BaseDataTableModel::BaseDataTableModel(QObject* parent)
    : TableModelBase<BaseDataItem>(parent)
{
}

BaseDataTableModel::~BaseDataTableModel()
{
}

const QStringList BaseDataTableModel::headers() const
{
    static const QStringList headers = {
        "", u8"数据名称", u8"数据类型", u8"数据来源", u8"状态", u8"创建时间"
    };
    return headers;
}

QVariant BaseDataTableModel::colValue(const BaseDataItem& data, int col) const
{
    switch (col) {
        case 1: return data.name;
        case 2: return data.type;
        case 3: return data.source;
        case 4: return data.status;
        case 5: return data.createTime.toString("yyyy-MM-dd hh:mm:ss");
        default: return QVariant();
    }
}

QVariant BaseDataTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) {
        return QVariant();
    }

    BaseDataItem item;
    if (!rowData(index.row(), item)) {
        return QVariant();
    }

    if (role == Qt::CheckStateRole && index.column() == 0) {
        return item.checked ? Qt::Checked : Qt::Unchecked;
    }

    return TableModelBase::data(index, role);
}

bool BaseDataTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= rowCount()) {
        return false;
    }

    if (role == Qt::CheckStateRole && index.column() == 0) {
        bool checked = value.toInt() == Qt::Checked;
        setChecked(index.row(), checked);
        return true;
    }

    return TableModelBase::setData(index, value, role);
}

Qt::ItemFlags BaseDataTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = TableModelBase::flags(index);
    if (index.isValid() && index.column() == 0) {
        flags |= Qt::ItemIsUserCheckable;
    }
    return flags;
}

void BaseDataTableModel::addBaseData(const BaseDataItem& item)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    QList<BaseDataItem> items = datas();
    items.append(item);
    setDatas(items);
    endInsertRows();
}

void BaseDataTableModel::removeBaseData(int index)
{
    if (index < 0 || index >= rowCount()) {
        return;
    }
    beginRemoveRows(QModelIndex(), index, index);
    QList<BaseDataItem> items = datas();
    items.removeAt(index);
    setDatas(items);
    endRemoveRows();
}

void BaseDataTableModel::updateBaseData(int index, const BaseDataItem& item)
{
    if (index < 0 || index >= rowCount()) {
        return;
    }
    QList<BaseDataItem> items = datas();
    items[index] = item;
    setDatas(items);
    emit dataChanged(createIndex(index, 0), createIndex(index, columnCount() - 1));
}

void BaseDataTableModel::setChecked(int index, bool checked)
{
    if (index < 0 || index >= rowCount()) {
        return;
    }
    QList<BaseDataItem> items = datas();
    items[index].checked = checked;
    setDatas(items);
    emit checkStateChanged(createIndex(index, 0), checked);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0));
}

QList<BaseDataItem> BaseDataTableModel::getCheckedItems() const
{
    QList<BaseDataItem> result;
    QList<BaseDataItem> items = datas();
    for (const auto& item : items) {
        if (item.checked) {
            result.append(item);
        }
    }
    return result;
}
