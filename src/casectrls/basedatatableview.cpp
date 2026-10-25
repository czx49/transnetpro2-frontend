/*****************************************************************//**
 * \file   basedatatableview.cpp
 * \brief  基础数据侧栏实现
 * \details 显示图层勾选和工具列表
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "basedatatableview.h"

#include "casedrcsvhelper.h"
#include "casewfsmapcontroller.h"
#include "../publicctrls/messagedialog.h"
#include "../publicctrls/phase2treeutil.h"
#include "../tnmapwidget/tnmapwidget.h"

#include <QAction>
#include <QHash>
#include <QHeaderView>
#include <QMenu>
#include <QCursor>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVector>
#include <QVBoxLayout>

namespace {
constexpr int kUserDataRole = Qt::UserRole;

static const char kBaseDataTreeMenuQss[] =
    "QMenu {"
    "   background-color: #1a2639;"
    "   color: rgba(255,255,255,0.92);"
    "   border: 1px solid rgba(255,255,255,0.12);"
    "}"
    "QMenu::item {"
    "   padding: 8px 20px;"
    "}"
    "QMenu::item:selected {"
    "   background-color: rgba(33,150,243,0.30);"
    "}";
} // namespace

QString baseDataUserDataKey(BaseDataUserDataField field)
{
    switch (field) {
        case BaseDataUserDataField::NodeCategory: return QStringLiteral("nodeCategory");
        case BaseDataUserDataField::Name: return QStringLiteral("name");
        case BaseDataUserDataField::Value: return QStringLiteral("value");
        case BaseDataUserDataField::Val: return QStringLiteral("val");
        case BaseDataUserDataField::Level: return QStringLiteral("level");
        case BaseDataUserDataField::Type: return QStringLiteral("type");
        case BaseDataUserDataField::Url: return QStringLiteral("url");
        default: return QString();
    }
}

BaseDataToolType baseDataToolTypeFromString(const QString& typeText)
{
    const QString normalized = typeText.trimmed().toLower();
    if (normalized == QStringLiteral("open")) {
        return BaseDataToolType::Open;
    }
    if (normalized == QStringLiteral("url")) {
        return BaseDataToolType::Url;
    }
    if (normalized == QStringLiteral("openpop")) {
        return BaseDataToolType::OpenPop;
    }
    if (normalized == QStringLiteral("openod")) {
        return BaseDataToolType::OpenOd;
    }
    if (normalized == QStringLiteral("opendeduce")) {
        return BaseDataToolType::OpenDeduce;
    }
    if (normalized == QStringLiteral("openhtml")) {
        return BaseDataToolType::OpenHtml;
    }
    if (normalized == QStringLiteral("openarea")) {
        return BaseDataToolType::OpenArea;
    }
    return BaseDataToolType::Unknown;
}

BaseDataTableView::BaseDataTableView(QWidget* parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_treeWidget(nullptr)
    , m_layersRootItem(nullptr)
    , m_toolsRootItem(nullptr)
{
    initUi();
    refresh();
}

BaseDataTableView::~BaseDataTableView()
{
}

BaseDataTableModel* BaseDataTableView::model() const
{
    return m_model;
}

void BaseDataTableView::setModel(BaseDataTableModel* model)
{
    m_model = model;
    refresh();
}

void BaseDataTableView::refresh()
{
    rebuildTree();
}

void BaseDataTableView::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    if (!item || column != 0 || m_updatingChecks) {
        return;
    }

    m_updatingChecks = true;
    {
        const QSignalBlocker blocker(m_treeWidget);

        if (isLayerGroupNode(item)) {
            const Qt::CheckState state = item->checkState(0);
            if (state != Qt::PartiallyChecked) {
                QVector<QPair<QTreeWidgetItem*, Qt::CheckState>> prevStates;
                prevStates.reserve(item->childCount());
                for (int i = 0; i < item->childCount(); ++i) {
                    QTreeWidgetItem* child = item->child(i);
                    if (!child || !isLayerNode(child)) {
                        continue;
                    }
                    prevStates.append(qMakePair(child, child->checkState(0)));
                }

                propagateCheckStateToChildren(item, state);

                const bool checked = (state == Qt::Checked);
                for (const QPair<QTreeWidgetItem*, Qt::CheckState>& entry : prevStates) {
                    QTreeWidgetItem* child = entry.first;
                    if (entry.second == state) {
                        continue;
                    }
                    const QVariantMap userData = itemUserData(child);
                    const QString layerName =
                        userData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
                    syncLayerGroupItemChecked(layerTypenameFromItem(child), checked);
                    emit layerStateChanged(layerName, checked);
                    emit layerStateChangedWithData(layerName, checked, userData);
                }
            }
        } else if (isLayerNode(item)) {
            updateParentGroupCheckState(item);
            const QVariantMap userData = itemUserData(item);
            const QString layerName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
            const bool checked = item->checkState(0) == Qt::Checked;
            syncLayerGroupItemChecked(layerTypenameFromItem(item), checked);
            emit layerStateChanged(layerName, checked);
            emit layerStateChangedWithData(layerName, checked, userData);
        }
    }
    m_updatingChecks = false;
}

void BaseDataTableView::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (!item || !m_treeWidget || !isLayerGroupNode(item) || item->childCount() <= 0) {
        return;
    }

    const QPoint pos = m_treeWidget->viewport()->mapFromGlobal(QCursor::pos());
    if (isTreeWidgetItemCheckBoxHit(item, pos)) {
        return;
    }

    const QRect itemRect = m_treeWidget->visualItemRect(item);
    const int branchWidth = m_treeWidget->indentation() + 12;
    const QRect branchRect(itemRect.left(), itemRect.top(), branchWidth, itemRect.height());
    if (branchRect.contains(pos)) {
        return;
    }

    toggleLayerGroupExpanded(item);
}

bool BaseDataTableView::isTreeWidgetItemCheckBoxHit(const QTreeWidgetItem* item, const QPoint& viewportPos) const
{
    if (!m_treeWidget || !item || !item->flags().testFlag(Qt::ItemIsUserCheckable)) {
        return false;
    }

    const QRect itemRect = m_treeWidget->visualItemRect(item);
    if (!itemRect.isValid()) {
        return false;
    }

    QStyleOptionViewItem option;
    option.rect = itemRect;
    option.state = QStyle::State_Enabled | QStyle::State_Active;
    if (m_treeWidget->hasFocus()) {
        option.state |= QStyle::State_HasFocus;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QStyle::SubElement checkSubElement = QStyle::SE_ItemViewItemCheckBox;
#else
    const QStyle::SubElement checkSubElement = QStyle::SE_ItemViewItemCheckIndicator;
#endif
    const QRect checkRect = m_treeWidget->style()->subElementRect(checkSubElement, &option, m_treeWidget);
    return checkRect.isValid() && checkRect.contains(viewportPos);
}

void BaseDataTableView::toggleLayerGroupExpanded(QTreeWidgetItem* item)
{
    if (!m_treeWidget || !item || !isLayerGroupNode(item) || item->childCount() <= 0) {
        return;
    }

    item->setExpanded(!item->isExpanded());
}

void BaseDataTableView::onTreeItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    if (!item || column != 0) {
        return;
    }

    if (isLayerNode(item)) {
        const QPoint pos = m_treeWidget ? m_treeWidget->viewport()->mapFromGlobal(QCursor::pos()) : QPoint();
        if (m_treeWidget && isTreeWidgetItemCheckBoxHit(item, pos)) {
            return;
        }

        const QVariantMap userData = itemUserData(item);
        const QString typeName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
        if (typeName.isEmpty()) {
            return;
        }

        emit baseDataLayerDataViewRequested(typeName, userData);
        return;
    }

    if (!isToolNode(item)) {
        return;
    }

    const QVariantMap userData = itemUserData(item);
    const QString toolName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
    emit toolClicked(toolName);
    emit toolNodeClicked(toolName, userData);
}

void BaseDataTableView::onTreeCustomContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = m_treeWidget ? m_treeWidget->itemAt(pos) : nullptr;
    if (!item || !isLayerNode(item)) {
        return;
    }

    m_contextLayerItem = item;

    QMenu menu(this);
    menu.setStyleSheet(QString::fromUtf8(kBaseDataTreeMenuQss));

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    QAction* zoomToLayerAction = menu.addAction(QString::fromUtf8(u8"缩放至图层"));
    connect(zoomToLayerAction, &QAction::triggered, this, &BaseDataTableView::onBaseDataLayerZoomToTriggered);
#endif

    QAction* importAction = menu.addAction(QString::fromUtf8(u8"导入"));
    connect(importAction, &QAction::triggered, this, &BaseDataTableView::onBaseDataLayerImportTriggered);

    QMenu* exportMenu = menu.addMenu(QString::fromUtf8(u8"导出"));
    QAction* exportExcelAction = exportMenu->addAction(QString::fromUtf8(u8"导出 Excel"));
    exportExcelAction->setProperty("exportType", QStringLiteral("xlsx"));
    connect(exportExcelAction, &QAction::triggered, this, &BaseDataTableView::onBaseDataLayerExportTriggered);

    QAction* exportCsvAction = exportMenu->addAction(QString::fromUtf8(u8"导出 CSV"));
    exportCsvAction->setProperty("exportType", QStringLiteral("csv"));
    connect(exportCsvAction, &QAction::triggered, this, &BaseDataTableView::onBaseDataLayerExportTriggered);

    menu.exec(m_treeWidget->mapToGlobal(pos));
    m_contextLayerItem = nullptr;
}

void BaseDataTableView::onBaseDataLayerImportTriggered()
{
    QTreeWidgetItem* item = m_contextLayerItem;
    if (!item || !isLayerNode(item)) {
        return;
    }

    const QVariantMap userData = itemUserData(item);
    const QString val = userData.value(baseDataUserDataKey(BaseDataUserDataField::Val)).toString();
    const QString table = CaseDrcsvHelper::tableNameFromLayerVal(val);
    const QString displayName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();

    if (table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"无法识别该图层对应的数据表（table），请确认接口菜单中图层 val 已配置（如 road_way、bus_point、other_od）。"));
        return;
    }

    emit baseDataLayerCsvImportRequested(table, displayName);
}

void BaseDataTableView::onBaseDataLayerExportTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }

    const QString exportType = action->property("exportType").toString();
    QTreeWidgetItem* item = m_contextLayerItem;
    if (exportType.isEmpty() || !item || !isLayerNode(item)) {
        return;
    }

    const QVariantMap userData = itemUserData(item);
    const QString val = userData.value(baseDataUserDataKey(BaseDataUserDataField::Val)).toString();
    const QString table = CaseDrcsvHelper::tableNameFromLayerVal(val);
    const QString displayName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();

    if (table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"无法识别该图层对应的数据表（table），请确认接口菜单中图层 val 已配置（如 road_way、bus_point、other_od）。"));
        return;
    }

    emit baseDataLayerExportRequested(table, exportType, displayName);
}

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
void BaseDataTableView::onBaseDataLayerZoomToTriggered()
{
    QTreeWidgetItem* item = m_contextLayerItem;
    if (!item || !isLayerNode(item)) {
        return;
    }

    const QVariantMap userData = itemUserData(item);
    const QString typeName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
    const QString displayName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();

    emit baseDataLayerZoomToRequested(typeName, displayName);
}
#endif

void BaseDataTableView::initUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "BaseDataTableView {"
        "   background-color: transparent;"
        "}"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 16, 12, 16);
    mainLayout->setSpacing(0);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setIndentation(18);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setExpandsOnDoubleClick(false);
    m_treeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_treeWidget->setStyleSheet(
        "QTreeWidget {"
        "   border: none;"
        "   background-color: #050f1f;"
        "   color: rgba(255,255,255,0.92);"
        "   font-size: 13px;"
        "}"
        "QTreeWidget::item {"
        "   min-height: 30px;"
        "   padding: 2px 0;"
        "}"
        "QTreeWidget::item:selected {"
        "   background-color: rgba(42,163,255,0.20);"
        "}"
        + Phase2Ui::darkSidebarTreeBranchStyleRules(QStringLiteral("QTreeWidget"))
    );
    m_treeWidget->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "   width: 10px;"
        "   margin: 0;"
        "   background: transparent;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(63, 135, 236, 0.35);"
        "   border-radius: 5px;"
        "   min-height: 28px;"
        "}");
    m_treeWidget->header()->setSectionResizeMode(QHeaderView::Stretch);
    connect(m_treeWidget, &QTreeWidget::itemChanged, this, &BaseDataTableView::onTreeItemChanged);
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &BaseDataTableView::onTreeItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &BaseDataTableView::onTreeItemDoubleClicked);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this,
        &BaseDataTableView::onTreeCustomContextMenuRequested);
    mainLayout->addWidget(m_treeWidget, 1);
}

void BaseDataTableView::rebuildTree()
{
    if (!m_treeWidget) {
        return;
    }

    const QSignalBlocker blocker(m_treeWidget);
    m_treeWidget->clear();
    m_layersRootItem = new QTreeWidgetItem(m_treeWidget, QStringList {QString::fromUtf8(u8"所有图层")});
    m_toolsRootItem = new QTreeWidgetItem(m_treeWidget, QStringList {QString::fromUtf8(u8"工具")});
    m_layersRootItem->setFlags(Qt::ItemIsEnabled);
    m_toolsRootItem->setFlags(Qt::ItemIsEnabled);
    m_layersRootItem->setExpanded(true);
    m_toolsRootItem->setExpanded(true);

    for (const BaseDataLayerGroupNodeItem& groupItem : m_layerGroupItems) {
        QTreeWidgetItem* groupNode =
            new QTreeWidgetItem(m_layersRootItem, QStringList {groupItem.displayName});
        groupNode->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        QVariantMap groupExtra;
        groupExtra.insert(baseDataUserDataKey(BaseDataUserDataField::Value), groupItem.groupValue);
        groupNode->setData(0,
                           kUserDataRole,
                           buildNodeUserData(BaseDataNodeCategory::LayerGroup, groupItem.displayName, groupExtra));

        if (!groupItem.layers.isEmpty()) {
            groupNode->setFlags(groupNode->flags() | Qt::ItemIsUserCheckable);
            groupNode->setCheckState(0, Qt::Unchecked);

            for (const BaseDataLayerNodeItem& layerItem : groupItem.layers) {
                QTreeWidgetItem* child = new QTreeWidgetItem(groupNode, QStringList {layerItem.displayName});
                child->setFlags(child->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                child->setCheckState(0, layerItem.checked ? Qt::Checked : Qt::Unchecked);
                child->setData(0,
                               kUserDataRole,
                               buildNodeUserData(BaseDataNodeCategory::Layer, layerItem.displayName, layerItem.userData));
            }
        }

    }

    for (const BaseDataToolNodeItem& toolItem : m_toolItems) {
        QTreeWidgetItem* child = new QTreeWidgetItem(m_toolsRootItem, QStringList {toolItem.displayName});
        child->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        child->setData(0, kUserDataRole,
                       buildNodeUserData(BaseDataNodeCategory::Tool, toolItem.displayName, toolItem.userData));
    }

    m_treeWidget->expandItem(m_layersRootItem);
    m_treeWidget->expandItem(m_toolsRootItem);
}

QVariantMap BaseDataTableView::itemUserData(const QTreeWidgetItem* item)
{
    if (!item) {
        return {};
    }
    return item->data(0, kUserDataRole).toMap();
}

bool BaseDataTableView::isLayerNode(const QTreeWidgetItem* item)
{
    const QVariantMap userData = itemUserData(item);
    return userData.value(baseDataUserDataKey(BaseDataUserDataField::NodeCategory)).toInt()
           == static_cast<int>(BaseDataNodeCategory::Layer);
}

bool BaseDataTableView::isLayerGroupNode(const QTreeWidgetItem* item)
{
    const QVariantMap userData = itemUserData(item);
    return userData.value(baseDataUserDataKey(BaseDataUserDataField::NodeCategory)).toInt()
           == static_cast<int>(BaseDataNodeCategory::LayerGroup);
}

bool BaseDataTableView::isToolNode(const QTreeWidgetItem* item)
{
    const QVariantMap userData = itemUserData(item);
    return userData.value(baseDataUserDataKey(BaseDataUserDataField::NodeCategory)).toInt()
           == static_cast<int>(BaseDataNodeCategory::Tool);
}

QVariantMap BaseDataTableView::buildNodeUserData(BaseDataNodeCategory category,
                                                 const QString& name,
                                                 const QVariantMap& extraData)
{
    QVariantMap userData = extraData;
    userData.insert(baseDataUserDataKey(BaseDataUserDataField::NodeCategory), static_cast<int>(category));
    userData.insert(baseDataUserDataKey(BaseDataUserDataField::Name), name);
    return userData;
}

void BaseDataTableView::propagateCheckStateToChildren(QTreeWidgetItem* item, Qt::CheckState state)
{
    if (!item) {
        return;
    }
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem* child = item->child(i);
        if (!child) {
            continue;
        }
        if (isLayerNode(child)) {
            child->setCheckState(0, state);
        } else if (isLayerGroupNode(child)) {
            propagateCheckStateToChildren(child, state);
            child->setCheckState(0, state);
        }
    }
}

Qt::CheckState BaseDataTableView::aggregateChildCheckState(const QTreeWidgetItem* parent) const
{
    if (!parent || parent->childCount() == 0) {
        return Qt::Unchecked;
    }

    int checkedCount = 0;
    int uncheckedCount = 0;
    for (int i = 0; i < parent->childCount(); ++i) {
        const QTreeWidgetItem* child = parent->child(i);
        if (!child || !isLayerNode(child)) {
            continue;
        }
        const Qt::CheckState cs = child->checkState(0);
        if (cs == Qt::PartiallyChecked) {
            return Qt::PartiallyChecked;
        }
        if (cs == Qt::Checked) {
            ++checkedCount;
        } else {
            ++uncheckedCount;
        }
    }

    const int total = checkedCount + uncheckedCount;
    if (total == 0) {
        return Qt::Unchecked;
    }
    if (checkedCount == total) {
        return Qt::Checked;
    }
    if (uncheckedCount == total) {
        return Qt::Unchecked;
    }
    return Qt::PartiallyChecked;
}

void BaseDataTableView::updateParentGroupCheckState(QTreeWidgetItem* item)
{
    if (!item) {
        return;
    }
    QTreeWidgetItem* parent = item->parent();
    while (parent && parent != m_layersRootItem) {
        if (isLayerGroupNode(parent)) {
            parent->setCheckState(0, aggregateChildCheckState(parent));
        }
        parent = parent->parent();
    }
}

void BaseDataTableView::setLayerGroupItems(const QList<BaseDataLayerGroupNodeItem>& items)
{
    m_layerGroupItems = items;
    refresh();
}

void BaseDataTableView::setToolItems(const QList<BaseDataToolNodeItem>& items)
{
    m_toolItems = items;
    refresh();
}

void BaseDataTableView::setTreeData(const QList<BaseDataLayerGroupNodeItem>& layerGroupItems,
                                    const QList<BaseDataToolNodeItem>& toolItems)
{
    m_layerGroupItems = layerGroupItems;
    m_toolItems = toolItems;
    refresh();
}

void BaseDataTableView::clearTreeData()
{
    m_layerGroupItems.clear();
    m_toolItems.clear();
    refresh();
}

bool BaseDataTableView::findLayerUserDataByVal(const QString& val, QVariantMap* outUserData) const
{
    if (!m_treeWidget || !m_layersRootItem || val.trimmed().isEmpty()) {
        return false;
    }

    const QString targetVal = val.trimmed().toLower();
    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QVariantMap userData = itemUserData(child);
            const QString layerVal =
                userData.value(baseDataUserDataKey(BaseDataUserDataField::Val)).toString().trimmed().toLower();
            if (layerVal == targetVal) {
                if (outUserData) {
                    *outUserData = userData;
                }
                return true;
            }
        }
    }

    return false;
}

bool BaseDataTableView::setLayerCheckedByVal(const QString& val, bool checked, QVariantMap* matchedUserData)
{
    if (!m_treeWidget || !m_layersRootItem || val.trimmed().isEmpty()) {
        return false;
    }

    const QString targetVal = val.trimmed().toLower();
    QTreeWidgetItem* targetItem = nullptr;
    QVariantMap targetUserData;

    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QVariantMap userData = itemUserData(child);
            const QString layerVal =
                userData.value(baseDataUserDataKey(BaseDataUserDataField::Val)).toString().trimmed().toLower();
            if (layerVal == targetVal) {
                targetItem = child;
                targetUserData = userData;
                break;
            }
        }
        if (targetItem) {
            break;
        }
    }

    if (!targetItem) {
        return false;
    }

    const Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    if (targetItem->checkState(0) == state) {
        if (checked) {
            const QString layerName =
                targetUserData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
            emit layerStateChanged(layerName, true);
            emit layerStateChangedWithData(layerName, true, targetUserData);
        }
        if (matchedUserData) {
            *matchedUserData = targetUserData;
        }
        return true;
    }

    m_updatingChecks = true;
    {
        const QSignalBlocker blocker(m_treeWidget);
        targetItem->setCheckState(0, state);
        updateParentGroupCheckState(targetItem);
    }
    m_updatingChecks = false;

    const QString layerName = targetUserData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
    emit layerStateChanged(layerName, checked);
    emit layerStateChangedWithData(layerName, checked, targetUserData);

    if (matchedUserData) {
        *matchedUserData = targetUserData;
    }
    return true;
}

bool BaseDataTableView::findLayerUserDataByValAndGroup(const QString& val,
                                                       const QString& groupValue,
                                                       QVariantMap* outUserData) const
{
    if (!m_treeWidget || !m_layersRootItem || val.trimmed().isEmpty() || groupValue.trimmed().isEmpty()) {
        return false;
    }

    const QString targetVal = val.trimmed().toLower();
    const QString targetGroup = groupValue.trimmed().toLower();
    QTreeWidgetItem* groupItem = findGroupItemByValue(groupValue);
    if (!groupItem) {
        return false;
    }
    if (groupValueFromItem(groupItem).trimmed().toLower() != targetGroup) {
        return false;
    }

    for (int i = 0; i < groupItem->childCount(); ++i) {
        QTreeWidgetItem* child = groupItem->child(i);
        if (!child || !isLayerNode(child)) {
            continue;
        }
        const QVariantMap userData = itemUserData(child);
        const QString layerVal =
            userData.value(baseDataUserDataKey(BaseDataUserDataField::Val)).toString().trimmed().toLower();
        if (layerVal != targetVal) {
            continue;
        }
        if (outUserData) {
            *outUserData = userData;
        }
        return true;
    }
    return false;
}

bool BaseDataTableView::setLayerCheckedByTypename(const QString& wfsTypeName, bool checked, QVariantMap* matchedUserData)
{
    if (!m_treeWidget || !m_layersRootItem) {
        return false;
    }

    const QString target = wfsTypeName.trimmed();
    if (target.isEmpty()) {
        return false;
    }

    QTreeWidgetItem* targetItem = nullptr;
    QVariantMap targetUserData;
    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QVariantMap userData = itemUserData(child);
            const QString value =
                userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (value.compare(target, Qt::CaseInsensitive) == 0) {
                targetItem = child;
                targetUserData = userData;
                break;
            }
        }
        if (targetItem) {
            break;
        }
    }

    if (!targetItem) {
        return false;
    }

    const Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    if (targetItem->checkState(0) == state) {
        if (checked) {
            const QString layerName =
                targetUserData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
            emit layerStateChanged(layerName, true);
            emit layerStateChangedWithData(layerName, true, targetUserData);
        }
        if (matchedUserData) {
            *matchedUserData = targetUserData;
        }
        return true;
    }

    m_updatingChecks = true;
    {
        const QSignalBlocker blocker(m_treeWidget);
        targetItem->setCheckState(0, state);
        updateParentGroupCheckState(targetItem);
    }
    m_updatingChecks = false;

    const QString layerName = targetUserData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
    emit layerStateChanged(layerName, checked);
    emit layerStateChangedWithData(layerName, checked, targetUserData);

    if (matchedUserData) {
        *matchedUserData = targetUserData;
    }
    return true;
}

bool BaseDataTableView::setLayerCheckedByTypenameQuiet(const QString& wfsTypeName, bool checked)
{
    if (!m_treeWidget || !m_layersRootItem) {
        return false;
    }

    const QString target = wfsTypeName.trimmed();
    if (target.isEmpty()) {
        return false;
    }

    QList<QTreeWidgetItem*> matchedItems;
    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QVariantMap userData = itemUserData(child);
            const QString value =
                userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (value.compare(target, Qt::CaseInsensitive) == 0) {
                matchedItems.append(child);
            }
        }
    }

    if (matchedItems.isEmpty()) {
        return false;
    }

    const Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    bool anyChanged = false;
    for (QTreeWidgetItem* targetItem : matchedItems) {
        if (targetItem->checkState(0) != state) {
            anyChanged = true;
            break;
        }
    }
    if (!anyChanged) {
        syncLayerGroupItemChecked(target, checked);
        return true;
    }

    m_updatingChecks = true;
    {
        const QSignalBlocker blocker(m_treeWidget);
        for (QTreeWidgetItem* targetItem : matchedItems) {
            if (targetItem->checkState(0) == state) {
                continue;
            }
            targetItem->setCheckState(0, state);
            updateParentGroupCheckState(targetItem);
        }
    }
    m_updatingChecks = false;
    syncLayerGroupItemChecked(target, checked);
    return true;
}

void BaseDataTableView::reconcileCheckedLayersWithMap(TNMapWidget* map)
{
    if (!map || !m_layersRootItem) {
        return;
    }

    QSet<QString> typenamesToUncheck;
    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem || !isLayerGroupNode(groupItem)) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child) || child->checkState(0) != Qt::Checked) {
                continue;
            }
            const QString tn = layerTypenameFromItem(child);
            if (tn.isEmpty() || typenamesToUncheck.contains(tn)) {
                continue;
            }
            const QString uri = CaseWfsMap::buildWfsDataSourceUri(tn);
            if (!map->getLayer(uri)) {
                typenamesToUncheck.insert(tn);
            }
        }
    }

    for (const QString& tn : typenamesToUncheck) {
        setLayerCheckedByTypenameQuiet(tn, false);
    }
}

QVariantMap BaseDataTableView::findCheckedLayerUserDataByWfsTypeName(const QString& wfsTypeName) const
{
    if (!m_treeWidget || !m_layersRootItem) {
        return {};
    }

    const QString target = wfsTypeName.trimmed();
    if (target.isEmpty()) {
        return {};
    }

    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            if (child->checkState(0) != Qt::Checked) {
                continue;
            }
            const QVariantMap userData = itemUserData(child);
            const QString value =
                userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (value.compare(target, Qt::CaseInsensitive) == 0) {
                return userData;
            }
        }
    }
    return {};
}

QString BaseDataTableView::layerTypenameFromItem(const QTreeWidgetItem* item)
{
    if (!item) {
        return {};
    }
    return itemUserData(item).value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
}

void BaseDataTableView::syncLayerGroupItemChecked(const QString& wfsTypeName, bool checked)
{
    const QString target = wfsTypeName.trimmed();
    if (target.isEmpty()) {
        return;
    }

    for (BaseDataLayerGroupNodeItem& group : m_layerGroupItems) {
        for (BaseDataLayerNodeItem& layer : group.layers) {
            const QString tn =
                layer.userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (tn.compare(target, Qt::CaseInsensitive) == 0) {
                layer.checked = checked;
                return;
            }
        }
    }
}

QString BaseDataTableView::groupValueFromItem(const QTreeWidgetItem* item)
{
    if (!item) {
        return {};
    }
    return itemUserData(item).value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
}

bool BaseDataTableView::ensureLayersTreeRoots()
{
    if (!m_treeWidget) {
        return false;
    }

    const QString layersRootText = QString::fromUtf8(u8"所有图层");
    const QString toolsRootText = QString::fromUtf8(u8"工具");

    if (!m_layersRootItem) {
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_treeWidget->topLevelItem(i);
            if (item && item->text(0) == layersRootText) {
                m_layersRootItem = item;
                break;
            }
        }
    }
    if (!m_layersRootItem) {
        m_layersRootItem = new QTreeWidgetItem(m_treeWidget, QStringList {layersRootText});
        m_layersRootItem->setFlags(Qt::ItemIsEnabled);
        m_layersRootItem->setExpanded(true);
    }

    if (!m_toolsRootItem) {
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_treeWidget->topLevelItem(i);
            if (item && item->text(0) == toolsRootText) {
                m_toolsRootItem = item;
                break;
            }
        }
    }
    if (!m_toolsRootItem) {
        m_toolsRootItem = new QTreeWidgetItem(m_treeWidget, QStringList {toolsRootText});
        m_toolsRootItem->setFlags(Qt::ItemIsEnabled);
        m_toolsRootItem->setExpanded(true);
    }

    return m_layersRootItem != nullptr;
}

QTreeWidgetItem* BaseDataTableView::findGroupItemByValue(const QString& groupValue) const
{
    if (!m_layersRootItem) {
        return nullptr;
    }

    const QString target = groupValue.trimmed();
    if (target.isEmpty()) {
        return nullptr;
    }

    for (int i = 0; i < m_layersRootItem->childCount(); ++i) {
        QTreeWidgetItem* child = m_layersRootItem->child(i);
        if (!child || !isLayerGroupNode(child)) {
            continue;
        }
        if (groupValueFromItem(child) == target) {
            return child;
        }
    }
    return nullptr;
}

QTreeWidgetItem* BaseDataTableView::findLayerItemByTypename(const QString& typenameKey,
                                                            QTreeWidgetItem** outParentGroup) const
{
    if (!m_layersRootItem) {
        return nullptr;
    }

    const QString target = typenameKey.trimmed();
    if (target.isEmpty()) {
        return nullptr;
    }

    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem || !isLayerGroupNode(groupItem)) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            if (layerTypenameFromItem(child).compare(target, Qt::CaseInsensitive) == 0) {
                if (outParentGroup) {
                    *outParentGroup = groupItem;
                }
                return child;
            }
        }
    }
    return nullptr;
}

void BaseDataTableView::applyLayerCheckState(QTreeWidgetItem* layerItem, const BaseDataLayerTreeSnapshot& snapshot)
{
    if (!layerItem || !isLayerNode(layerItem)) {
        return;
    }

    const QString tn = layerTypenameFromItem(layerItem);
    Qt::CheckState state = Qt::Unchecked;
    if (!tn.isEmpty() && snapshot.checkByTypename.contains(tn)) {
        state = snapshot.checkByTypename.value(tn);
    }

    if (layerItem->checkState(0) == state) {
        return;
    }

    layerItem->setCheckState(0, state);
}

void BaseDataTableView::removeLayerItemAt(QTreeWidgetItem* parentGroup, int row, bool notifyUnchecked)
{
    if (!parentGroup || row < 0 || row >= parentGroup->childCount()) {
        return;
    }

    QTreeWidgetItem* item = parentGroup->child(row);
    if (!item || !isLayerNode(item)) {
        return;
    }

    if (notifyUnchecked && item->checkState(0) == Qt::Checked) {
        const QVariantMap userData = itemUserData(item);
        const QString layerName = userData.value(baseDataUserDataKey(BaseDataUserDataField::Name)).toString();
        emit layerStateChanged(layerName, false);
        emit layerStateChangedWithData(layerName, false, userData);
    }

    delete parentGroup->takeChild(row);
}

QTreeWidgetItem* BaseDataTableView::createGroupItem(const BaseDataLayerGroupNodeItem& group, int insertIndex)
{
    if (!m_layersRootItem) {
        return nullptr;
    }

    QTreeWidgetItem* groupNode = new QTreeWidgetItem(QStringList {group.displayName});
    groupNode->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    QVariantMap groupExtra;
    groupExtra.insert(baseDataUserDataKey(BaseDataUserDataField::Value), group.groupValue);
    groupNode->setData(0,
                       kUserDataRole,
                       buildNodeUserData(BaseDataNodeCategory::LayerGroup, group.displayName, groupExtra));

    if (!group.layers.isEmpty()) {
        groupNode->setFlags(groupNode->flags() | Qt::ItemIsUserCheckable);
        groupNode->setCheckState(0, Qt::Unchecked);
    }

    const int index = qBound(0, insertIndex, m_layersRootItem->childCount());
    m_layersRootItem->insertChild(index, groupNode);
    return groupNode;
}

QTreeWidgetItem* BaseDataTableView::createLayerItem(QTreeWidgetItem* parentGroup,
                                                    const BaseDataLayerNodeItem& layer,
                                                    int insertIndex)
{
    if (!parentGroup) {
        return nullptr;
    }

    QTreeWidgetItem* child = new QTreeWidgetItem(QStringList {layer.displayName});
    child->setFlags(child->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    child->setCheckState(0, Qt::Unchecked);
    child->setData(0,
                   kUserDataRole,
                   buildNodeUserData(BaseDataNodeCategory::Layer, layer.displayName, layer.userData));

    if (!parentGroup->flags().testFlag(Qt::ItemIsUserCheckable)) {
        parentGroup->setFlags(parentGroup->flags() | Qt::ItemIsUserCheckable);
        parentGroup->setCheckState(0, Qt::Unchecked);
    }

    const int index = qBound(0, insertIndex, parentGroup->childCount());
    parentGroup->insertChild(index, child);
    return child;
}

BaseDataLayerTreeSnapshot BaseDataTableView::captureLayerCheckStates() const
{
    BaseDataLayerTreeSnapshot snapshot;
    if (!m_layersRootItem) {
        return snapshot;
    }

    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem || !isLayerGroupNode(groupItem)) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QString tn = layerTypenameFromItem(child);
            if (!tn.isEmpty()) {
                snapshot.checkByTypename.insert(tn, child->checkState(0));
            }
        }
    }
    return snapshot;
}

bool BaseDataTableView::syncLayerGroups(const QList<BaseDataLayerGroupNodeItem>& newGroups,
                                        const BaseDataLayerTreeSnapshot& snapshot,
                                        bool* layerListChanged)
{
    if (!ensureLayersTreeRoots()) {
        return false;
    }

    QSet<QString> currentTypenames;
    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem || !isLayerGroupNode(groupItem)) {
            continue;
        }
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QString tn = layerTypenameFromItem(child);
            if (!tn.isEmpty()) {
                currentTypenames.insert(tn);
            }
        }
    }

    QSet<QString> desiredTypenames;
    QSet<QString> desiredGroupValues;
    for (const BaseDataLayerGroupNodeItem& group : newGroups) {
        desiredGroupValues.insert(group.groupValue.trimmed());
        for (const BaseDataLayerNodeItem& layer : group.layers) {
            const QString tn = layer.userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (!tn.isEmpty()) {
                desiredTypenames.insert(tn);
            }
        }
    }

    if (layerListChanged) {
        *layerListChanged = currentTypenames != desiredTypenames;
    }

    const QSignalBlocker blocker(m_treeWidget);
    m_updatingChecks = true;

    QList<QPair<QTreeWidgetItem*, int>> orphanLayers;
    for (int g = 0; g < m_layersRootItem->childCount(); ++g) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(g);
        if (!groupItem || !isLayerGroupNode(groupItem)) {
            continue;
        }
        for (int i = groupItem->childCount() - 1; i >= 0; --i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QString tn = layerTypenameFromItem(child);
            if (!desiredTypenames.contains(tn)) {
                orphanLayers.append(qMakePair(groupItem, i));
            }
        }
    }
    for (const auto& pair : orphanLayers) {
        removeLayerItemAt(pair.first, pair.second, true);
    }

    for (int i = m_layersRootItem->childCount() - 1; i >= 0; --i) {
        QTreeWidgetItem* groupItem = m_layersRootItem->child(i);
        if (!groupItem || !isLayerGroupNode(groupItem)) {
            continue;
        }
        const QString gv = groupValueFromItem(groupItem);
        if (!desiredGroupValues.contains(gv)) {
            for (int c = groupItem->childCount() - 1; c >= 0; --c) {
                removeLayerItemAt(groupItem, c, true);
            }
            delete m_layersRootItem->takeChild(i);
        }
    }

    for (int gi = 0; gi < newGroups.size(); ++gi) {
        const BaseDataLayerGroupNodeItem& group = newGroups.at(gi);
        QTreeWidgetItem* groupItem = findGroupItemByValue(group.groupValue);
        if (!groupItem) {
            groupItem = createGroupItem(group, gi);
        } else {
            groupItem->setText(0, group.displayName);
            QVariantMap groupExtra;
            groupExtra.insert(baseDataUserDataKey(BaseDataUserDataField::Value), group.groupValue);
            groupItem->setData(0,
                               kUserDataRole,
                               buildNodeUserData(BaseDataNodeCategory::LayerGroup, group.displayName, groupExtra));
            if (m_layersRootItem->indexOfChild(groupItem) != gi) {
                QTreeWidgetItem* taken = m_layersRootItem->takeChild(m_layersRootItem->indexOfChild(groupItem));
                m_layersRootItem->insertChild(gi, taken);
                groupItem = taken;
            }
        }

        if (!groupItem) {
            continue;
        }

        QSet<QString> groupDesiredTypenames;
        for (const BaseDataLayerNodeItem& layer : group.layers) {
            const QString tn = layer.userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (!tn.isEmpty()) {
                groupDesiredTypenames.insert(tn);
            }
        }

        for (int li = 0; li < group.layers.size(); ++li) {
            const BaseDataLayerNodeItem& layer = group.layers.at(li);
            const QString tn =
                layer.userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (tn.isEmpty()) {
                continue;
            }

            QTreeWidgetItem* parentGroup = nullptr;
            QTreeWidgetItem* layerItem = findLayerItemByTypename(tn, &parentGroup);
            if (layerItem && parentGroup != groupItem) {
                const int oldRow = parentGroup->indexOfChild(layerItem);
                layerItem = parentGroup->takeChild(oldRow);
                const int insertRow = qMin(li, groupItem->childCount());
                groupItem->insertChild(insertRow, layerItem);
            }

            if (!layerItem) {
                layerItem = createLayerItem(groupItem, layer, li);
            } else {
                layerItem->setText(0, layer.displayName);
                layerItem->setData(0,
                                   kUserDataRole,
                                   buildNodeUserData(BaseDataNodeCategory::Layer, layer.displayName, layer.userData));
                if (layerItem->parent() == groupItem && groupItem->indexOfChild(layerItem) != li) {
                    QTreeWidgetItem* taken = groupItem->takeChild(groupItem->indexOfChild(layerItem));
                    groupItem->insertChild(li, taken);
                    layerItem = taken;
                }
            }

            applyLayerCheckState(layerItem, snapshot);
        }

        for (int i = groupItem->childCount() - 1; i >= 0; --i) {
            QTreeWidgetItem* child = groupItem->child(i);
            if (!child || !isLayerNode(child)) {
                continue;
            }
            const QString tn = layerTypenameFromItem(child);
            if (!groupDesiredTypenames.contains(tn)) {
                removeLayerItemAt(groupItem, i, true);
            }
        }

        updateParentGroupCheckState(groupItem);
    }

    m_layerGroupItems = newGroups;
    for (BaseDataLayerGroupNodeItem& group : m_layerGroupItems) {
        for (BaseDataLayerNodeItem& layer : group.layers) {
            const QString tn = layer.userData.value(baseDataUserDataKey(BaseDataUserDataField::Value)).toString().trimmed();
            if (snapshot.checkByTypename.contains(tn)) {
                layer.checked = snapshot.checkByTypename.value(tn) == Qt::Checked;
            }
        }
    }

    m_updatingChecks = false;
    return true;
}
