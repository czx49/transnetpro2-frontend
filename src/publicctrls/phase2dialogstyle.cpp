/*****************************************************************//**
 * \file   phase2dialogstyle.cpp
 * \brief  Phase2 FramelessDialog 深色弹窗外壳与控件套件样式实现
 *
 * \author zhangmg
 * \date   August 2026
 *********************************************************************/
#include "phase2dialogstyle.h"

namespace Phase2Ui {

QString dialogShellStyleSheet()
{
    return QStringLiteral(
        "FramelessDialog {"
        "   background-color: transparent;"
        "}"
        "#framelessDialogShell {"
        "   background-color: #051733;"
        "   border-radius: 16px;"
        "   border: 1px solid rgba(255, 255, 255, 0.18);"
        "}"
        "#framelessDialogTitleBar {"
        "   background-color: #083777;"
        "   border-top-left-radius: 16px;"
        "   border-top-right-radius: 16px;"
        "}"
        "#framelessDialogTitleLabel {"
        "   color: #ffffff;"
        "   font-size: 16px;"
        "   font-weight: 600;"
        "}"
        "#framelessDialogContent {"
        "   background-color: transparent;"
        "}"
        "#framelessDialogContent QLabel {"
        "   background: transparent;"
        "   color: #ffffff;"
        "}"
        "#framelessDialogContent QCheckBox {"
        "   background: transparent;"
        "   color: #ffffff;"
        "   spacing: 6px;"
        "}"
        "#framelessDialogContent QRadioButton {"
        "   background: transparent;"
        "   color: #ffffff;"
        "   spacing: 6px;"
        "}"
        "#framelessDialogContent QGroupBox {"
        "   background: transparent;"
        "   color: #ffffff;"
        "   font-weight: 700;"
        "   border: 1px solid rgba(255, 255, 255, 0.28);"
        "   border-radius: 10px;"
        "   margin-top: 10px;"
        "   padding: 10px;"
        "}"
        "#framelessDialogContent QGroupBox::title {"
        "   subcontrol-origin: margin;"
        "   left: 12px;"
        "   padding: 0 4px;"
        "   color: #ffffff;"
        "}");
}

QString dialogPrimaryButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton {"
        "   min-width: 82px;"
        "   min-height: 32px;"
        "   border: none;"
        "   border-radius: 16px;"
        "   background-color: #2188ff;"
        "   color: #ffffff;"
        "   font-size: 14px;"
        "   font-weight: 700;"
        "   padding: 0 16px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3a97ff;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #9bbde6;"
        "   color: rgba(255, 255, 255, 0.85);"
        "}");
}

QString dialogGhostButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton {"
        "   min-width: 82px;"
        "   min-height: 32px;"
        "   border: 1px solid rgba(255, 255, 255, 0.75);"
        "   border-radius: 16px;"
        "   background-color: transparent;"
        "   color: #ffffff;"
        "   font-size: 14px;"
        "   font-weight: 600;"
        "   padding: 0 16px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 0.10);"
        "   border-color: #ffffff;"
        "}"
        "QPushButton:disabled {"
        "   color: rgba(255, 255, 255, 0.45);"
        "   border-color: rgba(255, 255, 255, 0.30);"
        "}");
}

QString dialogSecondaryButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton {"
        "   min-height: 32px;"
        "   border: 1px solid #93c5fd;"
        "   border-radius: 16px;"
        "   background-color: #ffffff;"
        "   color: #1e3a5f;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   padding: 6px 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #e8f1ff;"
        "}");
}

QString dialogTabStyleSheet(const QString& tabObjectName)
{
    const QString name = tabObjectName.isEmpty() ? QStringLiteral("phase2DialogTab") : tabObjectName;
    return QStringLiteral(
        "QTabWidget#%1::pane {"
        "   border: none;"
        "   border-radius: 0;"
        "   background-color: transparent;"
        "   top: -1px;"
        "}"
        "QTabWidget#%1 QTabBar::tab {"
        "   min-width: 88px;"
        "   padding: 7px 18px;"
        "   margin-right: 4px;"
        "   border: none;"
        "   border-top-left-radius: 6px;"
        "   border-top-right-radius: 6px;"
        "   background-color: transparent;"
        "   color: #cbd5e1;"
        "   font-size: 13px;"
        "}"
        "QTabWidget#%1 QTabBar::tab:selected {"
        "   background-color: rgba(33, 136, 255, 0.25);"
        "   color: #ffffff;"
        "   font-weight: 600;"
        "}"
        "QTabWidget#%1 QTabBar::tab:hover:!selected {"
        "   background-color: rgba(255, 255, 255, 0.12);"
        "   color: #ffffff;"
        "}").arg(name);
}

QString dialogProgressBarStyleSheet()
{
    return QStringLiteral(
        "QProgressBar {"
        "   border: none;"
        "   border-radius: 5px;"
        "   background-color: rgba(255, 255, 255, 0.12);"
        "   text-align: center;"
        "   color: #ffffff;"
        "}"
        "QProgressBar::chunk {"
        "   border-radius: 5px;"
        "   background-color: #2188ff;"
        "}");
}

QString dialogLightContentPanelStyleSheet(const QString& objectName)
{
    const QString name = objectName.isEmpty() ? QStringLiteral("phase2DialogLightPanel") : objectName;
    return QStringLiteral(
        "QWidget#%1, QTextBrowser#%1, QPlainTextEdit#%1, QTableView#%1, QTableWidget#%1, QListWidget#%1 {"
        "   background-color: #ffffff;"
        "   color: #111827;"
        "   border: 1px solid #c7d8ee;"
        "   border-radius: 0;"
        "}"
        "QWidget#%1 QLabel {"
        "   background: transparent;"
        "   color: #111827;"
        "}"
        "QWidget#%1 QCheckBox {"
        "   background: transparent;"
        "   color: #1f2937;"
        "}").arg(name);
}

} // namespace Phase2Ui
