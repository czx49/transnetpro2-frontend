#ifndef TNFEATUREADDTOOL_H
#define TNFEATUREADDTOOL_H

#include "qgsmaptooledit.h"
#include "qgsmaptoolcapture.h"
#include "qgsvectorlayer.h"
#include "qgssnappingconfig.h"

#include <functional>
#include <memory>
#include <QPointer>
#include <QStringList>

class QgsGeometry;
class QgsFeature;
class QgsRubberBand;

/**
 * \class TNFeatureAddTool
 * \brief 添加要素工具，支持点、线、面几何图形的绘制和要素创建
 */
class TNFeatureAddTool : public QgsMapToolCapture
{
    Q_OBJECT

public:
    enum SnapType {
        SnapNone,
        SnapPointFeature, // 点要素
        SnapLineStart,    // 线起点
        SnapLineMiddle,   // 线中点
        SnapLineEnd,      // 线终点
        SnapLineVertex    // 线段折点
    };
    Q_ENUM(SnapType)

    explicit TNFeatureAddTool(QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsAdvancedDigitizingDockWidget* cadDockWidget, QgsMapToolCapture::CaptureMode mode);
    ~TNFeatureAddTool() override;

    // 获取当前图层（图层销毁后自动为空）
    QgsVectorLayer* layer() const { return mLayer.data(); }

    // 重写激活/停用
    void activate() override;
    void deactivate() override;

    // 重写鼠标释放事件
    void cadCanvasReleaseEvent(QgsMapMouseEvent *e) override;

    // 重写键盘事件
    void keyPressEvent(QKeyEvent *e) override;

    static QgsMapToolCapture::CaptureMode layerCaptureMode(QgsVectorLayer* layer);

    using AttributePrefillFn = std::function<void(QgsVectorLayer* layer, QgsFeature& feature, QStringList* readOnlyFieldNames)>;
    void setAttributePrefillHandler(AttributePrefillFn handler);

signals:
    // 要素添加信号
    void featureAdded(QgsVectorLayer* layer, QgsFeatureId fid);
    // 捕捉点类型信号
    void snapPointDetected(TNFeatureAddTool::SnapType type, const QString& typeName);

protected:
    // 捕获完成后的处理
    void cadCanvasMoveEvent(QgsMapMouseEvent *e) override;

    // 几何图形捕获完成后的处理
    void geometryCaptured(const QgsGeometry &geometry);

private:
    // 创建要素
    bool createFeature(const QgsGeometry& geometry);

    // 显示属性编辑对话框
    bool showAttributeDialog(QgsFeature& feature, const QStringList& readOnlyFieldNames);

    // 更新捕捉标记
    void updateSnapMarker(const QgsPointXY& point, bool visible);

    QPointer<QgsVectorLayer> mLayer;
    QgsRubberBand* mSnapMarker = nullptr;
    QgsSnappingConfig mPreviousSnappingConfig;
    AttributePrefillFn mAttributePrefillHandler;
};

#endif // TNFEATUREADDTOOL_H