/*****************************************************************//**
 * \file   phase2dialogstyle.h
 * \brief  Phase2 FramelessDialog 深色弹窗外壳与控件套件样式
 *
 * \author zhangmg
 * \date   August 2026
 *********************************************************************/
#ifndef PHASE2DIALOGSTYLE_H
#define PHASE2DIALOGSTYLE_H

#include <QString>

namespace Phase2Ui {

/** @brief 深色壳上主文字 */
inline constexpr const char* kDialogTextPrimary = "#ffffff";
/** @brief 深色壳上次要文字 / 提示 */
inline constexpr const char* kDialogTextSecondary = "#cbd5e1";

/** @brief 深色外壳 + 标题栏 + 壳上 Label/CheckBox/Radio/GroupBox 白字 */
QString dialogShellStyleSheet();

/** @brief 主操作按钮：#2188ff 胶囊 */
QString dialogPrimaryButtonStyleSheet();

/** @brief 取消/关闭：透明底白描边幽灵按钮 */
QString dialogGhostButtonStyleSheet();

/** @brief 多操作场景次要按钮：白底描边 */
QString dialogSecondaryButtonStyleSheet();

/** @brief 深色壳页签（未选次要色、选中白字+蓝底）；pane 无描边/圆角以免与外壳 16px 叠弧；空名用 phase2DialogTab */
QString dialogTabStyleSheet(const QString& tabObjectName = QString());

/** @brief 加载进度条（默认蓝 chunk） */
QString dialogProgressBarStyleSheet();

/** @brief 白底内容板：表格/HTML/输出区等，深字可读；空名用 phase2DialogLightPanel */
QString dialogLightContentPanelStyleSheet(const QString& objectName = QString());

} // namespace Phase2Ui

#endif // PHASE2DIALOGSTYLE_H
