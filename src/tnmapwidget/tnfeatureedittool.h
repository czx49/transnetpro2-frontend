#ifndef TNFEATUREEDITTOOL_H
#define TNFEATUREEDITTOOL_H

#include "qgsmaptooledit.h"
#include "qgsvectorlayer.h"

class QgsRubberBand;

class TNFeatureEditTool : public QgsMapToolEdit
{
    Q_OBJECT

public:
    explicit TNFeatureEditTool(QgsMapCanvas* canvas);
    ~TNFeatureEditTool() override;

    void activate() override;
    void deactivate() override;

    void canvasPressEvent(QgsMapMouseEvent* e) override;
    void canvasMoveEvent(QgsMapMouseEvent* e) override;
    void canvasReleaseEvent(QgsMapMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

    void setEnableDelete(bool enable) { mEnableDelete = enable; }
    bool isEnableDelete() const { return mEnableDelete; }

signals:
    void featureSelected(QgsVectorLayer* layer, const QgsFeatureIds& fids);
    void featureEdited(QgsVectorLayer* layer, QgsFeatureId fid);
    void featureDeleted(QgsVectorLayer* layer, QgsFeatureId fid);

private:
    QgsFeatureIds identifyFeaturesAtPoint(const QgsPointXY& mapPoint);
    QgsFeatureIds identifyFeaturesInMapRect(const QgsRectangle& mapRect, QgsVectorLayer* vl);
    void updateRubberBand(const QgsPointXY& a, const QgsPointXY& b);
    bool deleteSelectedFeatures(QgsVectorLayer* layer, const QgsFeatureIds& fids);

    /** 由 QgsMapCanvas 作为 QObject 父对象托管，勿在析构中 delete，避免与 Qt 析构顺序双重释放 */
    QgsRubberBand* mRubberBand = nullptr;
    bool mDragActive = false;
    QPoint mPressPixel;
    QgsPointXY mPressMapPoint;

    bool mEnableDelete = false;
};

#endif // TNFEATUREEDITTOOL_H
