/*****************************************************************//**
 * \file   phase2comboboxutil.h
 * \brief  Phase2 输入控件闭合态与 QComboBox 下拉弹层样式工具
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef PHASE2COMBOBOXUTIL_H
#define PHASE2COMBOBOXUTIL_H

#include <QString>

class QComboBox;
class QWidget;

namespace Phase2Ui {

enum class ComboPopupTheme {
    LightDialog,
    DarkToolbar
};

enum class InputFieldTheme {
    LightDialogRounded,
    LightCompact,
    LightCompactSoft,
    DarkToolbar
};

/** @brief QLineEdit / QSpinBox / QDoubleSpinBox / QDateTimeEdit 共用闭合态 QSS */
QString fieldStyleSheet(InputFieldTheme theme);

/** @brief QComboBox 闭合态 QSS（含 drop-down / down-arrow） */
QString comboClosedStyleSheet(InputFieldTheme theme);

/** @brief 闭合态 + 下拉弹层（推荐唯一入口，勿仅 attach 且依赖父级 QSS） */
void setupThemedCombo(QComboBox* combo, InputFieldTheme theme);

/** @brief 为单个输入控件设置闭合态样式 */
void applyFieldStyle(QWidget* widget, InputFieldTheme theme);

/** @brief 为已设闭合态样式的 QComboBox 创建 QListView 弹层并应用主题 QSS */
void attachStyledComboPopup(QComboBox* combo, ComboPopupTheme theme);

} // namespace Phase2Ui

#endif // PHASE2COMBOBOXUTIL_H
