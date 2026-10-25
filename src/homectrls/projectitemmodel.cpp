/*****************************************************************//**
 * \file   projectitemmodel.cpp
 * \brief  项目数据模型实现
 * \details 提供首页项目列表所需的数据访问与搜索能力
 *
 * \author zhangmg
 * \date   April 2026
 * \History 首页重构版本
 *********************************************************************/
#include "projectitemmodel.h"

QString formatProjectRegionText(const QString& provinceName, const QString& cityName)
{
    const QString province = provinceName.trimmed();
    const QString city = cityName.trimmed();
    if (province.isEmpty() && city.isEmpty()) {
        return QString(u8"-");
    }
    if (city.isEmpty()) {
        return province;
    }
    if (province.isEmpty()) {
        return city;
    }
    return QString(u8"%1 / %2").arg(province, city);
}

QString formatProjectRegionText(const ProjectItem& item)
{
    return formatProjectRegionText(item.provinceName, item.cityName);
}

ProjectItemModel::ProjectItemModel(QObject* parent)
    : TableModelBase<ProjectItem>(parent)
{
}

ProjectItemModel::~ProjectItemModel()
{
}

const QStringList ProjectItemModel::headers() const
{
    static const QStringList s_headers = {
        u8"项目名称", u8"描述", u8"创建时间", u8"方案数", u8"状态"
    };

    return s_headers;
}

QVariant ProjectItemModel::colValue(const ProjectItem& data, int col) const
{
    switch (col) {
    case 0:
        return data.name;
    case 1:
        return data.description;
    case 2:
        return data.createTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    case 3:
        return data.caseCount;
    case 4:
        return data.status;
    default:
        return QVariant();
    }
}

QVariant ProjectItemModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    ProjectItem item;
    if (!rowData(index.row(), item)) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        return colValue(item, index.column());
    }

    if (index.column() != 0) {
        return QVariant();
    }

    switch (role) {
    case Qt::UserRole:
        return item.caseCount;
    case Qt::UserRole + 1:
        return item.createTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    case Qt::UserRole + 2:
        return item.status;
    case Qt::UserRole + 3:
        return item.description;
    case Qt::UserRole + 4:
        return item.id;
    case Qt::UserRole + 5:
        return item.path;
    default:
        return QVariant();
    }
}

void ProjectItemModel::addProject(const ProjectItem& item)
{
    QList<ProjectItem> items = datas();
    items.append(item);
    setDatas(items);
}

void ProjectItemModel::removeProject(int index)
{
    if (index < 0 || index >= rowCount()) {
        return;
    }

    QList<ProjectItem> items = datas();
    items.removeAt(index);
    setDatas(items);
}

void ProjectItemModel::updateProject(int index, const ProjectItem& item)
{
    if (index < 0 || index >= rowCount()) {
        return;
    }

    QList<ProjectItem> items = datas();
    items[index] = item;
    setDatas(items);
}

QList<int> ProjectItemModel::searchProjects(const QString& keyword) const
{
    QList<int> results;
    const QList<ProjectItem> items = datas();
    for (int i = 0; i < items.size(); ++i) {
        const ProjectItem& item = items.at(i);
        if (item.name.contains(keyword, Qt::CaseInsensitive)
            || item.description.contains(keyword, Qt::CaseInsensitive)) {
            results.append(i);
        }
    }

    return results;
}

bool ProjectItemModel::projectById(int projectId, ProjectItem& item) const
{
    const QList<ProjectItem> items = datas();
    for (const ProjectItem& projectItem : items) {
        if (projectItem.id == projectId) {
            item = projectItem;
            return true;
        }
    }

    return false;
}

int ProjectItemModel::nextProjectId() const
{
    int maxId = 0;
    const QList<ProjectItem> items = datas();
    for (const ProjectItem& item : items) {
        maxId = qMax(maxId, item.id);
    }

    return maxId + 1;
}
