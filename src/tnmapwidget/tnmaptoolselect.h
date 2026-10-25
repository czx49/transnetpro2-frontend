#ifndef TNMAPTOOLSELECT_H
#define TNMAPTOOLSELECT_H

#include "qgsmaptoolcapturelayergeometry.h"
#include "qgis_gui.h"
#include <memory>
#include <QMap>

class QgsRubberBand;
class QgsVectorLayer;
class QgsMapMouseEvent;

/**
 * \class TnMapToolSelect
 * \brief 自定义要素选择工具，支持多种选择形状
 * 继承自QgsMapToolCaptureLayerGeometry，支持点选、矩形选择、圆形选择、多边形选择
 */
class TnMapToolSelect : public QgsMapToolCaptureLayerGeometry
{
    Q_OBJECT

public:
    //! 选择形状模式
    enum SelectionMode
    {
        Point,          //! 点选
        Rectangle,      //! 矩形选择
        Circle,         //! 圆形选择
        Polygon         //! 多边形选择
    };
    Q_ENUM(SelectionMode)

    //! 选择行为模式
    enum SelectBehavior
    {
        NewSelection,       //! 新建选择集
        AddToSelection,     //! 添加到选择集
        RemoveFromSelection,//! 从选择集中移除
        IntersectSelection  //! 与选择集交集
    };
    Q_ENUM(SelectBehavior)

    /**
     * 构造函数
     * \param canvas 地图画布
     * \param cadDockWidget CAD高级数字化面板
     */
    TnMapToolSelect(QgsMapCanvas *canvas, QgsAdvancedDigitizingDockWidget *cadDockWidget);

    ~TnMapToolSelect() override;

    //! 设置选择形状模式
    void setSelectionMode(SelectionMode mode) { mSelectionMode = mode; }

    //! 点选模式下是否支持拖拽自动转为框选（按下位移小判定点选，否则按框选矩形处理）
    void setPointDragAsRect(bool enabled) { mPointDragAsRect = enabled; }

    //! 获取当前选择形状模式
    SelectionMode selectionMode() const { return mSelectionMode; }

    //! 设置选择行为模式
    void setSelectBehavior(SelectBehavior behavior) { mSelectBehavior = behavior; }

    //! 获取当前选择行为模式
    SelectBehavior selectBehavior() const { return mSelectBehavior; }

    //! 是否正在绘制选择图形（框选进行中）
    bool isSelecting() const { return mSelecting; }

    //! 激活工具
    void activate() override;

    //! 停用工具
    void deactivate() override;

    //! 处理鼠标按下事件
    void canvasPressEvent(QgsMapMouseEvent *e) override;

    //! 处理鼠标释放事件
    void canvasReleaseEvent(QgsMapMouseEvent *e) override;

    //! 处理鼠标移动事件
    void canvasMoveEvent(QgsMapMouseEvent *e) override;

    //! 处理键盘事件
    void keyPressEvent(QKeyEvent *e) override;

signals:
    /**
     * 当选择集发生变化时触发
     * \param selected 新选中的要素ID
     * \param deselected 取消选中的要素ID
     */
    void selectionChanged(const QgsVectorLayer* layer, const QgsFeatureIds &selected, const QgsFeatureIds &deselected);

protected:
    //! 处理点捕获完成（用于点选和多边形选点）
    void layerPointCaptured(const QgsPoint &point) override;

    //! 处理线捕获完成（未使用）
    void layerLineCaptured(const QgsCurve *line) override;

    //! 处理多边形捕获完成（用于多边形选择）
    void layerPolygonCaptured(const QgsCurvePolygon *polygon) override;

private:
    //! 初始化橡胶带
    void initRubberBand();

    //! 以指定地图点为中心、按屏幕像素容差（10 像素）构造矩形并选择要素
    void selectAtTolerance(const QgsPointXY& center);

    //! 根据捕获的几何图形选择要素
    void selectFeatures(const QgsGeometry &geometry);

    //! 清除选择状态
    void clearSelection();

    //! 更新矩形选择区域
    void updateRectangleGeometry(const QgsPointXY &currentPoint);

    //! 更新圆形选择区域
    void updateCircleGeometry(const QgsPointXY &currentPoint);

    //! 当前选择形状模式
    SelectionMode mSelectionMode = Point;

    //! 点选模式下拖拽自动转框选开关
    bool mPointDragAsRect = false;

    //! 按下时的屏幕像素位置，用于判定点选/框选
    QPoint mPressScreenPos;

    //! 当前选择行为模式
    SelectBehavior mSelectBehavior = NewSelection;

    //! 用于显示选择区域的临时橡胶带
    std::unique_ptr<QgsRubberBand> mRubberBand;

    //! 是否正在选择
    bool mSelecting = false;

    //! 选择起始点
    QgsPointXY mStartPoint;

    //! 存储当前选择的几何图形
    QgsGeometry mSelectionGeometry;

    //! 上一次选择的要素ID，用于计算选择变化
    QMap<QgsVectorLayer*, QgsFeatureIds> mPreviousSelectedIds;
};

#endif // TNMAPTOOLSELECT_H
    