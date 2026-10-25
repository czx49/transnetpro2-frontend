/*****************************************************************//**
 * \file   projectitemmodel.h
 * \brief  项目数据模型
 * \details 管理首页项目列表的数据结构，继承自 TableModelBase
 *
 * \author zhangmg
 * \date   April 2026
 * \History 首页重构版本
 *********************************************************************/
#ifndef PROJECTITEMMODEL_H
#define PROJECTITEMMODEL_H

#include "../publicctrls/tablemodelbase.h"

/**
 * \brief  项目数据项
 */
struct ProjectItem {
    int id;
    QString name;
    QString description;
    QDateTime createTime;
    int caseCount;
    QString status;
    QString path;
    int provinceId = 0;
    QString provinceName;
    int cityId = 0;
    QString cityName;
};

QString formatProjectRegionText(const QString& provinceName, const QString& cityName);
QString formatProjectRegionText(const ProjectItem& item);

/**
 * \brief  首页项目数据模型
 */
class ProjectItemModel : public TableModelBase<ProjectItem> {
    Q_OBJECT

public:
    explicit ProjectItemModel(QObject* parent = nullptr);
    ~ProjectItemModel() override;

    const QStringList headers() const override;
    QVariant colValue(const ProjectItem& data, int col) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void addProject(const ProjectItem& item);
    void removeProject(int index);
    void updateProject(int index, const ProjectItem& item);
    QList<int> searchProjects(const QString& keyword) const;
    bool projectById(int projectId, ProjectItem& item) const;
    int nextProjectId() const;
};

#endif // PROJECTITEMMODEL_H
