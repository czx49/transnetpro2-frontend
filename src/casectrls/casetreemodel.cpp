/*****************************************************************//**
 * \file   casetreemodel.cpp
 * \brief  方案树模型实现
 * \details 管理方案树结构，支持展开/收起、复选框
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "casetreemodel.h"

#include "casemenuhelper.h"

#include <QDebug>
#include <QMutableListIterator>
#include <QSet>
#include <functional>

namespace {

const char kNodeKindKey[] = "nodeKind";
const char kNodeKindGroup[] = "group";
const char kNodeKindLayer[] = "layer";
const char kGroupValueKey[] = "groupValue";

bool isLayerGroupItem(const TreeItemBase* item)
{
    if (!item) {
        return false;
    }
    return item->data(0, Qt::UserRole).toMap().value(QLatin1String(kNodeKindKey)).toString()
           == QLatin1String(kNodeKindGroup);
}

bool isLayerLeafItem(const TreeItemBase* item)
{
    if (!item) {
        return false;
    }
    const QVariantMap meta = item->data(0, Qt::UserRole).toMap();
    if (meta.value(QLatin1String(kNodeKindKey)).toString() == QLatin1String(kNodeKindLayer)) {
        return true;
    }
    return !meta.value(QStringLiteral("typename")).toString().trimmed().isEmpty();
}

QVariantMap makeGroupMeta(const QString& groupValue)
{
    QVariantMap meta;
    meta.insert(QLatin1String(kNodeKindKey), QLatin1String(kNodeKindGroup));
    meta.insert(QLatin1String(kGroupValueKey), groupValue);
    return meta;
}

QVariantMap makeLayerMeta(const LayerItem& layer)
{
    QVariantMap meta;
    meta.insert(QLatin1String(kNodeKindKey), QLatin1String(kNodeKindLayer));
    meta.insert(QStringLiteral("typename"), layer.wfsTypeName);
    meta.insert(QStringLiteral("level"), layer.level);
    meta.insert(QStringLiteral("val"), layer.val);
    meta.insert(QStringLiteral("category"), layer.category);
    return meta;
}

QString treeCaseDisplayName(const CaseItem& caseItem)
{
    const QString rawName = caseItem.parameters.value(QStringLiteral("rawName")).toString().trimmed();
    if (!rawName.isEmpty()) {
        return rawName;
    }

    const int split = caseItem.name.indexOf(QLatin1Char('-'));
    if (split >= 0) {
        const QString displayName = caseItem.name.mid(split + 1).trimmed();
        if (!displayName.isEmpty()) {
            return displayName;
        }
    }
    return caseItem.name;
}

} // namespace

CaseTreeModel::CaseTreeModel(QObject* parent)
    : TreeModelBase(parent)
{
}

CaseTreeModel::~CaseTreeModel()
{
}

QVariant CaseTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0) {
            return QString::fromUtf8(u8"名称");
        }
    }
    return QVariant();
}

QVariant CaseTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    TreeItemBase* item = getItem(index);
    if (!item) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        return item->data(index.column(), Qt::DisplayRole);
    }
    if (role == Qt::CheckStateRole && index.column() == 0) {
        return static_cast<int>(item->checkState());
    }

    return item->data(index.column(), role);
}

bool CaseTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    TreeItemBase* item = getItem(index);
    if (!item) {
        return false;
    }

    if (role == Qt::CheckStateRole) {
        Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
        if (state == Qt::PartiallyChecked && isLayerGroupItem(item)) {
            state = Qt::Checked;
        }

        item->setCheckState(state);

        if (isLayerGroupItem(item)) {
            propagateCheckStateToChildren(item, state);
            emitLeafCheckStateChanged(item, state);
            emit dataChanged(index, index, {role});
            return true;
        }

        if (isLayerLeafItem(item)) {
            if (TreeItemBase* parent = item->parentItem()) {
                if (isLayerGroupItem(parent)) {
                    updateParentCheckState(parent);
                }
            }
            emit checkStateChanged(index, state);
            emit dataChanged(index, index, {role});
            return true;
        }

        return false;
    }

    return TreeModelBase::setData(index, value, role);
}

Qt::ItemFlags CaseTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = TreeModelBase::flags(index);
    TreeItemBase* item = getItem(index);
    if (!item) {
        return flags;
    }

    const QModelIndex parent = index.parent();
    const bool isCaseNode = parent.isValid() && !parent.parent().isValid();
    const bool isGroupUnderCase = parent.isValid() && !parent.parent().isValid() && isLayerGroupItem(item);
    const bool isLayerUnderGroup =
        parent.isValid() && parent.parent().isValid() && !parent.parent().parent().isValid() && isLayerLeafItem(item);
    const bool isLegacyLayerUnderCase =
        parent.isValid() && !parent.parent().isValid() && isLayerLeafItem(item) && !isLayerGroupItem(item);

    if (isGroupUnderCase && item->childCount() > 0) {
        flags |= Qt::ItemIsUserCheckable;
    } else if (isLayerUnderGroup || isLegacyLayerUnderCase) {
        flags |= Qt::ItemIsUserCheckable;
    }

    Q_UNUSED(isCaseNode)
    return flags;
}

void CaseTreeModel::initData(const QList<CaseItem>& caseList)
{
    beginResetModel();

    TreeItemBase* root = new TreeItemBase();
    for (const CaseItem& caseItem : caseList) {
        TreeItemBase::TreeData caseData;
        caseData[0][Qt::DisplayRole] = treeCaseDisplayName(caseItem);
        TreeItemBase* caseNode = root->appendChild(caseData);
        caseNode->setData(0, caseItem.id, Qt::UserRole);

        if (!caseItem.caseLayerGroups.isEmpty()) {
            createLayerGroupNodes(caseNode, caseItem.caseLayerGroups);
        } else {
            createLayerNodes(caseNode, buildLayerItemsForCase(caseItem));
        }
    }

    setRootItem(QSharedPointer<TreeItemBase>(root));
    m_caseList = caseList;
    endResetModel();
}

void CaseTreeModel::addCase(const CaseItem& item)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());

    TreeItemBase* root = rootItem();
    TreeItemBase::TreeData data;
    data[0][Qt::DisplayRole] = treeCaseDisplayName(item);
    TreeItemBase* caseNode = root->appendChild(data);
    caseNode->setData(0, item.id, Qt::UserRole);

    if (!item.caseLayerGroups.isEmpty()) {
        createLayerGroupNodes(caseNode, item.caseLayerGroups);
    } else {
        createLayerNodes(caseNode, buildLayerItemsForCase(item));
    }

    m_caseList.append(item);
    endInsertRows();
}

void CaseTreeModel::removeCase(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    TreeItemBase* item = getItem(index);
    if (!item || !item->parentItem() || item->parentItem() != rootItem()) {
        return;
    }

    const int row = item->row();
    beginRemoveRows(QModelIndex(), row, row);

    const int caseId = item->data(0, Qt::UserRole).toInt();
    QMutableListIterator<CaseItem> it(m_caseList);
    while (it.hasNext()) {
        if (it.next().id == caseId) {
            it.remove();
        }
    }

    TreeItemBase* parent = item->parentItem();
    delete parent->takeChild(row);
    endRemoveRows();
}

void CaseTreeModel::updateCase(const QModelIndex& index, const CaseItem& item)
{
    if (!index.isValid()) {
        return;
    }

    TreeItemBase* treeItem = getItem(index);
    if (!treeItem) {
        return;
    }

    treeItem->setData(0, treeCaseDisplayName(item), Qt::DisplayRole);

    const int caseId = treeItem->data(0, Qt::UserRole).toInt();
    for (CaseItem& caseInfo : m_caseList) {
        if (caseInfo.id == caseId) {
            caseInfo = item;
            break;
        }
    }

    emit dataChanged(index, index);
}

bool CaseTreeModel::getCaseData(const QModelIndex& index, CaseItem& item) const
{
    if (!index.isValid()) {
        return false;
    }

    TreeItemBase* treeItem = getItem(index);
    if (!treeItem) {
        return false;
    }

    const int caseId = treeItem->data(0, Qt::UserRole).toInt();
    for (const CaseItem& caseInfo : m_caseList) {
        if (caseInfo.id == caseId) {
            item = caseInfo;
            return true;
        }
    }

    return false;
}

void CaseTreeModel::createLayerGroupNodes(TreeItemBase* caseNode, const QList<LayerGroupItem>& groups)
{
    for (const LayerGroupItem& group : groups) {
        TreeItemBase::TreeData groupData;
        groupData[0][Qt::DisplayRole] = group.name;
        groupData[0][Qt::UserRole] = makeGroupMeta(group.value);
        TreeItemBase* groupNode = caseNode->appendChild(groupData);

        if (!group.layers.isEmpty()) {
            groupNode->setCheckState(Qt::Unchecked);
            createLayerNodes(groupNode, group.layers);
        }
    }
}

void CaseTreeModel::createLayerNodes(TreeItemBase* parentNode, const QList<LayerItem>& layers)
{
    for (const LayerItem& layer : layers) {
        TreeItemBase::TreeData data;
        data[0][Qt::DisplayRole] = layer.name;
        data[0][Qt::UserRole] = makeLayerMeta(layer);

        TreeItemBase* layerNode = parentNode->appendChild(data);
        layerNode->setCheckState(layer.checked ? Qt::Checked : Qt::Unchecked);
    }
}

void CaseTreeModel::propagateCheckStateToChildren(TreeItemBase* item, Qt::CheckState state)
{
    if (!item) {
        return;
    }

    const int childCount = item->childCount();
    for (int i = 0; i < childCount; ++i) {
        TreeItemBase* child = item->child(i);
        if (!child) {
            continue;
        }
        child->setCheckState(state);
        if (isLayerGroupItem(child)) {
            propagateCheckStateToChildren(child, state);
        }
        if (child->parentItem() != nullptr) {
            const QModelIndex idx = createIndex(child->row(), 0, child);
            emit dataChanged(idx, idx, {Qt::CheckStateRole});
        }
    }
}

void CaseTreeModel::updateParentCheckState(TreeItemBase* groupItem)
{
    if (!groupItem || !isLayerGroupItem(groupItem)) {
        return;
    }

    const int total = groupItem->childCount();
    if (total == 0) {
        return;
    }

    int checkedCount = 0;
    int uncheckedCount = 0;
    for (int i = 0; i < total; ++i) {
        TreeItemBase* child = groupItem->child(i);
        if (!child || !isLayerLeafItem(child)) {
            continue;
        }
        if (child->checkState() == Qt::PartiallyChecked) {
            groupItem->setCheckState(Qt::PartiallyChecked);
            if (groupItem->parentItem() != nullptr) {
                const QModelIndex idx = createIndex(groupItem->row(), 0, groupItem);
                emit dataChanged(idx, idx, {Qt::CheckStateRole});
            }
            return;
        }
        if (child->checkState() == Qt::Checked) {
            ++checkedCount;
        } else {
            ++uncheckedCount;
        }
    }

    const int leafTotal = checkedCount + uncheckedCount;
    Qt::CheckState newState = Qt::PartiallyChecked;
    if (leafTotal > 0 && checkedCount == leafTotal) {
        newState = Qt::Checked;
    } else if (leafTotal > 0 && uncheckedCount == leafTotal) {
        newState = Qt::Unchecked;
    } else if (leafTotal == 0) {
        newState = Qt::Unchecked;
    }

    groupItem->setCheckState(newState);
    if (groupItem->parentItem() != nullptr) {
        const QModelIndex idx = createIndex(groupItem->row(), 0, groupItem);
        emit dataChanged(idx, idx, {Qt::CheckStateRole});
    }
}

void CaseTreeModel::emitLeafCheckStateChanged(TreeItemBase* item, Qt::CheckState state)
{
    if (!item) {
        return;
    }

    const int childCount = item->childCount();
    for (int i = 0; i < childCount; ++i) {
        TreeItemBase* child = item->child(i);
        if (!child) {
            continue;
        }
        if (isLayerLeafItem(child)) {
            const QModelIndex idx = modelIndexForItem(child);
            if (idx.isValid()) {
                emit checkStateChanged(idx, state);
            }
        } else if (isLayerGroupItem(child)) {
            emitLeafCheckStateChanged(child, state);
        }
    }
}

QList<LayerItem> CaseTreeModel::buildLayerItemsForCase(const CaseItem& caseItem) const
{
    if (!caseItem.caseLayers.isEmpty()) {
        return caseItem.caseLayers;
    }
    if (!caseItem.caseLayerGroups.isEmpty()) {
        return flattenLayerGroups(caseItem.caseLayerGroups);
    }

    QList<LayerItem> layers;
    const auto appendLayers = [&layers](const QVariantMap& params, const QString& key, const QString& category) {
        const QStringList values = params.value(key).toStringList();
        for (const QString& value : values) {
            const QString name = value.trimmed();
            if (name.isEmpty()) {
                continue;
            }
            layers.append({name, category, QString(), QString(), 0, true});
        }
    };

    appendLayers(caseItem.parameters, QStringLiteral("roadLayers"), QStringLiteral("road"));
    appendLayers(caseItem.parameters, QStringLiteral("busLayers"), QStringLiteral("bus"));
    appendLayers(caseItem.parameters, QStringLiteral("otherLayers"), QStringLiteral("other"));

    if (layers.isEmpty()) {
        return buildDefaultLayerItems();
    }
    return layers;
}

QList<LayerItem> CaseTreeModel::buildDefaultLayerItems() const
{
    return {
        {QString::fromUtf8(u8"点层（道路）"), QStringLiteral("road"), QString(), QString(), 0, true},
        {QString::fromUtf8(u8"路段图层（道路）"), QStringLiteral("road"), QString(), QString(), 0, true},
        {QString::fromUtf8(u8"小区图层（道路）"), QStringLiteral("road"), QString(), QString(), 0, true},
        {QString::fromUtf8(u8"点层（公交）"), QStringLiteral("bus"), QString(), QString(), 0, true},
        {QString::fromUtf8(u8"路段图层（公交）"), QStringLiteral("bus"), QString(), QString(), 0, true},
        {QString::fromUtf8(u8"小区图层（公交）"), QStringLiteral("bus"), QString(), QString(), 0, true},
        {QString::fromUtf8(u8"其他"), QStringLiteral("other"), QString(), QString(), 0, true}
    };
}

TreeItemBase* CaseTreeModel::findCaseNode(int caseId) const
{
    if (caseId <= 0) {
        return nullptr;
    }
    TreeItemBase* root = rootItem();
    if (!root) {
        return nullptr;
    }
    for (int i = 0; i < root->childCount(); ++i) {
        TreeItemBase* caseNode = root->child(i);
        if (!caseNode) {
            continue;
        }
        if (caseNode->data(0, Qt::UserRole).toInt() == caseId) {
            return caseNode;
        }
    }
    return nullptr;
}

TreeItemBase* CaseTreeModel::findLayerNodeByVal(TreeItemBase* caseNode, const QString& val) const
{
    if (!caseNode) {
        return nullptr;
    }
    const QString targetVal = val.trimmed().toLower();
    if (targetVal.isEmpty()) {
        return nullptr;
    }

    const auto matchLayer = [&](TreeItemBase* item) -> TreeItemBase* {
        if (!item || !isLayerLeafItem(item)) {
            return nullptr;
        }
        const QVariantMap meta = item->data(0, Qt::UserRole).toMap();
        if (meta.value(QStringLiteral("val")).toString().trimmed().toLower() == targetVal) {
            return item;
        }
        return nullptr;
    };

    for (int i = 0; i < caseNode->childCount(); ++i) {
        TreeItemBase* child = caseNode->child(i);
        if (!child) {
            continue;
        }
        if (TreeItemBase* hit = matchLayer(child)) {
            return hit;
        }
        if (isLayerGroupItem(child)) {
            for (int j = 0; j < child->childCount(); ++j) {
                if (TreeItemBase* layerHit = matchLayer(child->child(j))) {
                    return layerHit;
                }
            }
        }
    }
    return nullptr;
}

bool CaseTreeModel::findLayerUserDataByVal(int caseId, const QString& val, QVariantMap* outMeta) const
{
    TreeItemBase* caseNode = findCaseNode(caseId);
    if (!caseNode) {
        return false;
    }

    TreeItemBase* layerNode = findLayerNodeByVal(caseNode, val);
    if (!layerNode) {
        return false;
    }

    if (outMeta) {
        *outMeta = layerNode->data(0, Qt::UserRole).toMap();
    }
    return true;
}

bool CaseTreeModel::setLayerCheckedByVal(int caseId, const QString& val, bool checked, QVariantMap* outMeta)
{
    TreeItemBase* caseNode = findCaseNode(caseId);
    if (!caseNode) {
        return false;
    }

    TreeItemBase* layerNode = findLayerNodeByVal(caseNode, val);
    if (!layerNode) {
        return false;
    }

    const QModelIndex layerIdx = modelIndexForItem(layerNode);
    if (!layerIdx.isValid()) {
        return false;
    }

    const Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    if (layerNode->checkState() == state) {
        if (checked) {
            emit checkStateChanged(layerIdx, state);
        }
        if (outMeta) {
            *outMeta = layerIdx.data(Qt::UserRole).toMap();
        }
        return true;
    }

    if (!setData(layerIdx, static_cast<int>(state), Qt::CheckStateRole)) {
        return false;
    }

    if (outMeta) {
        *outMeta = layerIdx.data(Qt::UserRole).toMap();
    }
    return true;
}

bool CaseTreeModel::setLayerCheckedByTypename(int caseId, const QString& typenameKey, bool checked, QVariantMap* outMeta)
{
    TreeItemBase* caseNode = findCaseNode(caseId);
    if (!caseNode) {
        return false;
    }

    TreeItemBase* layerNode = findLayerNodeByTypename(caseNode, typenameKey);
    if (!layerNode) {
        return false;
    }

    const QModelIndex layerIdx = modelIndexForItem(layerNode);
    if (!layerIdx.isValid()) {
        return false;
    }

    const Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    if (layerNode->checkState() == state) {
        if (checked) {
            emit checkStateChanged(layerIdx, state);
        }
        if (outMeta) {
            *outMeta = layerIdx.data(Qt::UserRole).toMap();
        }
        return true;
    }

    if (!setData(layerIdx, static_cast<int>(state), Qt::CheckStateRole)) {
        return false;
    }

    if (outMeta) {
        *outMeta = layerIdx.data(Qt::UserRole).toMap();
    }
    return true;
}

bool CaseTreeModel::setLayerCheckedByTypenameQuiet(int caseId, const QString& typenameKey, bool checked)
{
    TreeItemBase* caseNode = findCaseNode(caseId);
    if (!caseNode) {
        return false;
    }

    TreeItemBase* layerNode = findLayerNodeByTypename(caseNode, typenameKey);
    if (!layerNode) {
        return false;
    }

    const Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    if (layerNode->checkState() == state) {
        return true;
    }

    layerNode->setCheckState(state);
    if (TreeItemBase* parent = layerNode->parentItem()) {
        if (isLayerGroupItem(parent)) {
            updateParentCheckState(parent);
        }
    }

    const QModelIndex idx = modelIndexForItem(layerNode);
    if (idx.isValid()) {
        emit dataChanged(idx, idx, {Qt::CheckStateRole});
    }
    return true;
}

QModelIndex CaseTreeModel::modelIndexForItem(TreeItemBase* item) const
{
    if (!item || !rootItem() || item == rootItem()) {
        return QModelIndex();
    }

    TreeItemBase* parentItem = item->parentItem();
    if (!parentItem) {
        return QModelIndex();
    }

    if (parentItem == rootItem()) {
        return index(item->row(), 0, QModelIndex());
    }

    const QModelIndex parentIdx = modelIndexForItem(parentItem);
    if (!parentIdx.isValid()) {
        return QModelIndex();
    }
    return index(item->row(), 0, parentIdx);
}

TreeItemBase* CaseTreeModel::findGroupNodeByValue(TreeItemBase* caseNode, const QString& groupValue) const
{
    if (!caseNode) {
        return nullptr;
    }
    const QString target = groupValue.trimmed();
    if (target.isEmpty()) {
        return nullptr;
    }
    for (int i = 0; i < caseNode->childCount(); ++i) {
        TreeItemBase* child = caseNode->child(i);
        if (!child || !isLayerGroupItem(child)) {
            continue;
        }
        if (child->data(0, Qt::UserRole).toMap().value(QLatin1String(kGroupValueKey)).toString() == target) {
            return child;
        }
    }
    return nullptr;
}

TreeItemBase* CaseTreeModel::findLayerNodeByTypename(TreeItemBase* caseNode, const QString& typenameKey) const
{
    if (!caseNode) {
        return nullptr;
    }
    const QString target = typenameKey.trimmed();
    if (target.isEmpty()) {
        return nullptr;
    }

    const auto matchLayer = [&](TreeItemBase* item) -> TreeItemBase* {
        if (!item || !isLayerLeafItem(item)) {
            return nullptr;
        }
        const QString tn = item->data(0, Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
        return tn == target ? item : nullptr;
    };

    for (int i = 0; i < caseNode->childCount(); ++i) {
        TreeItemBase* child = caseNode->child(i);
        if (!child) {
            continue;
        }
        if (TreeItemBase* hit = matchLayer(child)) {
            return hit;
        }
        if (isLayerGroupItem(child)) {
            for (int j = 0; j < child->childCount(); ++j) {
                if (TreeItemBase* layerHit = matchLayer(child->child(j))) {
                    return layerHit;
                }
            }
        }
    }
    return nullptr;
}

void CaseTreeModel::removeTreeItemAt(TreeItemBase* parentItem, int row, bool notifyUnchecked)
{
    if (!parentItem || row < 0 || row >= parentItem->childCount()) {
        return;
    }

    TreeItemBase* child = parentItem->child(row);
    if (!child) {
        return;
    }

    if (notifyUnchecked) {
        if (isLayerLeafItem(child) && child->checkState() == Qt::Checked) {
            const QModelIndex layerIdx = modelIndexForItem(child);
            if (layerIdx.isValid()) {
                emit checkStateChanged(layerIdx, Qt::Unchecked);
            }
        } else if (isLayerGroupItem(child)) {
            emitLeafCheckStateChanged(child, Qt::Unchecked);
        }
    }

    const QModelIndex parentIdx = (parentItem == rootItem())
        ? QModelIndex()
        : createIndex(parentItem->row(), 0, parentItem);

    beginRemoveRows(parentIdx, row, row);
    delete parentItem->takeChild(row);
    endRemoveRows();
}

void CaseTreeModel::applyLayerCheckState(TreeItemBase* layerNode, const CaseLayerTreeSnapshot& snapshot)
{
    if (!layerNode || !isLayerLeafItem(layerNode)) {
        return;
    }

    const QString tn = layerNode->data(0, Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
    Qt::CheckState state = Qt::Unchecked;
    if (!tn.isEmpty() && snapshot.checkByTypename.contains(tn)) {
        state = snapshot.checkByTypename.value(tn);
    }

    if (layerNode->checkState() == state) {
        return;
    }

    layerNode->setCheckState(state);
    const QModelIndex idx = createIndex(layerNode->row(), 0, layerNode);
    emit dataChanged(idx, idx, {Qt::CheckStateRole});
}

CaseLayerTreeSnapshot CaseTreeModel::captureLayerCheckStates(int caseId) const
{
    CaseLayerTreeSnapshot snapshot;
    TreeItemBase* caseNode = findCaseNode(caseId);
    if (!caseNode) {
        return snapshot;
    }

    std::function<void(TreeItemBase*)> walk;
    walk = [&](TreeItemBase* node) {
        if (!node) {
            return;
        }
        if (isLayerLeafItem(node)) {
            const QString tn = node->data(0, Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
            if (!tn.isEmpty()) {
                snapshot.checkByTypename.insert(tn, node->checkState());
            }
            return;
        }
        for (int i = 0; i < node->childCount(); ++i) {
            walk(node->child(i));
        }
    };

    for (int i = 0; i < caseNode->childCount(); ++i) {
        walk(caseNode->child(i));
    }
    return snapshot;
}

bool CaseTreeModel::syncCaseLayerGroups(int caseId, const QList<LayerGroupItem>& newGroups,
                                        const CaseLayerTreeSnapshot& snapshot, bool* layerListChanged)
{
    TreeItemBase* caseNode = findCaseNode(caseId);
    if (!caseNode) {
        return false;
    }

    QSet<QString> currentTypenames;
    std::function<void(TreeItemBase*)> collectTypenames;
    collectTypenames = [&](TreeItemBase* parent) {
        for (int i = 0; i < parent->childCount(); ++i) {
            TreeItemBase* child = parent->child(i);
            if (!child) {
                continue;
            }
            if (isLayerLeafItem(child)) {
                const QString tn =
                    child->data(0, Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
                if (!tn.isEmpty()) {
                    currentTypenames.insert(tn);
                }
            } else if (isLayerGroupItem(child)) {
                collectTypenames(child);
            }
        }
    };
    collectTypenames(caseNode);

    QSet<QString> desiredTypenames;
    QSet<QString> desiredGroupValues;
    for (const LayerGroupItem& group : newGroups) {
        desiredGroupValues.insert(group.value);
        for (const LayerItem& layer : group.layers) {
            const QString tn = layer.wfsTypeName.trimmed();
            if (!tn.isEmpty()) {
                desiredTypenames.insert(tn);
            }
        }
    }

    if (layerListChanged) {
        *layerListChanged = currentTypenames != desiredTypenames;
    }

    QList<QPair<TreeItemBase*, int>> orphanLayers;
    std::function<void(TreeItemBase*)> collectOrphans;
    collectOrphans = [&](TreeItemBase* parent) {
        for (int i = parent->childCount() - 1; i >= 0; --i) {
            TreeItemBase* child = parent->child(i);
            if (!child) {
                continue;
            }
            if (isLayerLeafItem(child)) {
                const QString tn = child->data(0, Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
                if (!desiredTypenames.contains(tn)) {
                    orphanLayers.append(qMakePair(parent, i));
                }
            } else if (isLayerGroupItem(child)) {
                collectOrphans(child);
            }
        }
    };
    collectOrphans(caseNode);

    for (const auto& pair : orphanLayers) {
        removeTreeItemAt(pair.first, pair.second, true);
    }

    for (int i = caseNode->childCount() - 1; i >= 0; --i) {
        TreeItemBase* child = caseNode->child(i);
        if (!child || !isLayerGroupItem(child)) {
            continue;
        }
        const QString groupValue =
            child->data(0, Qt::UserRole).toMap().value(QLatin1String(kGroupValueKey)).toString();
        if (!desiredGroupValues.contains(groupValue)) {
            removeTreeItemAt(caseNode, i, true);
        }
    }

    const QModelIndex caseIdx = createIndex(caseNode->row(), 0, caseNode);
    for (int gi = 0; gi < newGroups.size(); ++gi) {
        const LayerGroupItem& group = newGroups.at(gi);
        TreeItemBase* groupNode = findGroupNodeByValue(caseNode, group.value);
        if (!groupNode) {
            const int insertRow = qMin(gi, caseNode->childCount());
            beginInsertRows(caseIdx, insertRow, insertRow);
            TreeItemBase::TreeData groupData;
            groupData[0][Qt::DisplayRole] = group.name;
            groupData[0][Qt::UserRole] = makeGroupMeta(group.value);
            groupNode = caseNode->appendChild(groupData);
            if (groupNode->row() != insertRow) {
                caseNode->moveChild(groupNode->row(), insertRow);
            }
            endInsertRows();
            groupNode->setCheckState(Qt::Unchecked);
        } else {
            groupNode->setData(0, group.name, Qt::DisplayRole);
            if (groupNode->row() != gi) {
                caseNode->moveChild(groupNode->row(), gi);
            }
        }

        QSet<QString> groupDesiredTypenames;
        for (const LayerItem& layer : group.layers) {
            groupDesiredTypenames.insert(layer.wfsTypeName.trimmed());
        }

        const QModelIndex groupIdx = createIndex(groupNode->row(), 0, groupNode);
        for (int li = 0; li < group.layers.size(); ++li) {
            const LayerItem& layer = group.layers.at(li);
            const QString tn = layer.wfsTypeName.trimmed();
            if (tn.isEmpty()) {
                continue;
            }

            TreeItemBase* layerNode = findLayerNodeByTypename(caseNode, tn);
            if (layerNode && layerNode->parentItem() != groupNode) {
                const int oldRow = layerNode->row();
                TreeItemBase* oldParent = layerNode->parentItem();
                removeTreeItemAt(oldParent, oldRow, false);
                layerNode = nullptr;
            }

            if (!layerNode) {
                const int insertRow = qMin(li, groupNode->childCount());
                beginInsertRows(groupIdx, insertRow, insertRow);
                TreeItemBase::TreeData data;
                data[0][Qt::DisplayRole] = layer.name;
                data[0][Qt::UserRole] = makeLayerMeta(layer);
                layerNode = groupNode->appendChild(data);
                if (layerNode->row() != insertRow) {
                    groupNode->moveChild(layerNode->row(), insertRow);
                }
                endInsertRows();
            } else {
                layerNode->setData(0, layer.name, Qt::DisplayRole);
                layerNode->setData(0, makeLayerMeta(layer), Qt::UserRole);
                if (layerNode->parentItem() == groupNode && layerNode->row() != li) {
                    groupNode->moveChild(layerNode->row(), li);
                }
                const QModelIndex layerModelIdx = createIndex(layerNode->row(), 0, layerNode);
                emit dataChanged(layerModelIdx, layerModelIdx, {Qt::DisplayRole, Qt::UserRole});
            }

            applyLayerCheckState(layerNode, snapshot);
        }

        for (int i = groupNode->childCount() - 1; i >= 0; --i) {
            TreeItemBase* child = groupNode->child(i);
            if (!child || !isLayerLeafItem(child)) {
                continue;
            }
            const QString tn = child->data(0, Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
            if (!groupDesiredTypenames.contains(tn)) {
                removeTreeItemAt(groupNode, i, true);
            }
        }

        updateParentCheckState(groupNode);
    }

    for (CaseItem& caseInfo : m_caseList) {
        if (caseInfo.id == caseId) {
            caseInfo.caseLayerGroups = newGroups;
            caseInfo.caseLayers = flattenLayerGroups(newGroups);
            for (LayerItem& layer : caseInfo.caseLayers) {
                const QString tn = layer.wfsTypeName.trimmed();
                if (snapshot.checkByTypename.contains(tn)) {
                    layer.checked = snapshot.checkByTypename.value(tn) == Qt::Checked;
                }
            }
            break;
        }
    }

    return true;
}
