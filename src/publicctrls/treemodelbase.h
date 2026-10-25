#ifndef TREEMODELBASE_H
#define TREEMODELBASE_H

#include <QAbstractItemModel>
#include <QMap>
#include <QModelIndex>
#include <QMutex>
#include <QSet>
#include <QSharedPointer>
#include <QVariant>

// 树模型节点基类
// - 使用 RoleToData 存储单行中不同 role 的值（例如 Qt::DisplayRole, Qt::UserRole 等）
// - TreeData 按列索引映射到 RoleToData，支持多列多 role 的存储
class TreeItemBase {
public:
    typedef QMap<int, QVariant> RoleToData; // Role -> 值
    typedef QMap<int, RoleToData> TreeData; // 列号 -> (Role -> 值)

public:
    // 构造：可以指定父节点或使用现成的 TreeData 初始化该节点
    explicit TreeItemBase(TreeItemBase* parentItem = nullptr);
    explicit TreeItemBase(const TreeData& data, TreeItemBase* parentItem = nullptr);
    ~TreeItemBase();

public:
    // 向当前节点追加子节点，返回新创建的子节点指针
    TreeItemBase* appendChild(const TreeData& data);

    // 访问父节点/子节点信息
    TreeItemBase* parentItem();
    TreeItemBase* child(int row);
    QList<TreeItemBase*> childItems() const;
    int childCount() const;
    int columnCount() const;

    // 获取/设置指定列和 role 的数据
    QVariant data(int column, int role = Qt::DisplayRole) const;
    void setData(int column, const QVariant& value, int role = Qt::DisplayRole);
    int row() const; // 返回该节点在父节点中的行号

    // 移动子节点（在同一父节点内部重排）
    void moveChild(int from, int to);

    /** @brief 从子节点列表移除并返回指定行（不 delete） */
    TreeItemBase* takeChild(int row);

    Qt::CheckState checkState() const { return mCheckState; };
    void setCheckState(Qt::CheckState state) { mCheckState = state; };
private:
    QList<TreeItemBase*> m_childItems; // 子节点列表
    TreeData m_itemData; // 存放列->(role->value)
    TreeItemBase* m_parentItem = nullptr; // 父节点指针
    Qt::CheckState mCheckState = Qt::CheckState::Unchecked;
};

// 通用树模型基类
// - 封装了基于 TreeItemBase 的数据存储并实现 QAbstractItemModel 的核心接口
// - 子类可以通过构造和填充 m_rootItem 来提供具体树结构数据
class TreeModelBase : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit TreeModelBase(QObject* parent = nullptr);
    ~TreeModelBase();

public:
    // 基本的模型接口实现：data/flags/headerData/index/parent/rowCount/columnCount
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    // 根据 QModelIndex 获取对应的 TreeItemBase 节点
    TreeItemBase* getItem(const QModelIndex& index) const;

    // 获取根节点（仅供子类使用）
    TreeItemBase* rootItem() const { return m_rootItem.data(); }

    void setRootItem(QSharedPointer<TreeItemBase> newRootItem);

private:
    QSharedPointer<TreeItemBase> m_rootItem = nullptr; // 根节点（由子类或数据提供者填充）
};

#endif // TREEMODELBASE_H
