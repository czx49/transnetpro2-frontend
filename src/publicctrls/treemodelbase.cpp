#include "treemodelbase.h"

// TreeItemBase 实现
TreeItemBase::TreeItemBase(TreeItemBase* parentItem)
    : m_parentItem(parentItem)
{
}

TreeItemBase::TreeItemBase(const TreeData& data, TreeItemBase* parent)
    : m_parentItem(parent)
    , m_itemData(data)
{
}

TreeItemBase::~TreeItemBase()
{
    qDeleteAll(m_childItems);
}

TreeItemBase* TreeItemBase::appendChild(const TreeData& data)
{
    TreeItemBase* newItem = new TreeItemBase(data, this);
    m_childItems.append(newItem);
    return newItem;
}

void TreeItemBase::moveChild(int from, int to)
{
    if (from < 0 || from >= m_childItems.size() || to < 0 || to > m_childItems.size())
        return;

    if (from == to || from == to - 1)
        return;

    TreeItemBase* item = m_childItems.takeAt(from);
    if (to > from)
        --to; // after removing earlier element, insertion index shifts left
    m_childItems.insert(to, item);
}

TreeItemBase* TreeItemBase::takeChild(int row)
{
    if (row < 0 || row >= m_childItems.size()) {
        return nullptr;
    }
    TreeItemBase* child = m_childItems.takeAt(row);
    if (child) {
        child->m_parentItem = nullptr;
    }
    return child;
}

TreeItemBase* TreeItemBase::child(int row)
{
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

QList<TreeItemBase*> TreeItemBase::childItems() const
{
    return m_childItems;
}

int TreeItemBase::childCount() const
{
    return m_childItems.count();
}

int TreeItemBase::columnCount() const
{
    return 1;
}

QVariant TreeItemBase::data(int column, int role) const
{
    if (!m_itemData.contains(column))
        return QVariant();
    return m_itemData[column].value(role);
}

void TreeItemBase::setData(int column, const QVariant& value, int role)
{
    m_itemData[column][role] = value;
}

TreeItemBase* TreeItemBase::parentItem()
{
    return m_parentItem;
}

int TreeItemBase::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<TreeItemBase*>(this));

    return 0;
}

// TreeModelBase 实现
TreeModelBase::TreeModelBase(QObject* parent)
    : QAbstractItemModel(parent)
{
}

TreeModelBase::~TreeModelBase()
{
}

int TreeModelBase::columnCount(const QModelIndex& parent) const
{
    // 模型层面只向视图报 1 列（Name），其余列仍保存在项数据中供程序访问
    Q_UNUSED(parent)
    return 1;
}

QVariant TreeModelBase::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeItemBase* item = static_cast<TreeItemBase*>(index.internalPointer());

    if (item) {
        return item->data(index.column(), role);
    }

    return QVariant();
}

Qt::ItemFlags TreeModelBase::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

QVariant TreeModelBase::headerData(int section, Qt::Orientation orientation, int role) const
{
    return QVariant();
}

QModelIndex TreeModelBase::index(int row, int column, const QModelIndex& parent) const
{
    if (!m_rootItem || !hasIndex(row, column, parent))
        return QModelIndex();

    TreeItemBase* parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem.data();
    else
        parentItem = static_cast<TreeItemBase*>(parent.internalPointer());

    TreeItemBase* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex TreeModelBase::parent(const QModelIndex& index) const
{
    if (!m_rootItem || !index.isValid())
        return QModelIndex();

    TreeItemBase* childItem = static_cast<TreeItemBase*>(index.internalPointer());
    TreeItemBase* parentItem = childItem->parentItem();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int TreeModelBase::rowCount(const QModelIndex& parent) const
{
    if (!m_rootItem)
        return 0;

    TreeItemBase* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem.data();
    else
        parentItem = static_cast<TreeItemBase*>(parent.internalPointer());

    return parentItem->childCount();
}

TreeItemBase* TreeModelBase::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        TreeItemBase* item = static_cast<TreeItemBase*>(index.internalPointer());
        if (item)
            return item;
    }
    if (m_rootItem)
        return m_rootItem.data();

    return nullptr;
}

void TreeModelBase::setRootItem(QSharedPointer<TreeItemBase> newRootItem)
{
    beginResetModel();
    m_rootItem = newRootItem;
    endResetModel();
}
