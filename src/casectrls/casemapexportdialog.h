/*****************************************************************//**
 * \file   casemapexportdialog.h
 * \brief  保存画布（输出地图）参数配置对话框
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEMAPEXPORTDIALOG_H
#define CASEMAPEXPORTDIALOG_H

#include "../publicctrls/framelessdialog.h"

#include <QImage>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class TNMapWidget;

struct CaseMapExportOptions {
    enum class ExtentMode {
        CurrentView = 0,
        AllVisibleLayers
    };
    enum class Format {
        Png = 0,
        Jpeg,
        Tiff,
        Pdf
    };

    ExtentMode extentMode = ExtentMode::CurrentView;
    Format format = Format::Png;
    QString outputPath;
    int dpi = 96;
    bool includeBasemap = true;
    /** @brief 导出时合成到左下角的图例图像（GUI 线程预先抓取；为空则不合图） */
    QImage legendOverlay;
};

/** @brief 画布导出默认文件名上下文（项目名 + 可选方案名） */
struct CaseMapExportNameContext {
    QString projectName;
    QString caseName;
};

QString sanitizeFileNameSegment(const QString& raw);
QString buildDefaultExportBaseName(const CaseMapExportNameContext& ctx);
QString buildDefaultExportFileName(const CaseMapExportNameContext& ctx, const QString& ext);

class CaseMapExportDialog : public FramelessDialog
{
    Q_OBJECT

public:
    explicit CaseMapExportDialog(TNMapWidget* mapWidget,
                                 const CaseMapExportNameContext& nameContext = {},
                                 QWidget* parent = nullptr);
    ~CaseMapExportDialog() override;

    CaseMapExportOptions options() const;

private slots:
    void onBrowseClicked();
    void onFormatChanged(int index);
    void onAcceptClicked();

private:
    void initUi();
    void updateSizeHintLabel();
    static QString formatExtension(CaseMapExportOptions::Format format);
    static QString formatFilter(CaseMapExportOptions::Format format);

    TNMapWidget* m_mapWidget = nullptr;
    CaseMapExportNameContext m_nameContext;
    QComboBox* m_extentCombo = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QComboBox* m_dpiCombo = nullptr;
    QCheckBox* m_basemapCheck = nullptr;
    QLabel* m_sizeHintLabel = nullptr;
};

#endif // CASEMAPEXPORTDIALOG_H
