#include "tnfeatureedittool.h"
#include "tnfeatureattributedialog.h"
#include "tnmapconfirmdialog.h"
#include "qgsmapcanvas.h"
#include "qgsvectorlayer.h"
#include "qgsfeaturepickerwidget.h"
#include "qgsmessagebar.h"
#include "tnmapwidget.h"

#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsmapmouseevent.h>
#include <qgsrubberband.h>

#include <QDialog>
#include <QKeyEvent>
#include <algorithm>

namespace {

constexpr int kPixelDragThreshold = 5;

} // namespace

TNFeatureEditTool::TNFeatureEditTool(QgsMapCanvas* canvas)
    : QgsMapToolEdit(canvas)
{
    mRubberBand = new QgsRubberBand(mCanvas, Qgis::GeometryType::Polygon);
    mRubberBand->setFillColor(QColor(255, 220, 64, 40));
    mRubberBand->setStrokeColor(QColor(255, 80, 80, 220));
    mRubberBand->setWidth(2);
}

TNFeatureEditTool::~TNFeatureEditTool() = default;

void TNFeatureEditTool::activate()
{
    mDragActive = false;
    if (mRubberBand) {
        mRubberBand->reset();
    }
    QgsMapToolEdit::activate();
}

void TNFeatureEditTool::deactivate()
{
    mDragActive = false;
    if (mRubberBand) {
        mRubberBand->reset();
    }
    QgsMapToolEdit::deactivate();
}

void TNFeatureEditTool::canvasPressEvent(QgsMapMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QgsMapTool::canvasPressEvent(e);
        return;
    }
    mDragActive = true;
    mPressPixel = e->pos();
    mPressMapPoint = e->mapPoint();
    if (mRubberBand) {
        mRubberBand->reset(Qgis::GeometryType::Polygon);
        updateRubberBand(mPressMapPoint, mPressMapPoint);
    }
}

void TNFeatureEditTool::canvasMoveEvent(QgsMapMouseEvent* e)
{
    if (!mDragActive || !mRubberBand) {
        QgsMapTool::canvasMoveEvent(e);
        return;
    }
    updateRubberBand(mPressMapPoint, e->mapPoint());
}

void TNFeatureEditTool::canvasReleaseEvent(QgsMapMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QgsMapTool::canvasReleaseEvent(e);
        return;
    }

    if (!mDragActive) {
        QgsMapTool::canvasReleaseEvent(e);
        return;
    }

    mDragActive = false;
    if (mRubberBand) {
        mRubberBand->reset();
    }

    QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(mCanvas->currentLayer());
    if (!vl || !vl->isValid() || !vl->isEditable()) {
        QgsMapTool::canvasReleaseEvent(e);
        return;
    }

    const int dragDist = (e->pos() - mPressPixel).manhattanLength();
    QgsFeatureIds fids;
    if (dragDist < kPixelDragThreshold) {
        fids = identifyFeaturesAtPoint(e->mapPoint());
    }
    else {
        const QgsPointXY endMap = e->mapPoint();
        const double x1 = std::min(mPressMapPoint.x(), endMap.x());
        const double y1 = std::min(mPressMapPoint.y(), endMap.y());
        const double x2 = std::max(mPressMapPoint.x(), endMap.x());
        const double y2 = std::max(mPressMapPoint.y(), endMap.y());
        fids = identifyFeaturesInMapRect(QgsRectangle(x1, y1, x2, y2), vl);
    }

    if (mEnableDelete)
    {
        if (!fids.isEmpty())
        {
            vl->selectByIds(fids, Qgis::SelectBehavior::SetSelection);
            if (deleteSelectedFeatures(vl, fids))
            {
                for (QgsFeatureId fid : fids)
                {
                    emit featureDeleted(vl, fid);
                }
                mCanvas->refresh();
            }
        }
    }
    else
    {
        if (!fids.isEmpty())
        {
            emit featureSelected(vl, fids);

            QList<QgsFeatureId> sorted = fids.values();
            std::sort(sorted.begin(), sorted.end());
            const QgsFeatureId fid = sorted.first();

            vl->selectByIds({fid}, Qgis::SelectBehavior::SetSelection);
            QgsFeature feature = vl->getFeature(fid);
            if (feature.isValid())
            {
                TNFeatureAttributeDialog dialog(vl, feature, nullptr);
                dialog.adjustSize();
                const int tWidth = dialog.width();
                const int tHeight = dialog.height();
                const QPoint tPoint = mCanvas->mapToGlobal(QPoint(mCanvas->width(), mCanvas->height()));
                dialog.setGeometry(tPoint.x() - tWidth, tPoint.y() - (mCanvas->height() + tHeight) / 2, tWidth,
                                   tHeight);
                if (dialog.exec() == QDialog::Accepted)
                {
                    vl->changeAttributeValues(fid, dialog.editedAttributes());
                    emit featureEdited(vl, fid);
                    mCanvas->refresh();
                }
            }
        }
    }

    QgsMapTool::canvasReleaseEvent(e);
}

void TNFeatureEditTool::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape && mDragActive)
    {
        mDragActive = false;
        if (mRubberBand) {
            mRubberBand->reset();
        }
        return;
    }
    QgsMapTool::keyPressEvent(e);
}

void TNFeatureEditTool::updateRubberBand(const QgsPointXY& a, const QgsPointXY& b)
{
    const double x1 = std::min(a.x(), b.x());
    const double y1 = std::min(a.y(), b.y());
    const double x2 = std::max(a.x(), b.x());
    const double y2 = std::max(a.y(), b.y());

    QgsPolygonXY polygon;
    QgsPolylineXY ring;
    ring << QgsPointXY(x1, y1) << QgsPointXY(x2, y1) << QgsPointXY(x2, y2) << QgsPointXY(x1, y2) << QgsPointXY(x1, y1);
    polygon << ring;
    const QgsGeometry geom = QgsGeometry::fromPolygonXY(polygon);
    mRubberBand->setToGeometry(geom, nullptr);
}

QgsFeatureIds TNFeatureEditTool::identifyFeaturesAtPoint(const QgsPointXY& mapPoint)
{
    QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(mCanvas->currentLayer());
    if (!vl || !vl->isValid()) {
        return QgsFeatureIds();
    }

    const int tolerance = 5;
    const double searchRadius = mCanvas->mapUnitsPerPixel() * tolerance;

    QgsFeatureRequest request;
    QgsRectangle rect(mapPoint.x() - searchRadius,
                      mapPoint.y() - searchRadius,
                      mapPoint.x() + searchRadius,
                      mapPoint.y() + searchRadius);
    QgsPointXY layerRadius(searchRadius, searchRadius);
    layerRadius = TNMapWidget::tranformMap2Layer(layerRadius, vl);
    const QgsPointXY layerPoint = TNMapWidget::tranformMap2Layer(mapPoint, vl);
    const QgsRectangle layerBound = TNMapWidget::tranformMap2Layer(rect, vl);
    request.setFilterRect(layerBound);

    QgsFeatureIterator it = vl->getFeatures(request);
    QgsFeature feature;
    QgsFeatureIds fids;

    while (it.nextFeature(feature))
    {
        if (feature.geometry().distance(QgsGeometry::fromPointXY(layerPoint)) < layerRadius.x())
        {
            fids << feature.id();
        }
    }

    if (!fids.isEmpty())
    {
        mCanvas->flashFeatureIds(vl, fids);
    }

    return fids;
}

QgsFeatureIds TNFeatureEditTool::identifyFeaturesInMapRect(const QgsRectangle& mapRect, QgsVectorLayer* vl)
{
    if (!vl || !vl->isValid() || mapRect.isEmpty()) {
        return QgsFeatureIds();
    }

    const QgsGeometry mapGeom = QgsGeometry::fromRect(mapRect);
    const QgsRectangle layerBound = TNMapWidget::tranformMap2Layer(mapGeom.boundingBox(), vl);
    const QgsGeometry layerRectGeom = QgsGeometry::fromWkt(layerBound.asWktPolygon());

    QgsFeatureRequest request;
    request.setFilterRect(layerBound);

    QgsFeatureIds fids;
    QgsFeature f;
    QgsFeatureIterator it = vl->getFeatures(request);
    while (it.nextFeature(f))
    {
        if (!f.hasGeometry()) {
            continue;
        }
        if (layerRectGeom.intersects(f.geometry()))
        {
            fids.insert(f.id());
        }
    }

    if (!fids.isEmpty())
    {
        mCanvas->flashFeatureIds(vl, fids);
    }

    return fids;
}

bool TNFeatureEditTool::deleteSelectedFeatures(QgsVectorLayer* layer, const QgsFeatureIds& fids)
{
    if (!layer || !layer->isValid() || fids.isEmpty()) {
        return false;
    }

    const QString msg = QString::fromUtf8(u8"确定删除选中的 %1 个要素？").arg(fids.count());
    if (!TnMapUi::confirmYesNo(QString::fromUtf8(u8"删除确认"), msg))
    {
        return false;
    }

    return layer->deleteFeatures(fids);
}
