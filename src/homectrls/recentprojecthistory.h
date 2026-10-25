/*****************************************************************//**
 * \file   recentprojecthistory.h
 * \brief  最近打开项目的本地 JSON 记忆
 * \details 与可执行文件同目录持久化，按最近打开时间排序
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef RECENTPROJECTHISTORY_H
#define RECENTPROJECTHISTORY_H

#include "projectitemmodel.h"

#include <QList>
#include <QString>

/**
 * \brief  一条最近打开记录（展示与持久化）
 */
struct RecentProjectEntry {
    int projectId = 0;
    QString name;
    QString path;
    QString provinceName;
    QString cityName;
    QDateTime lastOpened;
};

/**
 * \brief  管理 recent_projects.json 的加载、保存与 upsert
 */
class RecentProjectHistory {
public:
    RecentProjectHistory();

    static QString storagePath();

    bool load();
    bool save() const;

    void upsertFromProjectItem(const ProjectItem& item);
    bool enrichFromProjectItems(const QList<ProjectItem>& items);
    bool removeEntry(const RecentProjectEntry& entry);
    bool removeByProjectId(int projectId);

    QList<RecentProjectEntry> recentItems() const;

    static int maxPersistedEntries();
    static int maxDisplayedEntries();

private:
    static QString normalizedProjectPath(const QString& path);
    static QString stableKey(const RecentProjectEntry& entry);
    static QString stableKeyForProjectItem(const ProjectItem& item);

    void sortNewestFirstAndTrim();

    QList<RecentProjectEntry> m_items;
};

#endif // RECENTPROJECTHISTORY_H
