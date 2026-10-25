#include "tnmaptoolselect.h"
#include "qgsrubberband.h"
#include "qgsvectorlayer.h"
#include "qgsgeometry.h"
#include "qgsmapcanvas.h"
#include "qgsfeature.h"
#include "qgsfeaturerequest.h"
#include "qgscircle.h"
#include "qgspolygon.h"
#include "qgslogger.h"
#include "qgsapplication.h"
#include "tnmapwidget.h"
#include <qgsmapmouseevent.h>

TnMapToolSelect::TnMapToolSelect(QgsMapCanvas *canvas, QgsAdvancedDigitizingDockWidget *cadDockWidget)
    : QgsMapToolCaptureLayerGeometry(canvas, cadDockWidget, QgsMapToolCapture::CaptureNone)
{
    initRubberBand();
    setCursor(Qt::CrossCursor);
}

TnMapToolSelect::~TnMapToolSelect()
{
    //if (mRubberBand && mCanvas)
    //{
    //    canvas()->scene()->removeItem(mRubberBand.get());
    //}
    mRubberBand.release(); //QGIS会析构，此处直接释放
}

void TnMapToolSelect::activate()
{
    QgsMapToolCaptureLayerGeometry::activate();
    canvas()->setCursor(Qt::CrossCursor);
    
    for (auto layer : canvas()->layers())
    {
        // 记录当前选择状态
        QgsVectorLayer* currentLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (currentLayer)
        {
            mPreviousSelectedIds[currentLayer] = currentLayer->selectedFeatureIds();
        }
        else
        {
        }
    }
}

void TnMapToolSelect::deactivate()
{
    QgsMapToolCaptureLayerGeometry::deactivate();
    if (mRubberBand)
    {
        mRubberBand->reset();
    }
    mSelecting = false;
    stopCapturing();
}

void TnMapToolSelect::canvasPressEvent(QgsMapMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
    {
        // 右键取消选择
        if (e->button() == Qt::RightButton && mSelecting)
        {
            mRubberBand->reset();
            mSelecting = false;
            stopCapturing();
        }
        return;
    }

    if (!mSelecting)
    {
        mSelecting = true;
        mRubberBand->reset();
        mStartPoint = e->mapPoint();
        
        // 根据选择模式设置捕获模式
        switch (mSelectionMode)
        {
            case Point:
            {
                if (mPointDragAsRect) {
                    // 统一模式：按下仅记录起点，松开时按位移判定点选/框选
                    mPressScreenPos = e->pos();
                    mRubberBand->reset(Qgis::GeometryType::Polygon);
                } else {
                    // 获取「像素→地图单位」的转换系数（关键：每像素对应多少地图单位）
                    const QgsMapToPixel& mapToPixel = canvas()->mapSettings().mapToPixel();
                    double pixelSizeX = mapToPixel.mapUnitsPerPixel(); // X方向每像素的地图单位
                    double pixelSizeY = pixelSizeX; // 大部分投影下XY方向像素单位一致，特殊情况可单独计算

                    // 计算5像素对应的地图单位长度（中心扩展2.5像素）
                    double halfPixel = 2.5; // 5像素的一半
                    double expandX = halfPixel * pixelSizeX;
                    double expandY = halfPixel * pixelSizeY;

                    // 构造以mStartPoint为中心的矩形（地图坐标系）
                    QgsRectangle selectRect(
                        mStartPoint.x() - expandX,  // 左
                        mStartPoint.y() - expandY,  // 上
                        mStartPoint.x() + expandX,  // 右
                        mStartPoint.y() + expandY   // 下
                    );
                    mSelectionGeometry = QgsGeometry::fromRect(selectRect);
                    //mSelectionGeometry = QgsGeometry::fromPointXY(mStartPoint);
                    selectFeatures(mSelectionGeometry);
                    mSelecting = false;
                }
            } break;
            case Rectangle:
                // 矩形选择 - 只需记录起点，移动时更新矩形
                mRubberBand->reset(Qgis::GeometryType::Polygon);
                updateRectangleGeometry(mStartPoint);
                break;
                
            case Circle:
                // 圆形选择 - 记录圆心，移动时更新半径
                mRubberBand->reset(Qgis::GeometryType::Polygon);
                updateCircleGeometry(mStartPoint);
                break;
                
            case Polygon:
                // 多边形选择 - 使用捕获工具
                mRubberBand->reset(Qgis::GeometryType::Polygon);
                startCapturing();
                addVertex(e->mapPoint());
                break;
        }
    }
}

void TnMapToolSelect::canvasReleaseEvent(QgsMapMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !mSelecting)
        return;

    switch (mSelectionMode)
    {
        case Point:
            if (mPointDragAsRect) {
                // 位移小于约 4 像素判定为点选/拖拽的分界；点选时按 10 像素选取容差选要素，否则按拖拽框选处理
                const bool isClick = (e->pos() - mPressScreenPos).manhattanLength() < 4;
                if (isClick) {
                    selectAtTolerance(mStartPoint);
                } else {
                    updateRectangleGeometry(e->mapPoint());
                    selectFeatures(mSelectionGeometry);
                }
                mSelecting = false;
                mRubberBand->reset();
            }
            break;

        case Rectangle:
            // 完成矩形选择
            updateRectangleGeometry(e->mapPoint());
            selectFeatures(mSelectionGeometry);
            mSelecting = false;
            mRubberBand->reset();
            break;
            
        case Circle:
            // 完成圆形选择
            updateCircleGeometry(e->mapPoint());
            selectFeatures(mSelectionGeometry);
            mSelecting = false;
            mRubberBand->reset();
            break;
            
        case Polygon:
            // 多边形选择 - 检查是否是最后一个点
            if (e->modifiers() & Qt::ControlModifier || e->modifiers() & Qt::ShiftModifier)
            {
                // 按Ctrl或Shift键结束多边形绘制
                stopCapturing();
                mSelecting = false;
            }
            else
            {
                // 添加新的顶点
                addVertex(e->mapPoint());
            }
            break;
            
        default:
            break;
    }
}

void TnMapToolSelect::canvasMoveEvent(QgsMapMouseEvent *e)
{
    if (!mSelecting)
        return;

    QgsPointXY currentPoint = e->mapPoint();

    switch (mSelectionMode)
    {
        case Point:
            if (mPointDragAsRect) {
                // 统一模式下拖拽实时显示框选矩形
                updateRectangleGeometry(currentPoint);
            }
            break;

        case Rectangle:
            updateRectangleGeometry(currentPoint);
            break;
            
        case Circle:
            updateCircleGeometry(currentPoint);
            break;
            
        case Polygon:
            if (isCapturing() && size() > 0)
            {
                // 更新临时多边形
                QgsPoint lastPoint = pointsZM().last();
                mRubberBand->movePoint(QgsPointXY(lastPoint.x(), lastPoint.y()));
                mRubberBand->addPoint(currentPoint, false);
            }
            break;
            
        default:
            break;
    }
}

void TnMapToolSelect::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
    {
        // 按ESC键取消选择
        mRubberBand->reset();
        mSelecting = false;
        stopCapturing();
    }
    else if ((e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete) && 
             mSelectionMode == Polygon && isCapturing() && size() > 0)
    {
        // 按退格或删除键移除最后一个点（仅多边形模式）
        undo();
        mRubberBand->removeLastPoint();
    }
    else
    {
        QgsMapToolCaptureLayerGeometry::keyPressEvent(e);
    }
}

void TnMapToolSelect::layerPointCaptured(const QgsPoint &point)
{
    // 点选模式已在canvasPressEvent中处理
    // 多边形模式会在捕获完成后通过layerPolygonCaptured处理
}

void TnMapToolSelect::layerLineCaptured(const QgsCurve *line)
{
    Q_UNUSED(line);
    // 本工具不处理线捕获
}

void TnMapToolSelect::layerPolygonCaptured(const QgsCurvePolygon *polygon)
{
    if (mSelectionMode == Polygon)
    {
        mSelectionGeometry = QgsGeometry(polygon->clone());
        selectFeatures(mSelectionGeometry);
        mRubberBand->reset();
    }
}

void TnMapToolSelect::selectAtTolerance(const QgsPointXY& center)
{
    // 获取「像素→地图单位」的转换系数
    const QgsMapToPixel& mapToPixel = canvas()->mapSettings().mapToPixel();
    const double pixelSize = mapToPixel.mapUnitsPerPixel();
    // 10 像素容差（中心各扩展 5 像素）
    const double halfPixel = 5.0;
    const QgsRectangle selectRect(center.x() - halfPixel * pixelSize,
                                  center.y() - halfPixel * pixelSize,
                                  center.x() + halfPixel * pixelSize,
                                  center.y() + halfPixel * pixelSize);
    mSelectionGeometry = QgsGeometry::fromRect(selectRect);
    selectFeatures(mSelectionGeometry);
}

void TnMapToolSelect::initRubberBand()
{
    mRubberBand = std::make_unique<QgsRubberBand>(canvas(), Qgis::GeometryType::Polygon);
    
    // 设置橡胶带样式
    QColor fillColor(255, 255, 0, 30);    // 黄色半透明
    QColor strokeColor(255, 0, 0, 200);   // 红色边框
    mRubberBand->setFillColor(fillColor);
    mRubberBand->setStrokeColor(strokeColor);
    //mRubberBand->setStrokeWidth(2);
    mRubberBand->setWidth(2);
    mRubberBand->setLineStyle(Qt::DashLine);
}

void TnMapToolSelect::selectFeatures(const QgsGeometry &geometry)
{
    for (auto layer : canvas()->layers())
    {
        QgsVectorLayer* currentLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (!currentLayer)
            continue;

        // 保存操作前的选择状态
        QgsFeatureIds previousSelected = currentLayer->selectedFeatureIds();

        // 根据选择模式和几何图形选择要素
        QgsFeatureRequest request;
        auto layerBound = TNMapWidget::tranformMap2Layer(geometry.boundingBox(), layer);
        auto layerGeometry = QgsGeometry::fromWkt(layerBound.asWktPolygon());
        request.setFilterRect(layerBound);

        QgsFeatureIds selectedIds;
        QgsFeature f;
        QgsFeatureIterator it = currentLayer->getFeatures(request);

        while (it.nextFeature(f))
        {
            if (layerGeometry.intersects(f.geometry()))
            {
                selectedIds.insert(f.id());
            }
        }

        // 根据选择行为模式更新选择集
        switch (mSelectBehavior)
        {
        case NewSelection:
            currentLayer->selectByIds(selectedIds);
            break;
        case AddToSelection:
            currentLayer->selectByIds(selectedIds, Qgis::SelectBehavior::AddToSelection);
            break;
        case RemoveFromSelection:
            currentLayer->deselect(selectedIds);
            break;
        case IntersectSelection:
        {
            QgsFeatureIds currentSelected = currentLayer->selectedFeatureIds();
            QgsFeatureIds intersectIds = currentSelected.intersect(selectedIds);
            currentLayer->selectByIds(intersectIds);
            break;
        }
        }

        // 计算选择变化
        QgsFeatureIds newSelected = currentLayer->selectedFeatureIds();
        QgsFeatureIds added = newSelected - previousSelected;
        QgsFeatureIds removed = previousSelected - newSelected;

        // 触发选择变化信号
        if (!added.isEmpty() || !removed.isEmpty())
        {
            emit selectionChanged(currentLayer, added, removed);
        }

        // 更新上次选择ID
        mPreviousSelectedIds[currentLayer] = newSelected;
    }

    // 刷新画布
    canvas()->refresh();
}

void TnMapToolSelect::clearSelection()
{
    for (auto layer : canvas()->layers())
    {
        QgsVectorLayer* currentLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (currentLayer)
        {
            QgsFeatureIds previouslySelected = currentLayer->selectedFeatureIds();
            currentLayer->removeSelection();

            if (!previouslySelected.isEmpty())
            {
                emit selectionChanged(currentLayer, QgsFeatureIds(), previouslySelected);
            }
            mPreviousSelectedIds[currentLayer].clear();
        }
    }
}

void TnMapToolSelect::updateRectangleGeometry(const QgsPointXY &currentPoint)
{
    // 创建矩形几何
    double x1 = std::min(mStartPoint.x(), currentPoint.x());
    double y1 = std::min(mStartPoint.y(), currentPoint.y());
    double x2 = std::max(mStartPoint.x(), currentPoint.x());
    double y2 = std::max(mStartPoint.y(), currentPoint.y());
    
    QgsPolygonXY polygon;
    QgsPolylineXY lines;
    lines << QgsPointXY(x1, y1) << QgsPointXY(x2, y1) << QgsPointXY(x2, y2) << QgsPointXY(x1, y2) << QgsPointXY(x1, y1);
    polygon << lines;
    
    mSelectionGeometry = QgsGeometry::fromPolygonXY(polygon);
    mRubberBand->setToGeometry(mSelectionGeometry, nullptr);
}

void TnMapToolSelect::updateCircleGeometry(const QgsPointXY &currentPoint)
{
    // 计算半径（起点到当前点的距离）
    double radius = mStartPoint.distance(currentPoint);
    
    // 创建圆形几何
    QgsCircle circle(QgsPoint(mStartPoint), radius);
    // 获取圆心和半径
    QgsPointXY center = circle.center();

    // 创建多边形点序列
    int segments = 36;

    QgsPolylineXY ring;
    ring.reserve(segments + 1); // 预留空间
    // 计算圆周上的点（使用三角函数）
    for (int i = 0; i < segments; ++i)
    {
        double angle = 2 * M_PI * i / segments;
        double x = center.x() + radius * cos(angle);
        double y = center.y() + radius * sin(angle);
        ring.append(QgsPoint(x, y));
    }

    // 闭合多边形（最后一点与第一点相同）
    ring.append(ring.first());

    // 创建多边形几何
    QgsPolygonXY polygon;
    polygon << ring;
    mSelectionGeometry = QgsGeometry::fromPolygonXY(polygon);
    mRubberBand->setToGeometry(mSelectionGeometry, nullptr);
}
    