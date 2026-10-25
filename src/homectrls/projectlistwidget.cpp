/*****************************************************************//**
 * \file   projectlistwidget.cpp
 * \brief  项目列表控件实现
 * \details 实现项目搜索、展示与右键菜单
 *
 * \author zhangmg
 * \date   April 2026
 * \History 首页重构版本
 *********************************************************************/
#include "projectlistwidget.h"

#include "../publicctrls/messagedialog.h"
#include <QPainter>
#include <QStyleOptionViewItem>

namespace {
class ProjectFilterModel : public QSortFilterProxyModel {
public:
    explicit ProjectFilterModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (filterRegularExpression().pattern().isEmpty()) {
            return true;
        }

        const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        const QString name = sourceModel()->data(index, Qt::DisplayRole).toString();
        return name.contains(filterRegularExpression());
    }
};
} // namespace

ProjectItemDelegate::ProjectItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

ProjectItemDelegate::~ProjectItemDelegate()
{
}

void ProjectItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    if (!index.isValid()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRect cardRect = option.rect.adjusted(4, 4, -4, -4);
    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;

    QLinearGradient background(cardRect.topLeft(), cardRect.bottomLeft());
    if (isSelected) {
        background.setColorAt(0.0, QColor(17, 63, 142, 240));
        background.setColorAt(1.0, QColor(10, 35, 84, 240));
    } else if (isHovered) {
        background.setColorAt(0.0, QColor(17, 51, 114, 220));
        background.setColorAt(1.0, QColor(10, 32, 75, 220));
    } else {
        background.setColorAt(0.0, QColor(8, 24, 56, 215));
        background.setColorAt(1.0, QColor(8, 20, 46, 215));
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(background);
    painter->drawRoundedRect(cardRect, 12, 12);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(QColor(49, 151, 255, isSelected ? 200 : 90), isSelected ? 2 : 1));
    painter->drawRoundedRect(cardRect, 12, 12);

    const QString title = index.data(Qt::DisplayRole).toString();
    const QString time = index.data(Qt::UserRole + 1).toString();
    const QString status = index.data(Qt::UserRole + 2).toString();
    const int caseCount = index.data(Qt::UserRole).toInt();

    QRect titleRect = cardRect.adjusted(14, 12, -108, -48);
    painter->setPen(Qt::white);
    QFont titleFont = painter->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, title);

    QRect metaRect = cardRect.adjusted(14, 42, -14, -14);
    QFont metaFont = painter->font();
    metaFont.setPointSize(10);
    metaFont.setBold(false);
    painter->setFont(metaFont);
    painter->setPen(QColor(170, 192, 224));
    painter->drawText(metaRect, Qt::AlignLeft | Qt::AlignTop,
                      QString(u8"方案数 %1").arg(caseCount));
    painter->drawText(metaRect.adjusted(0, 22, 0, 0), Qt::AlignLeft | Qt::AlignTop, time);

    QRect statusRect(cardRect.right() - 88, cardRect.top() + 12, 72, 26);
    QColor statusColor(62, 131, 255, 215);
    if (status == u8"已完成") {
        statusColor = QColor(18, 171, 123, 210);
    } else if (status == u8"草稿") {
        statusColor = QColor(119, 135, 157, 210);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(statusColor);
    painter->drawRoundedRect(statusRect, 13, 13);

    painter->setPen(Qt::white);
    QFont statusFont = painter->font();
    statusFont.setPointSize(9);
    statusFont.setBold(true);
    painter->setFont(statusFont);
    painter->drawText(statusRect, Qt::AlignCenter, status);

    painter->restore();
}

QSize ProjectItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(240, 88);
}

ProjectListWidget::ProjectListWidget(QWidget* parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_proxyModel(new ProjectFilterModel(this))
    , m_listView(nullptr)
    , m_delegate(new ProjectItemDelegate(this))
    , m_searchEdit(nullptr)
    , m_newProjectBtn(nullptr)
{
    initUi();
}

ProjectListWidget::~ProjectListWidget()
{
}

ProjectItemModel* ProjectListWidget::model() const
{
    return m_model;
}

void ProjectListWidget::setModel(ProjectItemModel* model)
{
    m_model = model;
    m_proxyModel->setSourceModel(model);
    m_listView->setModel(m_proxyModel);
}

int ProjectListWidget::currentIndex() const
{
    const QModelIndex sourceIndex = mapToSourceIndex(m_listView->currentIndex());
    return sourceIndex.isValid() ? sourceIndex.row() : -1;
}

void ProjectListWidget::refresh()
{
    m_proxyModel->invalidate();
    m_listView->viewport()->update();
}

void ProjectListWidget::onSearchTextChanged(const QString& text)
{
    m_proxyModel->setFilterRegularExpression(QRegularExpression(
        QRegularExpression::escape(text),
        QRegularExpression::CaseInsensitiveOption));
}

void ProjectListWidget::onProjectDoubleClicked(const QModelIndex& index)
{
    const QModelIndex sourceIndex = mapToSourceIndex(index);
    if (!sourceIndex.isValid() || !m_model) {
        return;
    }

    ProjectItem item;
    if (m_model->rowData(sourceIndex.row(), item)) {
        emit projectOpened(item.id);
    }
}

void ProjectListWidget::onContextMenuRequested(const QPoint& pos)
{
    m_contextMenuPos = pos;
    QMenu* menu = createContextMenu();
    menu->exec(m_listView->viewport()->mapToGlobal(pos));
    delete menu;
}

void ProjectListWidget::onOpenProject()
{
    const QModelIndex sourceIndex = mapToSourceIndex(m_listView->indexAt(m_contextMenuPos));
    if (!sourceIndex.isValid() || !m_model) {
        return;
    }

    ProjectItem item;
    if (m_model->rowData(sourceIndex.row(), item)) {
        emit projectOpened(item.id);
    }
}

void ProjectListWidget::onEditProject()
{
    const QModelIndex sourceIndex = mapToSourceIndex(m_listView->indexAt(m_contextMenuPos));
    if (!sourceIndex.isValid() || !m_model) {
        return;
    }

    ProjectItem item;
    if (m_model->rowData(sourceIndex.row(), item)) {
        emit projectEdited(item.id);
    }
}

void ProjectListWidget::onDeleteProject()
{
    const QModelIndex sourceIndex = mapToSourceIndex(m_listView->indexAt(m_contextMenuPos));
    if (!sourceIndex.isValid() || !m_model) {
        return;
    }

    ProjectItem item;
    if (!m_model->rowData(sourceIndex.row(), item)) {
        return;
    }

    if (TnUi::confirmYesNo(u8"确认删除",
                            QString(u8"确定要删除项目“%1”吗？").arg(item.name))) {
        emit projectDeleted(item.id);
    }
}

void ProjectListWidget::onNewProjectButtonClicked()
{
    emit newProjectRequested();
}

void ProjectListWidget::initUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(12);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->setSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(u8"搜索项目名称...");
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "   min-height: 38px;"
        "   padding: 0 12px;"
        "   border-radius: 10px;"
        "   border: 1px solid rgba(53, 121, 220, 0.45);"
        "   background-color: rgba(7, 21, 49, 0.92);"
        "   color: white;"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid rgba(56, 160, 255, 0.95);"
        "}"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProjectListWidget::onSearchTextChanged);

    m_newProjectBtn = new QPushButton(u8"新建项目", this);
    m_newProjectBtn->setCursor(Qt::PointingHandCursor);
    m_newProjectBtn->setStyleSheet(
        "QPushButton {"
        "   min-height: 38px;"
        "   padding: 0 18px;"
        "   border-radius: 10px;"
        "   border: none;"
        "   background-color: #1e86ff;"
        "   color: white;"
        "   font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3197ff;"
        "}"
    );
    connect(m_newProjectBtn, &QPushButton::clicked, this, &ProjectListWidget::onNewProjectButtonClicked);

    topLayout->addWidget(m_searchEdit, 1);
    topLayout->addWidget(m_newProjectBtn);

    m_listView = new QListView(this);
    m_listView->setMouseTracking(true);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSpacing(4);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setStyleSheet(
        "QListView {"
        "   background-color: rgba(5, 18, 43, 0.72);"
        "   border: 1px solid rgba(40, 82, 145, 0.55);"
        "   border-radius: 14px;"
        "   padding: 6px;"
        "}"
    );
    connect(m_listView, &QListView::doubleClicked, this, &ProjectListWidget::onProjectDoubleClicked);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested,
            this, &ProjectListWidget::onContextMenuRequested);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_listView, 1);
}

QMenu* ProjectListWidget::createContextMenu()
{
    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu {"
        "   background-color: #071327;"
        "   border: 1px solid rgba(48, 106, 189, 0.65);"
        "   color: white;"
        "   padding: 8px;"
        "}"
        "QMenu::item {"
        "   padding: 8px 18px;"
        "   border-radius: 8px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: rgba(30, 134, 255, 0.2);"
        "}"
    );

    QAction* openAction = menu->addAction(u8"打开项目");
    connect(openAction, &QAction::triggered, this, &ProjectListWidget::onOpenProject);

    QAction* editAction = menu->addAction(u8"编辑项目");
    connect(editAction, &QAction::triggered, this, &ProjectListWidget::onEditProject);

    menu->addSeparator();

    QAction* deleteAction = menu->addAction(u8"删除项目");
    connect(deleteAction, &QAction::triggered, this, &ProjectListWidget::onDeleteProject);

    return menu;
}

QModelIndex ProjectListWidget::mapToSourceIndex(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    return m_proxyModel->mapToSource(index);
}
