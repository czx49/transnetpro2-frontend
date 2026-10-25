/*****************************************************************//**
 * \file   casetreeview.cpp
 * \brief  方案树视图实现
 * \details 显示方案树结构，支持展开/收起、复选框、三点菜单与右键菜单
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "casetreeview.h"

#include "casedrcsvhelper.h"
#include "../publicctrls/messagedialog.h"
#include "../publicctrls/phase2treeutil.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSize>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>

namespace {

static const char kCaseTreeMenuQss[] =
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

class CaseTreeItemDelegate : public QStyledItemDelegate {
public:
    explicit CaseTreeItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!index.parent().isValid()) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);

            const bool isSelected = (opt.state & QStyle::State_Selected);
            const QColor bgColor = isSelected ? QColor(31, 77, 145) : QColor(15, 31, 59);
            const QRect bgRect = opt.rect.adjusted(2, 3, -2, -3);
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawRoundedRect(bgRect, 5, 5);

            painter->setPen(QColor(235, 245, 255));
            QFont font = opt.font;
            font.setBold(true);
            font.setPointSize(10);
            painter->setFont(font);
            const QRect textRect = opt.rect.adjusted(26, 0, -30, 0);
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, index.data().toString());

            painter->setPen(QColor(214, 228, 245));
            QFont dotsFont = painter->font();
            dotsFont.setPointSize(11);
            dotsFont.setBold(true);
            painter->setFont(dotsFont);
            const QRect dotsRect(opt.rect.right() - 26, opt.rect.top(), 20, opt.rect.height());
            painter->drawText(dotsRect, Qt::AlignCenter, QStringLiteral("..."));
            painter->restore();
            return;
        }

        QStyledItemDelegate::paint(painter, option, index);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(index.parent().isValid() ? 34 : 40);
        return size;
    }
};
} // namespace

CaseTreeView::CaseTreeView(QWidget* parent)
    : QTreeView(parent)
    , m_caseModel(nullptr)
{
    initUi();
}

CaseTreeView::~CaseTreeView()
{
}

CaseTreeModel* CaseTreeView::caseModel() const
{
    return m_caseModel;
}

void CaseTreeView::setCaseModel(CaseTreeModel* model)
{
    if (m_caseModel) {
        disconnect(m_caseModel, nullptr, this, nullptr);
    }

    m_caseModel = model;
    setModel(model);
    setItemDelegate(new CaseTreeItemDelegate(this));

    if (m_caseModel) {
        connect(m_caseModel, &QAbstractItemModel::modelReset, this, &CaseTreeView::onModelReset);
    }

    setColumnWidth(0, 198);
    setIndentation(16);

    setStyleSheet(
        "QTreeView {"
        "   background-color: #050f1f;"
        "   border: none;"
        "   color: rgba(255,255,255,0.92);"
        "   outline: none;"
        "}"
        "QTreeView::item {"
        "   min-height: 30px;"
        "   padding: 3px 6px;"
        "   border: none;"
        "}"
        "QTreeView::item:hover:!selected {"
        "   background-color: rgba(34, 137, 255, 0.10);"
        "}"
        "QTreeView::item:selected {"
        "   background-color: rgba(34, 137, 255, 0.18);"
        "}"
        + Phase2Ui::darkSidebarTreeBranchStyleRules(QStringLiteral("QTreeView"))
    );

    verticalScrollBar()->setStyleSheet(
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
}

void CaseTreeView::onModelReset()
{
    collapseAll();
}

bool CaseTreeView::isSchemeLayerIndex(const QModelIndex& index) const
{
    return isLayerNodeIndex(index);
}

QModelIndex CaseTreeView::schemeCaseIndexForLayer(const QModelIndex& layerIndex) const
{
    return caseIndexForLayer(layerIndex);
}

void CaseTreeView::expandCaseAndGroups(const QModelIndex& caseIdx)
{
    if (!m_caseModel || !caseIdx.isValid()) {
        return;
    }

    expand(caseIdx);

    const int groupRows = m_caseModel->rowCount(caseIdx);
    for (int g = 0; g < groupRows; ++g) {
        const QModelIndex groupIdx = m_caseModel->index(g, 0, caseIdx);
        if (groupIdx.isValid() && m_caseModel->rowCount(groupIdx) > 0) {
            expand(groupIdx);
        }
    }
}

void CaseTreeView::expandCasesAndGroups()
{
    if (!m_caseModel) {
        return;
    }

    const int caseRows = m_caseModel->rowCount();
    for (int r = 0; r < caseRows; ++r) {
        const QModelIndex caseIdx = m_caseModel->index(r, 0);
        if (!caseIdx.isValid()) {
            continue;
        }
        expandCaseAndGroups(caseIdx);
    }
}

void CaseTreeView::focusCaseNode(int caseId)
{
    if (!m_caseModel || caseId <= 0) {
        return;
    }

    collapseAll();

    const int caseRows = m_caseModel->rowCount();
    for (int r = 0; r < caseRows; ++r) {
        const QModelIndex caseIdx = m_caseModel->index(r, 0);
        if (!caseIdx.isValid()) {
            continue;
        }
        if (caseIdx.data(Qt::UserRole).toInt() != caseId) {
            continue;
        }

        // 仅展开被打开的方案节点，点/线/面分组保持默认折叠
        expand(caseIdx);
        setCurrentIndex(caseIdx);
        scrollTo(caseIdx, QAbstractItemView::PositionAtCenter);
        break;
    }
}

void CaseTreeView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    const bool isLeftButton = event->button() == Qt::LeftButton;

    if (isLeftButton && isCaseNodeIndex(index) && actionButtonRect(index).contains(event->pos())) {
        setCurrentIndex(index);
        m_contextIndex = index;
        m_contextMenuPos = event->pos();
        showContextMenuForIndex(index, viewport()->mapToGlobal(actionButtonRect(index).bottomLeft()));
        return;
    }

    if (isLeftButton && index.isValid() && isLayerGroupNodeIndex(index) && m_caseModel
        && m_caseModel->rowCount(index) > 0) {
        const bool onCheckbox = isIndexCheckBoxHit(index, event->pos());
        const bool onBranch = isLayerGroupBranchHit(index, event->pos());
        if (!onCheckbox && !onBranch) {
            setCurrentIndex(index);
            toggleLayerGroupExpanded(index);
            event->accept();
            return;
        }
    }

    QTreeView::mousePressEvent(event);

    if (isLeftButton && isCaseNodeIndex(index) && !actionButtonRect(index).contains(event->pos())) {
        setCurrentIndex(index);
        if (isExpanded(index)) {
            collapse(index);
        } else {
            expand(index);
        }
    }
}

void CaseTreeView::mouseReleaseEvent(QMouseEvent* event)
{
    QTreeView::mouseReleaseEvent(event);

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || !m_caseModel || !isLayerNodeIndex(index)) {
        return;
    }
    if (isIndexCheckBoxHit(index, event->pos())) {
        return;
    }
    if (index.data(Qt::CheckStateRole).toInt() != static_cast<int>(Qt::Checked)) {
        return;
    }

    const QString typeName =
        index.data(Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        return;
    }

    const QModelIndex caseIdx = caseIndexForLayer(index);
    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    if (caseId > 0) {
        emit schemeLayerActivated(caseId, typeName);
    }
}

void CaseTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && isLayerNodeIndex(index) && isIndexCheckBoxHit(index, event->pos())) {
        event->accept();
        return;
    }

    QTreeView::mouseDoubleClickEvent(event);
}

void CaseTreeView::onDoubleClicked(const QModelIndex& index)
{
    if (!m_caseModel) {
        return;
    }

    if (isCaseNodeIndex(index)) {
        const int caseId = m_caseModel->getItem(index)->data(0, Qt::UserRole).toInt();
        if (caseId > 0) {
            emit caseOpened(caseId);
        }
        return;
    }

    if (!isLayerNodeIndex(index)) {
        return;
    }

    const QString typeName =
        index.data(Qt::UserRole).toMap().value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        return;
    }

    const QModelIndex caseIdx = caseIndexForLayer(index);
    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    if (caseId > 0) {
        emit schemeLayerDataViewRequested(caseId, typeName);
    }
}

void CaseTreeView::onContextMenuRequested(const QPoint& pos)
{
    const QModelIndex index = indexAt(pos);
    if (isLayerNodeIndex(index)) {
        m_contextMenuPos = pos;
        showLayerContextMenuForIndex(index, viewport()->mapToGlobal(pos));
        return;
    }
    if (!isCaseNodeIndex(index)) {
        return;
    }

    m_contextMenuPos = pos;
    showContextMenuForIndex(index, viewport()->mapToGlobal(pos));
}

void CaseTreeView::onOpenCase()
{
    const int caseId = getCurrentCaseId();
    if (caseId > 0) {
        emit caseOpened(caseId);
    }
}

void CaseTreeView::onEditCase()
{
    const int caseId = getCurrentCaseId();
    if (caseId > 0) {
        emit caseEdited(caseId);
    }
}

void CaseTreeView::onDeleteCase()
{
    const int caseId = getCurrentCaseId();
    if (caseId <= 0 || !m_caseModel || !m_contextIndex.isValid()) {
        return;
    }

    CaseItem item;
    if (!m_caseModel->getCaseData(m_contextIndex, item)) {
        return;
    }

    emit caseDeleted(caseId);
}

void CaseTreeView::onRenameCase()
{
    const int caseId = getCurrentCaseId();
    if (caseId > 0) {
        emit caseRenameRequested(caseId);
    }
}

void CaseTreeView::onCloneCase()
{
    const int caseId = getCurrentCaseId();
    if (caseId > 0) {
        emit caseCloned(caseId);
    }
}

void CaseTreeView::onOverwriteCase()
{
    const int caseId = getCurrentCaseId();
    if (caseId > 0) {
        emit caseOverwriteRequested(caseId);
    }
}

void CaseTreeView::onSchemeLayerImportTriggered()
{
    const QModelIndex layerIdx = m_contextIndex;
    if (!isLayerNodeIndex(layerIdx) || !m_caseModel) {
        return;
    }

    const QModelIndex caseIdx = caseIndexForLayer(layerIdx);
    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    const QVariantMap meta = layerIdx.data(Qt::UserRole).toMap();
    const QString table =
        CaseDrcsvHelper::tableNameFromLayerVal(meta.value(QStringLiteral("val")).toString());
    const QString displayName = layerIdx.data(Qt::DisplayRole).toString();

    if (!caseIdx.isValid() || caseId <= 0 || table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"无法识别该图层对应的数据表（table），请确认接口 list 中图层 val 已配置（如 road_way、bus_point、other_od）。"));
        return;
    }

    emit schemeLayerCsvImportRequested(caseId, table, displayName);
}

void CaseTreeView::onSchemeLayerExportTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }

    const QString exportType = action->property("exportType").toString();
    const QModelIndex layerIdx = m_contextIndex;
    if (exportType.isEmpty() || !isLayerNodeIndex(layerIdx) || !m_caseModel) {
        return;
    }

    const QModelIndex caseIdx = caseIndexForLayer(layerIdx);
    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    const QVariantMap meta = layerIdx.data(Qt::UserRole).toMap();
    const QString table =
        CaseDrcsvHelper::tableNameFromLayerVal(meta.value(QStringLiteral("val")).toString());
    const QString displayName = layerIdx.data(Qt::DisplayRole).toString();

    if (!caseIdx.isValid() || caseId <= 0 || table.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"提示"),
            QString::fromUtf8(u8"无法识别该图层对应的数据表（table），请确认接口 list 中图层 val 已配置（如 road_way、bus_point、other_od）。"));
        return;
    }

    emit schemeLayerExportRequested(caseId, table, exportType, displayName);
}

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
void CaseTreeView::onSchemeLayerZoomToTriggered()
{
    const QModelIndex layerIdx = m_contextIndex;
    if (!isLayerNodeIndex(layerIdx) || !m_caseModel) {
        return;
    }

    const QModelIndex caseIdx = caseIndexForLayer(layerIdx);
    const int caseId = caseIdx.data(Qt::UserRole).toInt();
    const QVariantMap meta = layerIdx.data(Qt::UserRole).toMap();
    const QString typeName = meta.value(QStringLiteral("typename")).toString().trimmed();
    const QString displayName = layerIdx.data(Qt::DisplayRole).toString();

    if (!caseIdx.isValid() || caseId <= 0) {
        return;
    }

    emit schemeLayerZoomToRequested(caseId, typeName, displayName);
}
#endif

void CaseTreeView::initUi()
{
    setHeaderHidden(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setExpandsOnDoubleClick(false);
    setRootIsDecorated(true);
    setUniformRowHeights(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(this, &QTreeView::doubleClicked, this, &CaseTreeView::onDoubleClicked);
    connect(this, &QTreeView::customContextMenuRequested, this, &CaseTreeView::onContextMenuRequested);
}

QMenu* CaseTreeView::createContextMenu(const CaseItem& item)
{
    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(QString::fromUtf8(kCaseTreeMenuQss));

    QAction* openAction = menu->addAction(QString::fromUtf8(u8"打开"));
    connect(openAction, &QAction::triggered, this, &CaseTreeView::onOpenCase);

    QAction* editAction = menu->addAction(QString::fromUtf8(u8"编辑"));
    connect(editAction, &QAction::triggered, this, &CaseTreeView::onEditCase);

    QAction* renameAction = menu->addAction(QString::fromUtf8(u8"重命名"));
    connect(renameAction, &QAction::triggered, this, &CaseTreeView::onRenameCase);

    QAction* cloneAction = menu->addAction(QString::fromUtf8(u8"克隆"));
    connect(cloneAction, &QAction::triggered, this, &CaseTreeView::onCloneCase);

    menu->addSeparator();

    const bool isBaseCase = item.isBase == 1;
    if (isBaseCase) {
        QAction* overwriteAction = menu->addAction(QString::fromUtf8(u8"覆盖"));
        connect(overwriteAction, &QAction::triggered, this, &CaseTreeView::onOverwriteCase);
    } else {
        QAction* deleteAction = menu->addAction(QString::fromUtf8(u8"删除"));
        QFont deleteFont = deleteAction->font();
        deleteFont.setBold(true);
        deleteAction->setFont(deleteFont);
        connect(deleteAction, &QAction::triggered, this, &CaseTreeView::onDeleteCase);
    }

    return menu;
}

QMenu* CaseTreeView::createLayerContextMenu()
{
    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(QString::fromUtf8(kCaseTreeMenuQss));

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    QAction* zoomToLayerAction = menu->addAction(QString::fromUtf8(u8"缩放至图层"));
    connect(zoomToLayerAction, &QAction::triggered, this, &CaseTreeView::onSchemeLayerZoomToTriggered);
#endif

    QAction* importAction = menu->addAction(QString::fromUtf8(u8"导入"));
    connect(importAction, &QAction::triggered, this, &CaseTreeView::onSchemeLayerImportTriggered);

    QMenu* exportMenu = menu->addMenu(QString::fromUtf8(u8"导出"));
    QAction* exportExcelAction = exportMenu->addAction(QString::fromUtf8(u8"导出 Excel"));
    exportExcelAction->setProperty("exportType", QStringLiteral("xlsx"));
    connect(exportExcelAction, &QAction::triggered, this, &CaseTreeView::onSchemeLayerExportTriggered);

    QAction* exportCsvAction = exportMenu->addAction(QString::fromUtf8(u8"导出 CSV"));
    exportCsvAction->setProperty("exportType", QStringLiteral("csv"));
    connect(exportCsvAction, &QAction::triggered, this, &CaseTreeView::onSchemeLayerExportTriggered);

    return menu;
}

int CaseTreeView::getCurrentCaseId() const
{
    if (!m_caseModel) {
        return -1;
    }

    QModelIndex index = m_contextIndex.isValid() ? m_contextIndex : currentIndex();
    if (!isCaseNodeIndex(index)) {
        return -1;
    }

    return m_caseModel->getItem(index)->data(0, Qt::UserRole).toInt();
}

bool CaseTreeView::isCaseNodeIndex(const QModelIndex& index) const
{
    if (!index.isValid() || !m_caseModel) {
        return false;
    }

    TreeItemBase* item = m_caseModel->getItem(index);
    if (!item || !item->parentItem()) {
        return false;
    }

    return item->parentItem() == m_caseModel->rootItem();
}

bool CaseTreeView::isLayerGroupNodeIndex(const QModelIndex& index) const
{
    if (!index.isValid() || !m_caseModel) {
        return false;
    }
    const QVariantMap meta = index.data(Qt::UserRole).toMap();
    if (meta.value(QStringLiteral("nodeKind")).toString() != QStringLiteral("group")) {
        return false;
    }
    const QModelIndex parent = index.parent();
    return parent.isValid() && !parent.parent().isValid();
}

bool CaseTreeView::isLayerNodeIndex(const QModelIndex& index) const
{
    if (!index.isValid() || !m_caseModel) {
        return false;
    }

    const QVariantMap meta = index.data(Qt::UserRole).toMap();
    const bool hasTypename = !meta.value(QStringLiteral("typename")).toString().trimmed().isEmpty();
    if (!hasTypename && meta.value(QStringLiteral("nodeKind")).toString() != QStringLiteral("layer")) {
        return false;
    }

    const QModelIndex parent = index.parent();
    if (!parent.isValid()) {
        return false;
    }

    // 方案 → 图层组 → 图层
    if (parent.parent().isValid() && !parent.parent().parent().isValid()) {
        return parent.data(Qt::UserRole).toMap().value(QStringLiteral("nodeKind")).toString()
               == QStringLiteral("group");
    }

    // 兼容：方案 → 图层（旧两级结构）
    return !parent.parent().isValid();
}

QModelIndex CaseTreeView::caseIndexForLayer(const QModelIndex& layerIndex) const
{
    if (!layerIndex.isValid()) {
        return QModelIndex();
    }
    const QModelIndex parent = layerIndex.parent();
    if (!parent.isValid()) {
        return QModelIndex();
    }
    if (!parent.parent().isValid()) {
        return parent;
    }
    if (!parent.parent().parent().isValid()) {
        return parent.parent();
    }
    return QModelIndex();
}

bool CaseTreeView::isIndexCheckBoxHit(const QModelIndex& index, const QPoint& viewportPos) const
{
    if (!index.isValid() || !index.flags().testFlag(Qt::ItemIsUserCheckable)) {
        return false;
    }

    const QRect itemRect = visualRect(index);
    if (!itemRect.isValid()) {
        return false;
    }

    QStyleOptionViewItem option;
    option.rect = itemRect;
    option.state = QStyle::State_Enabled | QStyle::State_Active;
    if (hasFocus()) {
        option.state |= QStyle::State_HasFocus;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QStyle::SubElement checkSubElement = QStyle::SE_ItemViewItemCheckBox;
#else
    const QStyle::SubElement checkSubElement = QStyle::SE_ItemViewItemCheckIndicator;
#endif
    const QRect checkRect = style()->subElementRect(checkSubElement, &option, this);
    return checkRect.isValid() && checkRect.contains(viewportPos);
}

bool CaseTreeView::isLayerGroupBranchHit(const QModelIndex& index, const QPoint& viewportPos) const
{
    if (!index.isValid()) {
        return false;
    }

    const QRect rowRect = visualRect(index);
    if (!rowRect.isValid()) {
        return false;
    }

    const int branchWidth = indentation() + 12;
    const QRect branchRect(rowRect.left(), rowRect.top(), branchWidth, rowRect.height());
    return branchRect.contains(viewportPos);
}

void CaseTreeView::toggleLayerGroupExpanded(const QModelIndex& index)
{
    if (!index.isValid() || !isLayerGroupNodeIndex(index) || !m_caseModel || m_caseModel->rowCount(index) <= 0) {
        return;
    }

    if (isExpanded(index)) {
        collapse(index);
    } else {
        expand(index);
    }
}

QRect CaseTreeView::actionButtonRect(const QModelIndex& index) const
{
    const QRect rowRect = visualRect(index);
    return QRect(rowRect.right() - 26, rowRect.top() + 5, 20, rowRect.height() - 10);
}

void CaseTreeView::showContextMenuForIndex(const QModelIndex& index, const QPoint& globalPos)
{
    if (!isCaseNodeIndex(index)) {
        return;
    }

    m_contextIndex = index;
    setCurrentIndex(index);

    CaseItem item;
    if (!m_caseModel || !m_caseModel->getCaseData(index, item)) {
        return;
    }

    QMenu* menu = createContextMenu(item);
    menu->exec(globalPos);
    delete menu;
}

void CaseTreeView::showLayerContextMenuForIndex(const QModelIndex& index, const QPoint& globalPos)
{
    if (!isLayerNodeIndex(index)) {
        return;
    }

    m_contextIndex = index;
    setCurrentIndex(index);

    QMenu* menu = createLayerContextMenu();
    menu->exec(globalPos);
    delete menu;
}
