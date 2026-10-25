/*****************************************************************//**
 * \file   casemapexportdialog.cpp
 * \brief  保存画布（输出地图）参数配置对话框
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casemapexportdialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "../tnmapwidget/tnmapwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <qgsmapcanvas.h>

namespace {

constexpr int kLabelColumnWidth = 88;
constexpr int kFieldHeight = 32;
constexpr int kBrowseButtonWidth = 72;

QString browseButtonStyleSheet()
{
    return Phase2Ui::dialogSecondaryButtonStyleSheet()
        + QStringLiteral(
            "QPushButton {"
            "   min-width: %1px;"
            "   max-width: %1px;"
            "   min-height: %2px;"
            "   max-height: %2px;"
            "   padding: 0;"
            "}")
            .arg(kBrowseButtonWidth)
            .arg(kFieldHeight);
}

QString mapExportDialogStyleSheet()
{
    return Phase2Ui::dialogShellStyleSheet()
        + QStringLiteral(
            "#framelessDialogTitleBar {"
            "   border-bottom: 1px solid #c5d9f0;"
            "}"
            "#framelessDialogContent QLabel { background: transparent; color: #ffffff; font-size: 13px; }"
            "QCheckBox { color: #ffffff; font-size: 13px; spacing: 6px; background: transparent; }"
            "QCheckBox::indicator { width: 16px; height: 16px; }");
}

QLabel* createFieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setFixedWidth(kLabelColumnWidth);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setStyleSheet("QLabel { background: transparent; color: #ffffff; font-size: 13px; }");
    return label;
}

void tuneFieldHeight(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setMinimumHeight(kFieldHeight);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

} // namespace

QString sanitizeFileNameSegment(const QString& raw)
{
    QString segment = raw.trimmed();
    if (segment.isEmpty()) {
        return QString();
    }
    static const QRegularExpression invalidChars(QStringLiteral(R"([\\/:*?"<>|])"));
    segment.replace(invalidChars, QStringLiteral("_"));
    return segment;
}

QString buildDefaultExportBaseName(const CaseMapExportNameContext& ctx)
{
    QStringList parts;
    const QString project = sanitizeFileNameSegment(ctx.projectName);
    if (!project.isEmpty()) {
        parts.append(project);
    }
    const QString caseName = sanitizeFileNameSegment(ctx.caseName);
    if (!caseName.isEmpty()) {
        parts.append(caseName);
    }
    parts.append(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    return parts.join(QStringLiteral("_"));
}

QString buildDefaultExportFileName(const CaseMapExportNameContext& ctx, const QString& ext)
{
    const QString normalizedExt = ext.startsWith(QLatin1Char('.')) ? ext.mid(1) : ext;
    return QStringLiteral("%1.%2").arg(buildDefaultExportBaseName(ctx), normalizedExt);
}

CaseMapExportDialog::CaseMapExportDialog(TNMapWidget* mapWidget,
                                         const CaseMapExportNameContext& nameContext,
                                         QWidget* parent)
    : FramelessDialog(parent)
    , m_mapWidget(mapWidget)
    , m_nameContext(nameContext)
{
    setTitleText(QString::fromUtf8(u8"保存画布"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(520, 348);
    
    initUi();
    updateSizeHintLabel();

    setStyleSheet(mapExportDialogStyleSheet());
}

CaseMapExportDialog::~CaseMapExportDialog() = default;

void CaseMapExportDialog::initUi()
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(12);
    root->setContentsMargins(16, 12, 16, 16);

    auto* formGrid = new QGridLayout();
    formGrid->setContentsMargins(0, 0, 0, 0);
    formGrid->setHorizontalSpacing(10);
    formGrid->setVerticalSpacing(12);
    formGrid->setColumnStretch(1, 1);

    int row = 0;

    m_extentCombo = new QComboBox(this);
    m_extentCombo->addItem(QString::fromUtf8(u8"当前地图可视范围"),
        static_cast<int>(CaseMapExportOptions::ExtentMode::CurrentView));
    m_extentCombo->addItem(QString::fromUtf8(u8"全部可见图层范围"),
        static_cast<int>(CaseMapExportOptions::ExtentMode::AllVisibleLayers));
    Phase2Ui::setupThemedCombo(m_extentCombo, Phase2Ui::InputFieldTheme::LightCompact);
    tuneFieldHeight(m_extentCombo);
    formGrid->addWidget(createFieldLabel(QString::fromUtf8(u8"输出范围："), this), row, 0);
    formGrid->addWidget(m_extentCombo, row, 1);
    ++row;

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(QStringLiteral("PNG"), static_cast<int>(CaseMapExportOptions::Format::Png));
    m_formatCombo->addItem(QStringLiteral("JPEG"), static_cast<int>(CaseMapExportOptions::Format::Jpeg));
    m_formatCombo->addItem(QStringLiteral("TIFF"), static_cast<int>(CaseMapExportOptions::Format::Tiff));
    m_formatCombo->addItem(QStringLiteral("PDF"), static_cast<int>(CaseMapExportOptions::Format::Pdf));
    Phase2Ui::setupThemedCombo(m_formatCombo, Phase2Ui::InputFieldTheme::LightCompact);
    tuneFieldHeight(m_formatCombo);
    connect(m_formatCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &CaseMapExportDialog::onFormatChanged);
    formGrid->addWidget(createFieldLabel(QString::fromUtf8(u8"文件格式："), this), row, 0);
    formGrid->addWidget(m_formatCombo, row, 1);
    ++row;

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(QString::fromUtf8(u8"请选择输出文件路径"));
    Phase2Ui::applyFieldStyle(m_pathEdit, Phase2Ui::InputFieldTheme::LightCompact);
    tuneFieldHeight(m_pathEdit);

    auto* browseBtn = new QPushButton(QString::fromUtf8(u8"浏览…"), this);
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setStyleSheet(browseButtonStyleSheet());
    connect(browseBtn, &QPushButton::clicked, this, &CaseMapExportDialog::onBrowseClicked);

    auto* pathFieldWrap = new QWidget(this);
    auto* pathFieldLayout = new QHBoxLayout(pathFieldWrap);
    pathFieldLayout->setContentsMargins(0, 0, 0, 0);
    pathFieldLayout->setSpacing(8);
    pathFieldLayout->addWidget(m_pathEdit, 1);
    pathFieldLayout->addWidget(browseBtn, 0, Qt::AlignVCenter);

    formGrid->addWidget(createFieldLabel(QString::fromUtf8(u8"输出路径："), this), row, 0);
    formGrid->addWidget(pathFieldWrap, row, 1);
    ++row;

    m_dpiCombo = new QComboBox(this);
    m_dpiCombo->addItem(QString::fromUtf8(u8"72 DPI"), 72);
    m_dpiCombo->addItem(QString::fromUtf8(u8"96 DPI"), 96);
    m_dpiCombo->addItem(QString::fromUtf8(u8"150 DPI"), 150);
    m_dpiCombo->addItem(QString::fromUtf8(u8"300 DPI"), 300);
    m_dpiCombo->addItem(QString::fromUtf8(u8"600 DPI"), 600);
    m_dpiCombo->setCurrentIndex(1);
    Phase2Ui::setupThemedCombo(m_dpiCombo, Phase2Ui::InputFieldTheme::LightCompact);
    tuneFieldHeight(m_dpiCombo);
    connect(m_dpiCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateSizeHintLabel(); });
    formGrid->addWidget(createFieldLabel(QString::fromUtf8(u8"分辨率："), this), row, 0);
    formGrid->addWidget(m_dpiCombo, row, 1);
    ++row;

    m_basemapCheck = new QCheckBox(QString::fromUtf8(u8"输出天地图"), this);
    m_basemapCheck->setChecked(true);
    m_basemapCheck->setStyleSheet("QCheckBox{ color: #ffffff; font-size: 13px; spacing: 6px; background: transparent; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }");

    formGrid->addWidget(m_basemapCheck, row, 1);
    ++row;

    m_sizeHintLabel = new QLabel(this);
    m_sizeHintLabel->setStyleSheet(QStringLiteral("QLabel { color: #f8f8f8; font-size: 13px; }"));
    m_sizeHintLabel->setWordWrap(true);
    formGrid->addWidget(m_sizeHintLabel, row, 1);
    ++row;

    root->addLayout(formGrid);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), this);
    QPushButton* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
    okBtn->setDefault(true);
    okBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(okBtn, &QPushButton::clicked, this, &CaseMapExportDialog::onAcceptClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &CaseMapExportDialog::reject);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);

    if (m_pathEdit) {
        const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        const QString defaultPath =
            QDir(downloads).filePath(buildDefaultExportFileName(m_nameContext, QStringLiteral("png")));
        m_pathEdit->setText(defaultPath);
    }
}

CaseMapExportOptions CaseMapExportDialog::options() const
{
    CaseMapExportOptions opt;
    if (m_extentCombo) {
        opt.extentMode = static_cast<CaseMapExportOptions::ExtentMode>(m_extentCombo->currentData().toInt());
    }
    if (m_formatCombo) {
        opt.format = static_cast<CaseMapExportOptions::Format>(m_formatCombo->currentData().toInt());
    }
    if (m_pathEdit) {
        opt.outputPath = m_pathEdit->text().trimmed();
    }
    if (m_dpiCombo) {
        opt.dpi = m_dpiCombo->currentData().toInt();
        if (opt.dpi <= 0) {
            opt.dpi = 96;
        }
    }
    if (m_basemapCheck) {
        opt.includeBasemap = m_basemapCheck->isChecked();
    }
    return opt;
}

void CaseMapExportDialog::onBrowseClicked()
{
    const auto fmt = static_cast<CaseMapExportOptions::Format>(
        m_formatCombo ? m_formatCombo->currentData().toInt() : static_cast<int>(CaseMapExportOptions::Format::Png));
    const QString filter = formatFilter(fmt);
    const QString ext = formatExtension(fmt);
    QString suggested = m_pathEdit ? m_pathEdit->text().trimmed() : QString();
    if (suggested.isEmpty()) {
        suggested = buildDefaultExportFileName(m_nameContext, ext);
    }
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8(u8"保存画布"), suggested, filter);
    if (!path.isEmpty() && m_pathEdit) {
        m_pathEdit->setText(path);
    }
}

void CaseMapExportDialog::onFormatChanged(int /*index*/)
{
    if (m_pathEdit) {
        const QString current = m_pathEdit->text().trimmed();
        if (!current.isEmpty()) {
            const auto fmt = static_cast<CaseMapExportOptions::Format>(
                m_formatCombo ? m_formatCombo->currentData().toInt() : static_cast<int>(CaseMapExportOptions::Format::Png));
            QFileInfo info(current);
            m_pathEdit->setText(QDir(info.path()).filePath(QStringLiteral("%1.%2")
                .arg(info.completeBaseName(), formatExtension(fmt))));
        }
    }
    updateSizeHintLabel();
}

void CaseMapExportDialog::onAcceptClicked()
{
    const QString path = m_pathEdit ? m_pathEdit->text().trimmed() : QString();
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo info(path);
    if (info.exists() && !info.isWritable()) {
        return;
    }
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return;
    }
    accept();
}

void CaseMapExportDialog::updateSizeHintLabel()
{
    if (!m_sizeHintLabel || !m_mapWidget || !m_mapWidget->canvas()) {
        return;
    }
    const int cw = m_mapWidget->canvas()->width();
    const int ch = m_mapWidget->canvas()->height();
    const int dpi = m_dpiCombo ? m_dpiCombo->currentData().toInt() : 96;
    const int outW = qMax(1, static_cast<int>(qRound(cw * dpi / 96.0)));
    const int outH = qMax(1, static_cast<int>(qRound(ch * dpi / 96.0)));
    m_sizeHintLabel->setText(
        QString::fromUtf8(u8"输出约 %1×%2 像素（画布 %3×%4 @ %5 DPI）")
            .arg(outW)
            .arg(outH)
            .arg(cw)
            .arg(ch)
            .arg(dpi));
}

QString CaseMapExportDialog::formatExtension(CaseMapExportOptions::Format format)
{
    switch (format) {
    case CaseMapExportOptions::Format::Jpeg:
        return QStringLiteral("jpg");
    case CaseMapExportOptions::Format::Tiff:
        return QStringLiteral("tiff");
    case CaseMapExportOptions::Format::Pdf:
        return QStringLiteral("pdf");
    case CaseMapExportOptions::Format::Png:
    default:
        return QStringLiteral("png");
    }
}

QString CaseMapExportDialog::formatFilter(CaseMapExportOptions::Format format)
{
    switch (format) {
    case CaseMapExportOptions::Format::Jpeg:
        return QString::fromUtf8(u8"JPEG 图像 (*.jpg *.jpeg)");
    case CaseMapExportOptions::Format::Tiff:
        return QString::fromUtf8(u8"TIFF 图像 (*.tif *.tiff)");
    case CaseMapExportOptions::Format::Pdf:
        return QString::fromUtf8(u8"PDF 文档 (*.pdf)");
    case CaseMapExportOptions::Format::Png:
    default:
        return QString::fromUtf8(u8"PNG 图像 (*.png)");
    }
}
