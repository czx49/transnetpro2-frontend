#include "caseiconutils.h"

#include <QFile>
#include <QHash>

namespace {
QString iconResourcePath(const QString& iconName)
{
    return QStringLiteral(":/icons/icons/%1.png").arg(iconName);
}

QString workbenchResourcePath(const QString& stem)
{
    return iconResourcePath(QStringLiteral("workspace_") + stem);
}
} // namespace

namespace CaseIconUtils {

QIcon mapToolbarDualStateIcon(const QString& stem)
{
    QIcon icon;
    const QString normalPath = iconResourcePath(stem);
    const QString checkedPath = iconResourcePath(stem + QStringLiteral("_check"));
    if (!QFile::exists(normalPath)) {
        icon.addFile(resolveIconPath(QStringLiteral("chart")));
        return icon;
    }
    icon.addPixmap(normalPath, QIcon::Normal, QIcon::Off);
    if (QFile::exists(checkedPath)) {
        icon.addPixmap(checkedPath, QIcon::Normal, QIcon::On);
    } else {
        icon.addPixmap(normalPath, QIcon::Normal, QIcon::On);
    }
    return icon;
}

QIcon mapToolbarSingleStateIcon(const QString& stem)
{
    const QString normalPath = iconResourcePath(stem);
    if (QFile::exists(normalPath)) {
        return QIcon(normalPath);
    }
    return QIcon(resolveIconPath(QStringLiteral("chart")));
}

QIcon workbenchDualStateIcon(const QString& stem)
{
    QIcon icon;
    const QString normalPath = workbenchResourcePath(stem);
    const QString checkedPath = workbenchResourcePath(stem + QStringLiteral("_checked"));
    if (!QFile::exists(normalPath)) {
        return mapToolbarSingleStateIcon(QStringLiteral("chart"));
    }
    icon.addPixmap(normalPath, QIcon::Normal, QIcon::Off);
    if (QFile::exists(checkedPath)) {
        icon.addPixmap(checkedPath, QIcon::Normal, QIcon::On);
    } else {
        icon.addPixmap(normalPath, QIcon::Normal, QIcon::On);
    }
    return icon;
}

QIcon workbenchSingleStateIcon(const QString& stem)
{
    const QString normalPath = workbenchResourcePath(stem);
    if (QFile::exists(normalPath)) {
        return QIcon(normalPath);
    }
    return QIcon(resolveIconPath(QStringLiteral("chart")));
}

QString workbenchIconPath(const QString& stem)
{
    return workbenchResourcePath(stem);
}

QString resolveIconPath(const QString& iconKey)
{
    const QString normalPath = iconResourcePath(iconKey);
    if (QFile::exists(normalPath)) {
        return normalPath;
    }

    static const QHash<QString, QString> aliasMap = {
        {QStringLiteral("zoom_in"), QStringLiteral("navigation")},
        {QStringLiteral("zoom_out"), QStringLiteral("dropdown")},
        {QStringLiteral("edit"), QStringLiteral("view_1")},
        {QStringLiteral("measure"), QStringLiteral("chart")},
        {QStringLiteral("statistics"), QStringLiteral("chart")},
        {QStringLiteral("connect"), QStringLiteral("compare_view")},
        {QStringLiteral("share"), QStringLiteral("copy")},
        {QStringLiteral("print"), QStringLiteral("document")},
        {QStringLiteral("waveform"), QStringLiteral("chart")}
    };

    const QString alias = aliasMap.value(iconKey);
    if (!alias.isEmpty()) {
        const QString aliasPath = iconResourcePath(alias);
        if (QFile::exists(aliasPath)) {
            return aliasPath;
        }
    }

    return iconResourcePath(QStringLiteral("chart"));
}

} // namespace CaseIconUtils
