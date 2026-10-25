#include "tnfeatureaddtool.h"
#include "tnfeatureattributedialog.h"
#include "tnmapwidget.h"

#include "qgsmapcanvas.h"
#include "qgsvectorlayer.h"
#include "qgsgeometry.h"
#include "qgsfeature.h"
#include "qgsproject.h"
#include "qgsmapmouseevent.h"
#include "qgsmessagebar.h"
#include "qgsattributes.h"
#include "qgsrubberband.h"
#include "qgssnappingutils.h"
#include "qgspointlocator.h"
#include "qgsgeometryutils.h"
#include "qgscurve.h"

#include <QDialog>
#include <QKeyEvent>
#include "../publicctrls/messagedialog.h"
#include <QDebug>
#include <QtMath>

TNFeatureAddTool::TNFeatureAddTool(QgsMapCanvas* canvas, QgsVectorLayer* layer, QgsAdvancedDigitizingDockWidget* cadDockWidge, QgsMapToolCapture::CaptureMode mode)
    : QgsMapToolCapture(canvas, cadDockWidge, mode)
{
    mLayer = layer;
    setCursor(QCursor(Qt::CrossCursor));
}

TNFeatureAddTool::~TNFeatureAddTool()
{
    if (mSnapMarker) {
        delete mSnapMarker;
        mSnapMarker = nullptr;
    }
}

void TNFeatureAddTool::setAttributePrefillHandler(AttributePrefillFn handler)
{
    mAttributePrefillHandler = std::move(handler);
}

void TNFeatureAddTool::activate()
{
    QgsMapToolCapture::activate();

    // 初始化捕捉标记
    if (!mSnapMarker) {
        mSnapMarker = new QgsRubberBand(canvas(), Qgis::GeometryType::Polygon);
        //mSnapMarker->setStrokeColor(Qt::red);
        mSnapMarker->setStrokeColor(QColor("#ff00ff"));
        mSnapMarker->setFillColor(Qt::transparent);
        mSnapMarker->setWidth(1);
    }

    // 保存并设置捕捉配置
    mPreviousSnappingConfig = canvas()->snappingUtils()->config();
    QgsSnappingConfig config = mPreviousSnappingConfig;
    config.setEnabled(true);
    config.setMode(Qgis::SnappingMode::AllLayers);
    config.setTolerance(10);
    config.setUnits(Qgis::MapToolUnit::Pixels);
    // 设置自捕捉
    config.setSelfSnapping(true);
    // 启用顶点和中点捕捉
    //config.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment | Qgis::SnappingType::MiddleOfSegment | Qgis::SnappingType::LineEndpoint);
    config.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::MiddleOfSegment | Qgis::SnappingType::LineEndpoint);
    canvas()->snappingUtils()->setConfig(config);
}

void TNFeatureAddTool::deactivate()
{
    stopCapturing();

    QgsMapCanvas* mapCanvas = canvas();
    if (mapCanvas && mapCanvas->snappingUtils()) {
        mapCanvas->snappingUtils()->setConfig(mPreviousSnappingConfig);
    }

    if (mSnapMarker) {
        delete mSnapMarker;
        mSnapMarker = nullptr;
    }

    QgsMapToolCapture::deactivate();
}

void TNFeatureAddTool::cadCanvasReleaseEvent(QgsMapMouseEvent *e)
{
    // 确保有图层且可编辑
    if (!mLayer || !mLayer->isValid() || !mLayer->isEditable())
    {
        TnUi::showWarning(u8"警告", u8"请选择一个可编辑图层。");
        return;
    }

    // 调用父类方法处理点击事件
    QgsMapToolCapture::cadCanvasReleaseEvent(e);
}

void TNFeatureAddTool::cadCanvasMoveEvent(QgsMapMouseEvent *e)
{
    QgsMapToolCapture::cadCanvasMoveEvent(e);

    // 处理捕捉
    e->snapPoint();
    QgsPointLocator::Match match = e->mapPointMatch();
    if (match.isValid()) {
        SnapType type = SnapLineVertex;
        QString typeName = u8"线折点";

        QgsFeature feature;
        if (!match.layer()) {
            return;
        }
        QgsFeatureRequest req(match.featureId());
        req.setNoAttributes(); // 不需要属性，只需要几何
        match.layer()->getFeatures(req).nextFeature(feature);
        if (match.hasLineEndpoint())
        {
            if (feature.hasGeometry() && feature.geometry().type() == Qgis::GeometryType::Line) {
                // 检查是否为起点或终点
                QgsPointXY snapPt = match.point();
                QgsGeometry geom = feature.geometry();

                // 转换为多段线以便统一处理
                if (QgsWkbTypes::geometryType(geom.wkbType()) == Qgis::GeometryType::Line) {
                    // 遍历所有部分，检查捕捉点是否为任意部分的起点或终点
                    bool isStart = false;
                    bool isEnd = false;
                    for (auto it = geom.const_parts_begin(); it != geom.const_parts_end(); ++it) {
                        const QgsAbstractGeometry* part = *it;
                        // 使用 QgsCurve 基类以支持 LineString, CompoundCurve 等
                        const QgsCurve* curve = qgsgeometry_cast<const QgsCurve*>(part);
                        if (curve) {
                            QgsPointXY start(curve->startPoint());
                            QgsPointXY end(curve->endPoint());

                            // 使用容差比较，避免浮点数精度问题
                            if (snapPt.sqrDist(start) < 1e-8) isStart = true;
                            if (snapPt.sqrDist(end) < 1e-8) isEnd = true;
                        }
                    }

                    if (isStart) {
                        type = SnapLineStart;
                        typeName = u8"线起点";
                    }
                    else if (isEnd) {
                        type = SnapLineEnd;
                        typeName = u8"线终点";
                    }
                }
            }
        }
        else if (match.hasMiddleSegment()) {
            type = SnapLineMiddle;
            typeName = u8"线中点";
        }
        else if (match.hasVertex())
        {
            if (feature.hasGeometry() && feature.geometry().type() == Qgis::GeometryType::Point) {
                type = SnapPointFeature;
                typeName = u8"点要素";
            }
        }
        emit snapPointDetected(type, typeName);
        updateSnapMarker(match.point(), true);
    } else {
        emit snapPointDetected(SnapNone, "");
        updateSnapMarker(QgsPointXY(), false);
    }
}

void TNFeatureAddTool::updateSnapMarker(const QgsPointXY& point, bool visible)
{
    return;
    if (!mSnapMarker) return;

    if (!visible) {
        mSnapMarker->hide();
        return;
    }

    mSnapMarker->reset(Qgis::GeometryType::Polygon);
    
    // 绘制五角星
    // 中心点
    double cx = point.x();
    double cy = point.y();
    
    // 半径（地图单位）
    // 假设五角星外接圆半径为10像素
    double rOuter = 10 * canvas()->mapUnitsPerPixel();
    double rInner = rOuter * 0.4; // 内圆半径
    
    // 10个顶点
    for (int i = 0; i < 10; ++i) {
        double angle = (i * 36 - 18) * M_PI / 180.0; // -18度起始，使尖角向上
        double r = (i % 2 == 0) ? rOuter : rInner;
        double x = cx + r * cos(angle);
        double y = cy + r * sin(angle);
        mSnapMarker->addPoint(QgsPointXY(x, y));
    }
    mSnapMarker->closePoints(true);
    mSnapMarker->show();
}

void TNFeatureAddTool::geometryCaptured(const QgsGeometry &geometry)
{
    if (geometry.isEmpty())
        return;

    // 创建要素
    createFeature(geometry);
}

void TNFeatureAddTool::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
    {
        // 取消当前捕获
        stopCapturing();
        e->accept();
    }
    else
    {
        QgsMapToolCapture::keyPressEvent(e);
    }
}

QgsMapToolCapture::CaptureMode TNFeatureAddTool::layerCaptureMode(QgsVectorLayer* layer)
{
    Qgis::GeometryType geoType = layer->geometryType();
    switch (geoType)
    {
    case Qgis::GeometryType::Point:
        return QgsMapToolCapture::CapturePoint;
    case Qgis::GeometryType::Line:
        return QgsMapToolCapture::CaptureLine;
    case Qgis::GeometryType::Polygon:
        return QgsMapToolCapture::CapturePolygon;
    default:
        return QgsMapToolCapture::CaptureNone;
    }
    return QgsMapToolCapture::CaptureMode::CaptureNone;
}

bool TNFeatureAddTool::createFeature(const QgsGeometry& geometry)
{
    if (!mLayer || !mLayer->isValid() || !mLayer->isEditable())
        return false;
    
    // 创建新要素
    QgsFeature feature(mLayer->fields());
    feature.setGeometry(geometry);

    if (mAttributePrefillHandler) {
        QStringList readOnlyFieldNames;
        mAttributePrefillHandler(mLayer, feature, &readOnlyFieldNames);
        if (!showAttributeDialog(feature, readOnlyFieldNames))
            return false;
    } else if (!showAttributeDialog(feature, {})) {
        return false;
    }
    
    // 添加要素到图层
    if (mLayer->addFeature(feature))
    {
        // 刷新画布
        canvas()->refresh();
        
        // 发送要素添加信号
        emit featureAdded(mLayer, feature.id());
        return true;
    }
    
    return false;
}

bool TNFeatureAddTool::showAttributeDialog(QgsFeature& feature, const QStringList& readOnlyFieldNames)
{
    if (!mLayer || !mLayer->isValid())
        return false;
    
    // 显示属性编辑对话框
    TNFeatureAttributeDialog dialog(mLayer, feature, nullptr, QString::fromUtf8(u8"添加要素"), readOnlyFieldNames);
    dialog.adjustSize();
    int tWidth = dialog.width(), tHeight = dialog.height();
    QgsMapCanvas* cv = canvas();
    QPoint tPoint = cv->mapToGlobal(QPoint(cv->width(), cv->height()));
    dialog.setGeometry(tPoint.x() - tWidth, tPoint.y() - (cv->height() + tHeight) / 2, tWidth, tHeight);
    if (dialog.exec() == QDialog::Accepted)
    {
        // 应用属性更改
        QgsAttributeMap attrs = dialog.editedAttributes();
        for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it)
        {
            feature.setAttribute(it.key(), it.value());
        }
        return true;
    }
    
    return false;
}
