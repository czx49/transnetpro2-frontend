/*****************************************************************//**
 * \file   recentprojecthistory.cpp
 * \brief  最近打开项目记忆实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "recentprojecthistory.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr int kJsonVersion = 1;
constexpr int kMaxPersistedEntries = 50;
constexpr int kMaxDisplayedEntries = 15;
} // namespace

RecentProjectHistory::RecentProjectHistory() = default;

QString RecentProjectHistory::storagePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("recent_projects.json"));
}

QString RecentProjectHistory::normalizedProjectPath(const QString& path)
{
    if (path.isEmpty()) {
        return QString();
    }
    const QFileInfo fi(path);
    return QDir::cleanPath(fi.absoluteFilePath());
}

QString RecentProjectHistory::stableKey(const RecentProjectEntry& entry)
{
    if (!entry.path.isEmpty()) {
        return entry.path;
    }
    return QStringLiteral("id:%1").arg(entry.projectId);
}

QString RecentProjectHistory::stableKeyForProjectItem(const ProjectItem& item)
{
    if (!item.path.isEmpty()) {
        return normalizedProjectPath(item.path);
    }
    return QStringLiteral("id:%1").arg(item.id);
}

bool RecentProjectHistory::load()
{
    m_items.clear();

    QFile file(storagePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError = {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray arr = root.value(QStringLiteral("items")).toArray();

    QHash<QString, RecentProjectEntry> dedup;
    for (const QJsonValue& val : arr) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject o = val.toObject();
        RecentProjectEntry e;
        e.projectId = o.value(QStringLiteral("projectId")).toInt();
        e.name = o.value(QStringLiteral("name")).toString();
        const QString rawPath = o.value(QStringLiteral("path")).toString();
        e.path = rawPath.isEmpty() ? QString() : normalizedProjectPath(rawPath);
        e.provinceName = o.value(QStringLiteral("provinceName")).toString().trimmed();
        e.cityName = o.value(QStringLiteral("cityName")).toString().trimmed();

        const QString iso = o.value(QStringLiteral("lastOpened")).toString();
        e.lastOpened = QDateTime::fromString(iso, Qt::ISODateWithMs);
        if (!e.lastOpened.isValid()) {
            e.lastOpened = QDateTime::fromString(iso, Qt::ISODate);
        }
        if (e.name.isEmpty() || !e.lastOpened.isValid()) {
            continue;
        }

        const QString key = stableKey(e);
        const auto it = dedup.find(key);
        if (it == dedup.end() || it.value().lastOpened < e.lastOpened) {
            dedup.insert(key, e);
        }
    }

    m_items = dedup.values();
    sortNewestFirstAndTrim();
    return true;
}

bool RecentProjectHistory::save() const
{
    QJsonArray arr;
    for (const RecentProjectEntry& e : m_items) {
        QJsonObject o;
        o.insert(QStringLiteral("projectId"), e.projectId);
        o.insert(QStringLiteral("name"), e.name);
        o.insert(QStringLiteral("path"), e.path);
        o.insert(QStringLiteral("provinceName"), e.provinceName);
        o.insert(QStringLiteral("cityName"), e.cityName);
        o.insert(QStringLiteral("lastOpened"), e.lastOpened.toString(Qt::ISODateWithMs));
        arr.append(o);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), kJsonVersion);
    root.insert(QStringLiteral("items"), arr);

    const QJsonDocument doc(root);
    QFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const qint64 written = file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return written > 0;
}

bool RecentProjectHistory::removeEntry(const RecentProjectEntry& entry)
{
    const QString key = stableKey(entry);
    for (int i = 0; i < m_items.size(); ++i) {
        if (stableKey(m_items.at(i)) == key) {
            m_items.removeAt(i);
            return true;
        }
    }
    return false;
}

bool RecentProjectHistory::removeByProjectId(int projectId)
{
    if (projectId <= 0) {
        return false;
    }

    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).projectId == projectId) {
            m_items.removeAt(i);
            return true;
        }
    }
    return false;
}

void RecentProjectHistory::upsertFromProjectItem(const ProjectItem& item)
{
    RecentProjectEntry e;
    e.projectId = item.id;
    e.name = item.name;
    e.path = item.path.isEmpty() ? QString() : normalizedProjectPath(item.path);
    e.provinceName = item.provinceName.trimmed();
    e.cityName = item.cityName.trimmed();
    e.lastOpened = QDateTime::currentDateTime();

    const QString key = stableKeyForProjectItem(item);
    for (int i = 0; i < m_items.size(); ++i) {
        if (stableKey(m_items.at(i)) == key) {
            m_items[i] = e;
            sortNewestFirstAndTrim();
            return;
        }
    }
    m_items.append(e);
    sortNewestFirstAndTrim();
}

bool RecentProjectHistory::enrichFromProjectItems(const QList<ProjectItem>& items)
{
    if (items.isEmpty() || m_items.isEmpty()) {
        return false;
    }

    QHash<int, ProjectItem> byId;
    for (const ProjectItem& item : items) {
        if (item.id > 0) {
            byId.insert(item.id, item);
        }
    }
    if (byId.isEmpty()) {
        return false;
    }

    bool changed = false;
    for (RecentProjectEntry& entry : m_items) {
        if (entry.projectId <= 0) {
            continue;
        }
        const auto it = byId.constFind(entry.projectId);
        if (it == byId.constEnd()) {
            continue;
        }

        const ProjectItem& item = it.value();
        const QString province = item.provinceName.trimmed();
        const QString city = item.cityName.trimmed();
        if (province.isEmpty() && city.isEmpty()) {
            continue;
        }

        if (entry.provinceName.trimmed().isEmpty() && !province.isEmpty()) {
            entry.provinceName = province;
            changed = true;
        }
        if (entry.cityName.trimmed().isEmpty() && !city.isEmpty()) {
            entry.cityName = city;
            changed = true;
        }
    }

    return changed;
}

QList<RecentProjectEntry> RecentProjectHistory::recentItems() const
{
    return m_items;
}

int RecentProjectHistory::maxPersistedEntries()
{
    return kMaxPersistedEntries;
}

int RecentProjectHistory::maxDisplayedEntries()
{
    return kMaxDisplayedEntries;
}

void RecentProjectHistory::sortNewestFirstAndTrim()
{
    std::sort(m_items.begin(), m_items.end(), [](const RecentProjectEntry& a, const RecentProjectEntry& b) {
        return a.lastOpened > b.lastOpened;
    });
    while (m_items.size() > kMaxPersistedEntries) {
        m_items.removeLast();
    }
}
