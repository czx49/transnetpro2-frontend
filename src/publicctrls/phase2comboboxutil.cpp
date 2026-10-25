/*****************************************************************//**
 * \file   phase2comboboxutil.cpp
 * \brief  Phase2 输入控件闭合态与 QComboBox 下拉弹层样式工具实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "phase2comboboxutil.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLineEdit>
#include <QListView>
#include <QScrollBar>
#include <QSpinBox>

namespace Phase2Ui {

namespace {

ComboPopupTheme popupThemeFor(InputFieldTheme theme)
{
    if (theme == InputFieldTheme::DarkToolbar) {
        return ComboPopupTheme::DarkToolbar;
    }
    return ComboPopupTheme::LightDialog;
}

QString spinButtonStyle()
{
    return QStringLiteral(
        "QSpinBox::up-button, QDoubleSpinBox::up-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: top right;"
        "   width: 20px;"
        "   border-left: 1px solid #b8c7da;"
        "   background-color: #f8fafc;"
        "}"
        "QSpinBox::down-button, QDoubleSpinBox::down-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: bottom right;"
        "   width: 20px;"
        "   border-left: 1px solid #b8c7da;"
        "   background-color: #f8fafc;"
        "}"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {"
        "   width: 0; height: 0;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-bottom: 5px solid #475569;"
        "}"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
        "   width: 0; height: 0;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-top: 5px solid #475569;"
        "}");
}

QString lightCompactFieldBase(const QString& minHeight, const QString& radius, const QString& borderColor)
{
    return QStringLiteral(
        "QLineEdit, QSpinBox, QDoubleSpinBox, QDateTimeEdit {"
        "   min-height: %1;"
        "   max-height: %1;"
        "   border: 1px solid %2;"
        "   border-radius: %3;"
        "   background-color: #ffffff;"
        "   color: #111827;"
        "   padding: 0 8px;"
        "   font-size: 13px;"
        "}"
        "QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QDateTimeEdit:hover {"
        "   border-color: #4f96e8;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateTimeEdit:focus {"
        "   border-color: #4f96e8;"
        "}"
        "QLineEdit::placeholder {"
        "   color: #94a3b8;"
        "}"
        "QDateTimeEdit::drop-down {"
        "   subcontrol-origin: padding;"
        "   subcontrol-position: top right;"
        "   width: 24px;"
        "   border-left: 1px solid %2;"
        "   background-color: #f8fafc;"
        "   border-top-right-radius: %3;"
        "   border-bottom-right-radius: %3;"
        "}")
        .arg(minHeight, borderColor, radius)
        + spinButtonStyle();
}

QString lightCompactComboClosed(const QString& minHeight, const QString& radius, const QString& borderColor,
                                bool useImageArrow)
{
    QString dropDown = useImageArrow
        ? QStringLiteral(
              "QComboBox::drop-down {"
              "   subcontrol-origin: padding;"
              "   subcontrol-position: top right;"
              "   width: 24px;"
              "   border-left: 1px solid %1;"
              "   background-color: #f8fafc;"
              "   border-top-right-radius: %2;"
              "   border-bottom-right-radius: %2;"
              "}"
              "QComboBox::down-arrow {"
              "   image: url(:/icons/icons/dropdown@2x.png);"
              "   width: 10px; height: 10px;"
              "}"
              "QComboBox:hover { border-color: #4f96e8; }")
              .arg(borderColor, radius)
        : QStringLiteral(
              "QComboBox::drop-down {"
              "   subcontrol-origin: padding;"
              "   subcontrol-position: top right;"
              "   width: 24px;"
              "   border-left: 1px solid %1;"
              "   background-color: #f8fafc;"
              "   border-top-right-radius: %2;"
              "   border-bottom-right-radius: %2;"
              "}"
              "QComboBox::down-arrow {"
              "   width: 0; height: 0;"
              "   border-left: 5px solid transparent;"
              "   border-right: 5px solid transparent;"
              "   border-top: 6px solid #475569;"
              "}"
              "QComboBox:hover { border-color: #4f96e8; }"
              "QComboBox:hover::down-arrow { border-top-color: #2563eb; }")
              .arg(borderColor, radius);

    return QStringLiteral(
        "QComboBox {"
        "   min-height: %1;"
        "   max-height: %1;"
        "   min-width: 80px;"
        "   border: 1px solid %2;"
        "   border-radius: %3;"
        "   background-color: #ffffff;"
        "   color: #111827;"
        "   padding: 0 8px;"
        "}"
        "%4"
        "QComboBox QAbstractItemView {"
        "   background-color: #ffffff;"
        "   color: #1e293b;"
        "   border: 1px solid #b8c7da;"
        "   selection-background-color: #dbeafe;"
        "   selection-color: #111827;"
        "   outline: none;"
        "}")
        .arg(minHeight, borderColor, radius, dropDown);
}

QString listViewPopupStyleSheet(ComboPopupTheme theme)
{
    switch (theme) {
    case ComboPopupTheme::DarkToolbar:
        return QStringLiteral(
            "QListView#phase2ComboPopup {"
            "   background-color: #0d2547;"
            "   border: 1px solid rgba(255, 255, 255, 0.14);"
            "   border-radius: 8px;"
            "   padding: 4px;"
            "   outline: none;"
            "}"
            "QListView#phase2ComboPopup::item {"
            "   padding: 8px 12px;"
            "   color: #ffffff;"
            "   border-radius: 6px;"
            "   outline: none;"
            "}"
            "QListView#phase2ComboPopup::item:hover {"
            "   background-color: rgba(34, 137, 255, 0.20);"
            "}"
            "QListView#phase2ComboPopup::item:selected {"
            "   background-color: #2289ff;"
            "   color: #ffffff;"
            "}"
            "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
            "QScrollBar::handle:vertical {"
            "   background: rgba(255, 255, 255, 0.25); border-radius: 4px; min-height: 24px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            "QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }"
            "QScrollBar::handle:horizontal {"
            "   background: rgba(255, 255, 255, 0.25); border-radius: 4px; min-width: 24px; }"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");
    case ComboPopupTheme::LightDialog:
    default:
        return QStringLiteral(
            "QListView#phase2ComboPopup {"
            "   background-color: #ffffff;"
            "   border: 1px solid #b8c7da;"
            "   border-radius: 6px;"
            "   padding: 4px;"
            "   outline: none;"
            "}"
            "QListView#phase2ComboPopup::item {"
            "   padding: 6px 10px;"
            "   min-height: 18px;"
            "   color: #1e293b;"
            "   border-radius: 4px;"
            "   outline: none;"
            "}"
            "QListView#phase2ComboPopup::item:hover { background-color: #f1f5f9; }"
            "QListView#phase2ComboPopup::item:selected {"
            "   background-color: #dbeafe; color: #111827; }"
            "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
            "QScrollBar::handle:vertical {"
            "   background: #cbd5e1; border-radius: 4px; min-height: 24px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            "QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }"
            "QScrollBar::handle:horizontal {"
            "   background: #cbd5e1; border-radius: 4px; min-width: 24px; }"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");
    }
}

} // namespace

QString fieldStyleSheet(InputFieldTheme theme)
{
    switch (theme) {
    case InputFieldTheme::LightDialogRounded:
        return lightCompactFieldBase(QStringLiteral("30px"), QStringLiteral("8px"), QStringLiteral("#c5d0e0"));
    case InputFieldTheme::LightCompactSoft:
        return lightCompactFieldBase(QStringLiteral("30px"), QStringLiteral("8px"), QStringLiteral("#cbd5e1"));
    case InputFieldTheme::LightCompact:
        return lightCompactFieldBase(QStringLiteral("30px"), QStringLiteral("4px"), QStringLiteral("#b8c7da"));
    case InputFieldTheme::DarkToolbar:
    default:
        return QString();
    }
}

QString comboClosedStyleSheet(InputFieldTheme theme)
{
    switch (theme) {
    case InputFieldTheme::LightDialogRounded:
        return lightCompactComboClosed(QStringLiteral("30px"), QStringLiteral("8px"), QStringLiteral("#c5d0e0"), true);
    case InputFieldTheme::LightCompactSoft:
        return lightCompactComboClosed(QStringLiteral("30px"), QStringLiteral("8px"), QStringLiteral("#cbd5e1"), true);
    case InputFieldTheme::LightCompact:
        return lightCompactComboClosed(QStringLiteral("30px"), QStringLiteral("4px"), QStringLiteral("#b8c7da"), false);
    case InputFieldTheme::DarkToolbar:
        return QStringLiteral(
            "QComboBox {"
            "   min-height: 40px;"
            "   padding: 0 14px;"
            "   border: none;"
            "   border-radius: 12px;"
            "   background-color: rgba(255,255,255,0.10);"
            "   color: white;"
            "   font-size: 14px;"
            "}"
            "QComboBox::drop-down { border: none; width: 22px; }"
            "QComboBox::down-arrow { image: url(:/icons/icons/dropdown.png); width: 12px; height: 12px; }"
            "QComboBox QAbstractItemView {"
            "   background-color: #0d2547;"
            "   color: #ffffff;"
            "   border: 1px solid rgba(255, 255, 255, 0.14);"
            "   selection-background-color: #2289ff;"
            "   selection-color: #ffffff;"
            "   outline: none;"
            "}");
    default:
        return QString();
    }
}

void attachStyledComboPopup(QComboBox* combo, ComboPopupTheme theme)
{
    if (!combo) {
        return;
    }

    auto* view = new QListView(combo);
    view->setObjectName(QStringLiteral("phase2ComboPopup"));
    view->setFrameShape(QFrame::NoFrame);
    view->setUniformItemSizes(true);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setStyleSheet(listViewPopupStyleSheet(theme));
    combo->setView(view);
}

void setupThemedCombo(QComboBox* combo, InputFieldTheme theme)
{
    if (!combo) {
        return;
    }
    combo->setStyleSheet(comboClosedStyleSheet(theme));
    attachStyledComboPopup(combo, popupThemeFor(theme));
    if (theme != InputFieldTheme::DarkToolbar) {
        combo->setMinimumHeight(30);
        combo->setMaximumHeight(30);
    }
}

void applyFieldStyle(QWidget* widget, InputFieldTheme theme)
{
    if (!widget) {
        return;
    }
    const QString sheet = fieldStyleSheet(theme);
    if (sheet.isEmpty()) {
        return;
    }
    widget->setStyleSheet(sheet);
    if (theme != InputFieldTheme::DarkToolbar) {
        widget->setMinimumHeight(30);
        widget->setMaximumHeight(30);
    }
}

} // namespace Phase2Ui
