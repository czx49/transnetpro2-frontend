#include "tnmaptoolcompareextent.h"

#include "qgsgeometry.h"
#include "qgsmapcanvas.h"
#include "qgsrubberband.h"

#include <qgsmapmouseevent.h>

#include <QKeyEvent>

namespace {

constexpr double kMinExtentMapUnits = 1e-8;

double minCoord(double a, double b)
{
    return a < b ? a : b;
}

double maxCoord(double a, double b)
{
    return a > b ? a : b;
}

} // namespace

TnMapToolCompareExtent::TnMapToolCompareExtent(QgsMapCanvas* canvas)
    : QgsMapTool(canvas)
{
    initRubberBand();
    setCursor(Qt::CrossCursor);
}

TnMapToolCompareExtent::~TnMapToolCompareExtent()
{
    mRubberBand.release();
}

void TnMapToolCompareExtent::activate()
{
    QgsMapTool::activate();
    if (canvas()) {
        canvas()->setCursor(Qt::CrossCursor);
    }
    mSelecting = false;
    if (mRubberBand) {
        mRubberBand->reset();
    }
}

void TnMapToolCompareExtent::deactivate()
{
    QgsMapTool::deactivate();
    mSelecting = false;
    if (mRubberBand) {
        mRubberBand->reset();
    }
}

void TnMapToolCompareExtent::canvasPressEvent(QgsMapMouseEvent* e)
{
    if (e->button() == Qt::RightButton) {
        cancelPick();
        return;
    }

    if (e->button() != Qt::LeftButton || mSelecting) {
        return;
    }

    mSelecting = true;
    mStartPoint = e->mapPoint();
    if (mRubberBand) {
        mRubberBand->reset(Qgis::GeometryType::Polygon);
        updateRectangleGeometry(mStartPoint);
    }
}

void TnMapToolCompareExtent::canvasReleaseEvent(QgsMapMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || !mSelecting) {
        return;
    }

    mSelecting = false;
    updateRectangleGeometry(e->mapPoint());

    const double x1 = minCoord(mStartPoint.x(), e->mapPoint().x());
    const double y1 = minCoord(mStartPoint.y(), e->mapPoint().y());
    const double x2 = maxCoord(mStartPoint.x(), e->mapPoint().x());
    const double y2 = maxCoord(mStartPoint.y(), e->mapPoint().y());
    const QgsRectangle rect(x1, y1, x2, y2);

    if (mRubberBand) {
        mRubberBand->reset();
    }

    if (!isExtentLargeEnough(rect)) {
        emit extentTooSmall();
        return;
    }

    const QgsGeometry geometry = QgsGeometry::fromRect(rect);
    emit extentSelected(geometry.asJson());
}

void TnMapToolCompareExtent::canvasMoveEvent(QgsMapMouseEvent* e)
{
    if (!mSelecting) {
        return;
    }
    updateRectangleGeometry(e->mapPoint());
}

void TnMapToolCompareExtent::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        cancelPick();
        return;
    }
    QgsMapTool::keyPressEvent(e);
}

void TnMapToolCompareExtent::initRubberBand()
{
    mRubberBand = std::make_unique<QgsRubberBand>(canvas(), Qgis::GeometryType::Polygon);
    mRubberBand->setFillColor(QColor(255, 255, 0, 30));
    mRubberBand->setStrokeColor(QColor(255, 0, 0, 200));
    mRubberBand->setWidth(2);
    mRubberBand->setLineStyle(Qt::DashLine);
}

void TnMapToolCompareExtent::updateRectangleGeometry(const QgsPointXY& currentPoint)
{
    if (!mRubberBand) {
        return;
    }

    const double x1 = minCoord(mStartPoint.x(), currentPoint.x());
    const double y1 = minCoord(mStartPoint.y(), currentPoint.y());
    const double x2 = maxCoord(mStartPoint.x(), currentPoint.x());
    const double y2 = maxCoord(mStartPoint.y(), currentPoint.y());

    QgsPolygonXY polygon;
    QgsPolylineXY lines;
    lines << QgsPointXY(x1, y1) << QgsPointXY(x2, y1) << QgsPointXY(x2, y2) << QgsPointXY(x1, y2) << QgsPointXY(x1, y1);
    polygon << lines;

    mRubberBand->setToGeometry(QgsGeometry::fromPolygonXY(polygon), nullptr);
}

void TnMapToolCompareExtent::cancelPick()
{
    mSelecting = false;
    if (mRubberBand) {
        mRubberBand->reset();
    }
    emit extentPickCancelled();
}

bool TnMapToolCompareExtent::isExtentLargeEnough(const QgsRectangle& rect) const
{
    return rect.width() > kMinExtentMapUnits && rect.height() > kMinExtentMapUnits;
}
