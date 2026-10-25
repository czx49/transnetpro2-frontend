/*****************************************************************//**
 * \file   projectlistwidget.h
 * \brief  项目列表控件
 * \details 提供项目列表展示、搜索和基础操作入口
 *
 * \author zhangmg
 * \date   April 2026
 * \History 首页重构版本
 *********************************************************************/
#ifndef PROJECTLISTWIDGET_H
#define PROJECTLISTWIDGET_H

#include "projectitemmodel.h"

#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QWidget>

class ProjectItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ProjectItemDelegate(QObject* parent = nullptr);
    ~ProjectItemDelegate() override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class ProjectListWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProjectListWidget(QWidget* parent = nullptr);
    ~ProjectListWidget() override;

    ProjectItemModel* model() const;
    void setModel(ProjectItemModel* model);
    int currentIndex() const;
    void refresh();

signals:
    void projectOpened(int projectId);
    void projectEdited(int projectId);
    void projectDeleted(int projectId);
    void newProjectRequested();

private slots:
    void onSearchTextChanged(const QString& text);
    void onProjectDoubleClicked(const QModelIndex& index);
    void onContextMenuRequested(const QPoint& pos);
    void onOpenProject();
    void onEditProject();
    void onDeleteProject();
    void onNewProjectButtonClicked();

private:
    void initUi();
    QMenu* createContextMenu();
    QModelIndex mapToSourceIndex(const QModelIndex& index) const;

private:
    ProjectItemModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    QListView* m_listView;
    ProjectItemDelegate* m_delegate;
    QLineEdit* m_searchEdit;
    QPushButton* m_newProjectBtn;
    QPoint m_contextMenuPos;
};

#endif // PROJECTLISTWIDGET_H
