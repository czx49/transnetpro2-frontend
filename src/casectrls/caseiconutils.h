#ifndef CASEICONUTILS_H
#define CASEICONUTILS_H

#include <QIcon>
#include <QString>

namespace CaseIconUtils {

QString resolveIconPath(const QString& iconKey);

/** @brief 资源名为 case_tb_<stem>.png；选中态若无独立 _check 资源则复用同一张图 */
QIcon mapToolbarDualStateIcon(const QString& stem);

/** @brief 资源名为 case_tb_<stem>.png，无选中变体 */
QIcon mapToolbarSingleStateIcon(const QString& stem);

/** @brief 资源名为 workspace_<stem>.png；选中态为 workspace_<stem>_checked.png */
QIcon workbenchDualStateIcon(const QString& stem);

/** @brief 资源名为 workspace_<stem>.png，无选中变体 */
QIcon workbenchSingleStateIcon(const QString& stem);

/** @brief 返回 workspace_<stem>.png 资源路径 */
QString workbenchIconPath(const QString& stem);

} // namespace CaseIconUtils

#endif // CASEICONUTILS_H
