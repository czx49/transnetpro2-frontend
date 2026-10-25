/*****************************************************************//**
 * \file   casetreemodel.h
 * \brief  方案树模型
 * \details 管理方案的树形结构，继承自 TreeModelBase
 *
 * \author zhangmg
 * \date   April 2026
 * \History 初始版本
 *********************************************************************/
#ifndef CASETREEMODEL_H
#define CASETREEMODEL_H

#include "../publicctrls/treemodelbase.h"
#include "caseitem.h"

#include <QHash>

/** @brief 方案图层树勾选快照（按 API value / typename） */
struct CaseLayerTreeSnapshot {
    QHash<QString, Qt::CheckState> checkByTypename;
};

/**
 * \brief  方案树模型
 * \details 管理方案的树形结构，支持展开/收起、复选框
 * \author zhangmg
 * \date   April 2026
 */
class CaseTreeModel : public TreeModelBase {
    Q_OBJECT

public:
    explicit CaseTreeModel(QObject* parent = nullptr);
    ~CaseTreeModel() override;

    /**
     * \brief  获取表头
     * \return 表头字符串列表
     * \author zhangmg
     * \date   April 2026
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /**
     * \brief  获取数据
     * \param  index 模型索引
     * \param  role 角色
     * \return 数据
     * \author zhangmg
     * \date   April 2026
     */
    QVariant data(const QModelIndex& index, int role) const override;

    /**
     * \brief  设置数据
     * \param  index 模型索引
     * \param  value 值
     * \param  role 角色
     * \return 是否设置成功
     * \author zhangmg
     * \date   April 2026
     */
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    /**
     * \brief  获取项目标志
     * \param  index 模型索引
     * \return 项目标志
     * \author zhangmg
     * \date   April 2026
     */
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /**
     * \brief  初始化数据
     * \param  caseList 方案列表
     * \author zhangmg
     * \date   April 2026
     */
    void initData(const QList<CaseItem>& caseList);

    /**
     * \brief  添加方案
     * \param  item 方案数据项
     * \author zhangmg
     * \date   April 2026
     */
    void addCase(const CaseItem& item);

    /**
     * \brief  删除方案
     * \param  index 方案索引
     * \author zhangmg
     * \date   April 2026
     */
    void removeCase(const QModelIndex& index);

    /**
     * \brief  更新方案
     * \param  index 方案索引
     * \param  item 新的方案数据
     * \author zhangmg
     * \date   April 2026
     */
    void updateCase(const QModelIndex& index, const CaseItem& item);

    /**
     * \brief  获取方案数据
     * \param  index 方案索引
     * \param  item 输出方案数据
     * \return 是否获取成功
     * \author zhangmg
     * \date   April 2026
     */
    bool getCaseData(const QModelIndex& index, CaseItem& item) const;

    /** @brief 按方案 id 与图层 val 查找图层 UserRole 元数据 */
    bool findLayerUserDataByVal(int caseId, const QString& val, QVariantMap* outMeta = nullptr) const;

    /** @brief 按方案 id 与图层 val 勾选/取消勾选，并返回图层元数据 */
    bool setLayerCheckedByVal(int caseId, const QString& val, bool checked, QVariantMap* outMeta = nullptr);

    /** @brief 按 typename 勾选/取消勾选，并返回图层元数据 */
    bool setLayerCheckedByTypename(int caseId, const QString& typenameKey, bool checked, QVariantMap* outMeta = nullptr);

    /** @brief 按 typename 静默更新勾选态，不 emit checkStateChanged */
    bool setLayerCheckedByTypenameQuiet(int caseId, const QString& typenameKey, bool checked);

    CaseLayerTreeSnapshot captureLayerCheckStates(int caseId) const;
    bool syncCaseLayerGroups(int caseId, const QList<LayerGroupItem>& newGroups,
                             const CaseLayerTreeSnapshot& snapshot, bool* layerListChanged = nullptr);

signals:
    /**
     * \brief  复选框状态改变信号
     * \param  index 模型索引
     * \param  state 复选框状态
     * \author zhangmg
     * \date   April 2026
     */
    void checkStateChanged(const QModelIndex& index, Qt::CheckState state);

private:
    QList<LayerItem> buildLayerItemsForCase(const CaseItem& caseItem) const;
    QList<LayerItem> buildDefaultLayerItems() const;
    void createLayerGroupNodes(TreeItemBase* caseNode, const QList<LayerGroupItem>& groups);
    void createLayerNodes(TreeItemBase* parentNode, const QList<LayerItem>& layers);
    void propagateCheckStateToChildren(TreeItemBase* item, Qt::CheckState state);
    void updateParentCheckState(TreeItemBase* groupItem);
    void emitLeafCheckStateChanged(TreeItemBase* item, Qt::CheckState state);
    TreeItemBase* findCaseNode(int caseId) const;
    TreeItemBase* findLayerNodeByVal(TreeItemBase* caseNode, const QString& val) const;
    TreeItemBase* findGroupNodeByValue(TreeItemBase* caseNode, const QString& groupValue) const;
    TreeItemBase* findLayerNodeByTypename(TreeItemBase* caseNode, const QString& typenameKey) const;
    void removeTreeItemAt(TreeItemBase* parentItem, int row, bool notifyUnchecked = true);
    void applyLayerCheckState(TreeItemBase* layerNode, const CaseLayerTreeSnapshot& snapshot);
    QModelIndex modelIndexForItem(TreeItemBase* item) const;

private:
    QList<CaseItem> m_caseList;     ///< 方案列表
};

#endif // CASETREEMODEL_H
