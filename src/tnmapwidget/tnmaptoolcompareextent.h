#ifndef TNMAPTOOLCOMPAREEXTENT_H
#define TNMAPTOOLCOMPAREEXTENT_H

#include "qgsmaptool.h"
#include "qgspointxy.h"
#include "qgis_gui.h"

#include <memory>

class QKeyEvent;
class QgsMapMouseEvent;
class QgsRubberBand;

/**
 * \class TnMapToolCompareExtent
 * \brief 方案对比专用四至框选工具，仅输出 GeoJSON 范围，不选择要素
 */
class TnMapToolCompareExtent : public QgsMapTool
{
    Q_OBJECT

public:
    explicit TnMapToolCompareExtent(QgsMapCanvas* canvas);
    ~TnMapToolCompareExtent() override;

    void activate() override;
    void deactivate() override;

    void canvasPressEvent(QgsMapMouseEvent* e) override;
    void canvasReleaseEvent(QgsMapMouseEvent* e) override;
    void canvasMoveEvent(QgsMapMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

signals:
    void extentSelected(const QString& geoJson);
    void extentPickCancelled();
    void extentTooSmall();

private:
    void initRubberBand();
    void updateRectangleGeometry(const QgsPointXY& currentPoint);
    void cancelPick();
    bool isExtentLargeEnough(const QgsRectangle& rect) const;

    std::unique_ptr<QgsRubberBand> mRubberBand;
    bool mSelecting = false;
    QgsPointXY mStartPoint;
};

#endif // TNMAPTOOLCOMPAREEXTENT_H
