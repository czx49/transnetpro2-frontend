/*****************************************************************//**
 * \file   basedatatableview.h
 * \brief  基础数据表格视图
 * \details 显示基础数据列表（复选框列表形式），支持图层选择/取消选择
 *
 * \author zhangmg
 * \date   April 2026
 * \History 初始版本
 *********************************************************************/
#ifndef BASEDATATABLEVIEW_H
#define BASEDATATABLEVIEW_H

#include "basedatamodel.h"
#include <QHash>
#include <QList>
#include <QPoint>
#include <QVariantMap>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class TNMapWidget;

enum class BaseDataNodeCategory {
    Layer = 1,
    Tool = 2,
    LayerGroup = 3
};

enum class BaseDataUserDataField {
    NodeCategory = 1,
    Name = 2,
    Value = 3,
    Val = 4,
    Level = 5,
    Type = 6,
    Url = 7
};

enum class BaseDataToolType {
    Unknown = 0,
    Open = 1,
    Url = 2,
    OpenPop = 3,
    OpenOd = 4,
    OpenDeduce = 5,
    OpenHtml = 6,
    OpenArea = 7
};

struct BaseDataLayerNodeItem {
    QString displayName;
    QVariantMap userData;
    bool checked = false;
};

/** @brief 基础数据图层树勾选快照（按 API value / WFS typename） */
struct BaseDataLayerTreeSnapshot {
    QHash<QString, Qt::CheckState> checkByTypename;
};

struct BaseDataLayerGroupNodeItem {
    QString displayName;
    QString groupValue;
    QList<BaseDataLayerNodeItem> layers;
};

struct BaseDataToolNodeItem {
    QString displayName;
    QVariantMap userData;
};

QString baseDataUserDataKey(BaseDataUserDataField field);
BaseDataToolType baseDataToolTypeFromString(const QString& typeText);

/**
 * \brief  基础数据表格视图
 * \details 显示基础数据列表（复选框列表形式），支持图层选择/取消选择
 * \author zhangmg
 * \date   April 2026
 */
class BaseDataTableView : public QWidget {
    Q_OBJECT

public:
    explicit BaseDataTableView(QWidget* parent = nullptr);
    ~BaseDataTableView() override;

    /**
     * \brief  获取数据模型
     * \return 数据模型指针
     * \author zhangmg
     * \date   April 2026
     */
    BaseDataTableModel* model() const;

    /**
     * \brief  设置数据模型
     * \param  model 数据模型指针
     * \author zhangmg
     * \date   April 2026
     */
    void setModel(BaseDataTableModel* model);

    /**
     * \brief  刷新显示
     * \author zhangmg
     * \date   April 2026
     */
    void refresh();

signals:
    /**
     * \brief  图层状态改变信号
     * \param  layerName 图层名称
     * \param  checked 是否选中
     * \author zhangmg
     * \date   April 2026
     */
    void layerStateChanged(const QString& layerName, bool checked);
    void layerStateChangedWithData(const QString& layerName, bool checked, const QVariantMap& userData);

    /** @brief 基础数据图层右键「导入」，table 为 `/api/files/drcsv` 的 table */
    void baseDataLayerCsvImportRequested(const QString& table, const QString& layerDisplayName);
    /** @brief 基础数据图层右键「导出」：`exportType` 为 xlsx 或 txt */
    void baseDataLayerExportRequested(const QString& table, const QString& exportType, const QString& layerDisplayName);
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    /** @brief 基础数据图层右键「缩放至图层」 */
    void baseDataLayerZoomToRequested(const QString& wfsTypeName, const QString& layerDisplayName);
#endif

    /**
     * \brief  工具点击信号
     * \param  toolName 工具名称
     * \author zhangmg
     * \date   April 2026
     */
    void toolClicked(const QString& toolName);
    void toolNodeClicked(const QString& toolName, const QVariantMap& userData);
    /** @brief 双击图层叶子：请求切换到该图层的数据视图 */
    void baseDataLayerDataViewRequested(const QString& wfsTypeName, const QVariantMap& userData);

private slots:
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onTreeCustomContextMenuRequested(const QPoint& pos);
    void onBaseDataLayerImportTriggered();
    void onBaseDataLayerExportTriggered();
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    void onBaseDataLayerZoomToTriggered();
#endif

private:
    /**
     * \brief  初始化UI
     * \author zhangmg
     * \date   April 2026
     */
    void initUi();

    /**
     * \brief  创建图层列表区
     * \return 图层列表区容器
     * \author zhangmg
     * \date   April 2026
     */
    void rebuildTree();
    static QVariantMap itemUserData(const QTreeWidgetItem* item);
    static bool isLayerNode(const QTreeWidgetItem* item);
    static bool isLayerGroupNode(const QTreeWidgetItem* item);
    static bool isToolNode(const QTreeWidgetItem* item);
    void propagateCheckStateToChildren(QTreeWidgetItem* item, Qt::CheckState state);
    void updateParentGroupCheckState(QTreeWidgetItem* item);
    Qt::CheckState aggregateChildCheckState(const QTreeWidgetItem* parent) const;
    static QVariantMap buildNodeUserData(BaseDataNodeCategory category,
                                         const QString& name,
                                         const QVariantMap& extraData);
    bool isTreeWidgetItemCheckBoxHit(const QTreeWidgetItem* item, const QPoint& viewportPos) const;
    void toggleLayerGroupExpanded(QTreeWidgetItem* item);
    bool ensureLayersTreeRoots();
    QTreeWidgetItem* findGroupItemByValue(const QString& groupValue) const;
    QTreeWidgetItem* findLayerItemByTypename(const QString& typenameKey,
                                             QTreeWidgetItem** outParentGroup = nullptr) const;
    void applyLayerCheckState(QTreeWidgetItem* layerItem, const BaseDataLayerTreeSnapshot& snapshot);
    void removeLayerItemAt(QTreeWidgetItem* parentGroup, int row, bool notifyUnchecked);
    QTreeWidgetItem* createGroupItem(const BaseDataLayerGroupNodeItem& group, int insertIndex);
    QTreeWidgetItem* createLayerItem(QTreeWidgetItem* parentGroup, const BaseDataLayerNodeItem& layer, int insertIndex);
    static QString layerTypenameFromItem(const QTreeWidgetItem* item);
    static QString groupValueFromItem(const QTreeWidgetItem* item);
    /** @brief 同步 m_layerGroupItems 中对应图层的 checked 字段（与 QTreeWidget 勾选态一致） */
    void syncLayerGroupItemChecked(const QString& wfsTypeName, bool checked);

public:
    void setLayerGroupItems(const QList<BaseDataLayerGroupNodeItem>& items);
    void setToolItems(const QList<BaseDataToolNodeItem>& items);
    void setTreeData(const QList<BaseDataLayerGroupNodeItem>& layerGroupItems,
                     const QList<BaseDataToolNodeItem>& toolItems);
    void clearTreeData();

    /** @brief 按 val 查找图层叶子 userData，不修改勾选态、不 emit 信号 */
    bool findLayerUserDataByVal(const QString& val, QVariantMap* outUserData) const;

    /** @brief 按 val 勾选/取消图层叶子，并发出与手动勾选相同的信号；成功时可选返回节点 userData */
    bool setLayerCheckedByVal(const QString& val, bool checked, QVariantMap* matchedUserData = nullptr);

    /** @brief 按 WFS typename（API value）勾选/取消图层叶子，并发出与手动勾选相同的信号 */
    bool setLayerCheckedByTypename(const QString& wfsTypeName, bool checked, QVariantMap* matchedUserData = nullptr);

    /** @brief 按 WFS typename 静默更新勾选态，不 emit layerStateChanged */
    bool setLayerCheckedByTypenameQuiet(const QString& wfsTypeName, bool checked);

    /** @brief 取消勾选地图上未加载的图层叶子（批量加载失败兜底） */
    void reconcileCheckedLayersWithMap(TNMapWidget* map);

    /** @brief 按 WFS typename（API value）查找已勾选图层节点 userData */
    QVariantMap findCheckedLayerUserDataByWfsTypeName(const QString& wfsTypeName) const;

    /**
     * \brief  在指定图层组下按 val 查找图层节点 userData
     * \param  val 图层 val（如 road_community）
     * \param  groupValue 图层组 groupValue（如 road）
     * \param  outUserData 输出节点 userData
     * \return 是否找到
     */
    bool findLayerUserDataByValAndGroup(const QString& val,
                                        const QString& groupValue,
                                        QVariantMap* outUserData) const;

    BaseDataLayerTreeSnapshot captureLayerCheckStates() const;
    bool syncLayerGroups(const QList<BaseDataLayerGroupNodeItem>& newGroups,
                         const BaseDataLayerTreeSnapshot& snapshot,
                         bool* layerListChanged = nullptr);

private:
    BaseDataTableModel* m_model = nullptr;
    QTreeWidget* m_treeWidget;
    QTreeWidgetItem* m_layersRootItem;
    QTreeWidgetItem* m_toolsRootItem;
    QList<BaseDataLayerGroupNodeItem> m_layerGroupItems;
    QList<BaseDataToolNodeItem> m_toolItems;
    QTreeWidgetItem* m_contextLayerItem = nullptr;
    bool m_updatingChecks = false;
};

#endif // BASEDATATABLEVIEW_H
