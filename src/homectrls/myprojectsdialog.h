/*****************************************************************//**
 * \file   myprojectsdialog.h
 * \brief  我的项目弹窗
 * \details 对齐首页设计图的项目选择弹窗，支持搜索、打开和新建项目入口
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef MYPROJECTSDIALOG_H
#define MYPROJECTSDIALOG_H

#include "framelessdialog.h"
#include "projectitemmodel.h"
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class MyProjectsDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit MyProjectsDialog(QWidget* parent = nullptr);
    ~MyProjectsDialog() override;

    void setModel(ProjectItemModel* model);
    void refresh();
    int selectedProjectId() const;

signals:
    void projectActivated(int projectId);
    void newProjectRequested();
    void projectEditRequested(int projectId);
    void projectDeleteRequested(int projectId);

private slots:
    void onKeywordChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    void onOpenClicked();
    void onNewProjectClicked();

private:
    QWidget* createRowWidget(const ProjectItem& item, bool selected);
    void rebuildList();
    void updateRowWidgets();

private:
    ProjectItemModel* m_model;
    QLineEdit* m_searchEdit;
    QListWidget* m_listWidget;
    QPushButton* m_openButton;
};

#endif // MYPROJECTSDIALOG_H
