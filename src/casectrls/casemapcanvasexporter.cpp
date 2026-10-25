/*****************************************************************//**
 * \file   casemapcanvasexporter.cpp
 * \brief  地图画布离屏渲染导出
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casemapcanvasexporter.h"

#include "../tnmapwidget/tnmapwidget.h"

#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>

#include <qgis.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmaprenderercustompainterjob.h>
#include <qgsmaprendererparalleljob.h>
#include <qgsmapsettings.h>
#include <qgsrectangle.h>

namespace {

QImage flattenForJpeg(const QImage& src)
{
    QImage flat(src.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    QPainter painter(&flat);
    painter.drawImage(0, 0, src);
    painter.end();
    return flat;
}

QgsRectangle mergeLayerExtents(const QList<QgsMapLayer*>& layers)
{
    QgsRectangle combined;
    for (QgsMapLayer* layer : layers) {
        if (!layer) {
            continue;
        }
        const QgsRectangle ext = layer->extent();
        if (ext.isNull() || !ext.isFinite()) {
            continue;
        }
        if (combined.isNull()) {
            combined = ext;
        } else {
            combined.combineExtentWith(ext);
        }
    }
    return combined;
}

} // namespace

QgsRectangle CaseMapCanvasExporter::computeExtent(TNMapWidget* mapWidget, const CaseMapExportOptions& options)
{
    if (!mapWidget || !mapWidget->canvas()) {
        return {};
    }
    if (options.extentMode == CaseMapExportOptions::ExtentMode::CurrentView) {
        return mapWidget->canvas()->extent();
    }
    const QList<QgsMapLayer*> layers = mapWidget->exportLayerStack(options.includeBasemap);
    QgsRectangle merged = mergeLayerExtents(layers);
    if (!merged.isNull()) {
        return merged;
    }
    return mapWidget->canvas()->extent();
}

QSize CaseMapCanvasExporter::computeOutputSize(TNMapWidget* mapWidget, int dpi)
{
    if (!mapWidget || !mapWidget->canvas()) {
        return {};
    }
    const int cw = mapWidget->canvas()->width();
    const int ch = mapWidget->canvas()->height();
    const int outW = qMax(1, static_cast<int>(qRound(cw * dpi / 96.0)));
    const int outH = qMax(1, static_cast<int>(qRound(ch * dpi / 96.0)));
    return QSize(outW, outH);
}

QgsMapSettings CaseMapCanvasExporter::buildExportSettings(TNMapWidget* mapWidget, const CaseMapExportOptions& options)
{
    QgsMapSettings settings = mapWidget->exportMapSettings(options.includeBasemap);
    settings.setExtent(computeExtent(mapWidget, options));
    settings.setOutputSize(computeOutputSize(mapWidget, options.dpi));
    settings.setOutputDpi(options.dpi);
    settings.setFlag(Qgis::MapSettingsFlag::Antialiasing, true);
    settings.setBackgroundColor(Qt::transparent);
    return settings;
}

QString CaseMapCanvasExporter::exportToFile(TNMapWidget* mapWidget, const CaseMapExportOptions& options)
{
    if (!mapWidget || !mapWidget->canvas()) {
        return QString::fromUtf8(u8"地图画布未就绪。");
    }
    if (options.outputPath.trimmed().isEmpty()) {
        return QString::fromUtf8(u8"输出路径为空。");
    }
    const QgsMapSettings settings = buildExportSettings(mapWidget, options);
    return renderAndSave(settings, options);
}

QString CaseMapCanvasExporter::renderAndSave(const QgsMapSettings& settings, const CaseMapExportOptions& options)
{
    const QSize outSize = settings.outputSize();
    if (outSize.isEmpty()) {
        return QString::fromUtf8(u8"输出尺寸无效。");
    }

    QgsMapRendererParallelJob job(settings);
    job.start();
    job.waitForFinished();

    QImage rendered = job.renderedImage();
    if (rendered.isNull()) {
        rendered = QImage(outSize, QImage::Format_ARGB32);
        rendered.fill(Qt::transparent);
        QPainter painter(&rendered);
        QgsMapRendererCustomPainterJob painterJob(settings, &painter);
        painterJob.start();
        painterJob.waitForFinished();
        painter.end();
    }

    if (rendered.isNull()) {
        return QString::fromUtf8(u8"地图渲染失败。");
    }

    if (rendered.format() != QImage::Format_ARGB32 && rendered.format() != QImage::Format_RGBA8888) {
        rendered = rendered.convertToFormat(QImage::Format_ARGB32);
    }

    // QGIS renderedImage 可能携带 devicePixelRatio，统一为纯像素坐标，
    // 避免合成时逻辑坐标与按物理尺寸计算的 y 错位
    rendered.setDevicePixelRatio(1.0);

    // 合成左下角图例（按 dpi 缩放，与画布内 overlay 边距一致）
    if (!options.legendOverlay.isNull()) {
        const double scale = options.dpi / 96.0;
        // 全链路统一 DPR 1.0：抓取端已归一到逻辑像素，这里再强制一次并显式缩放，
        // 用与图像同尺寸的目标矩形绘制，杜绝任何一端携带 devicePixelRatio 导致放大/错位
        QImage legend = options.legendOverlay;
        legend.setDevicePixelRatio(1.0);
        if (!qFuzzyCompare(scale, 1.0)) {
            legend = legend.scaled(legend.size() * scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        const int margin = qRound(10 * scale);
        const QRect legendRect(margin, rendered.height() - legend.height() - margin,
                               legend.width(), legend.height());
        qInfo() << QString::fromUtf8(u8"合成画布图例：dpi=%1 输出=%2x%3 图例=%4x%5 margin=%6 目标=(%7,%8)")
                       .arg(options.dpi)
                       .arg(rendered.width()).arg(rendered.height())
                       .arg(legend.width()).arg(legend.height())
                       .arg(margin)
                       .arg(legendRect.x()).arg(legendRect.y());
        QPainter legendPainter(&rendered);
        legendPainter.drawImage(legendRect, legend);
        legendPainter.end();
    }

    const QString path = options.outputPath.trimmed();

    switch (options.format) {
    case CaseMapExportOptions::Format::Jpeg: {
        const QImage flat = flattenForJpeg(rendered);
        if (!flat.save(path, "JPEG", 90)) {
            return QString::fromUtf8(u8"保存 JPEG 文件失败。");
        }
        break;
    }
    case CaseMapExportOptions::Format::Tiff:
        if (!rendered.save(path, "TIFF")) {
            return QString::fromUtf8(u8"保存 TIFF 文件失败。");
        }
        break;
    case CaseMapExportOptions::Format::Pdf: {
        QPdfWriter writer(path);
        writer.setResolution(options.dpi);
        const double widthMm = outSize.width() * 25.4 / options.dpi;
        const double heightMm = outSize.height() * 25.4 / options.dpi;
        writer.setPageSize(QPageSize(QSizeF(widthMm, heightMm), QPageSize::Millimeter));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));

        QPainter painter(&writer);
        painter.drawImage(QRect(0, 0, outSize.width(), outSize.height()), rendered);
        painter.end();
        break;
    }
    case CaseMapExportOptions::Format::Png:
    default:
        if (!rendered.save(path, "PNG")) {
            return QString::fromUtf8(u8"保存 PNG 文件失败。");
        }
        break;
    }

    return {};
}
