#ifndef TNMAPWIDGET_H
#define TNMAPWIDGET_H

#include <QFrame>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPointer>
#include <QMetaType>
#include <QVariant>
#include <QVariantMap>
#include <QWidget>
#include <QStringList>
#include "qgsmapcanvas.h"
#include "qgsmapsettings.h"
#include "qgsvectorlayer.h"
#include "qgsrasterlayer.h"
#include "qgsmaptoolpan.h"
#include "qgsmaptoolzoom.h"
#include "qgsproject.h"
#include "tnmaptoolselect.h"
#include "tnmaptoolcompareextent.h"
#include "tnfeatureaddtool.h"
#include <functional>
#include <memory>

class QgsFeature;

struct TnpXyzConnection
{
    // required
    QString url;
    QString name;
    int zMin = -1;
    int zMax = -1;
    // Authentication configuration id
    QString authCfg;
    // HTTP Basic username
    QString username;
    // HTTP Basic password
    QString password;
    // http headers, default is empty
    QString referer = "";
    // tile pixel ratio (0 = unknown (not scaled), 1.0 = 256x256, 2.0 = 512x512)
    double tilePixelRatio = 0;
    // Interpretation key related to the converter from color to value, empty if none
    QString interpretation;
};

struct MapStatus
{
    QString crsText;
    QString coordText;
    QString scaleText;

    bool operator==(const MapStatus& other) const
    {
        return crsText == other.crsText
            && coordText == other.coordText
            && scaleText == other.scaleText;
    }

    bool operator!=(const MapStatus& other) const
    {
        return !(*this == other);
    }
};
Q_DECLARE_METATYPE(MapStatus)

using FeatureAttributePrefillFn = std::function<void(QgsVectorLayer* layer, QgsFeature& feature, QStringList* readOnlyFieldNames)>;

// 前向声明
class TNFeatureEditTool;
class TNFeatureAddTool;
class TNSelectionSetDialog;
class TNGisEditStackManager;
class QgsAdvancedDigitizingDockWidget;
class QToolBar;
class QHBoxLayout;
class QgsStatusBar;
class QLabel;
class QEvent;
class TNMapWidget : public QFrame
{
    Q_OBJECT

public:
    enum SelectMode
    {
        PointSelect,
        RectangleSelect,
        CircleSelect,
        PolygonSelect
    };

    explicit TNMapWidget(QWidget *parent = nullptr);
    ~TNMapWidget() override;
    // 清空地图
    void clearMap(bool bKeepBasemap = true);

    // 添加图层接口
    void addLayer(QgsVectorLayer *layer);
    void addLayer(QgsRasterLayer *layer);

    // 移除图层接口
    bool removeLayer(QgsMapLayer* layer);
    bool removeLayer(const QString& layerUri);

    /** @brief 移除带指定 customProperty 的图层（用于工作台统一卸载 WFS 业务层） */
    void removeLayersWithCustomProperty(const QString& key, const QVariant& value, bool refreshCanvas = true);

    /** @brief 批量增删图层期间合并 canvas 刷新（可嵌套） */
    void beginLayerBatchUpdate();
    void endLayerBatchUpdate(bool refreshCanvas = true);

    /** @brief 下一次 endLayerBatchUpdate 的 canvas 首刷完成后回调（一次性） */
    void armAfterFirstCanvasRefresh(const std::function<void()>& onComplete);

    /** @brief setDataSource 后同步 mUriLayers 键（按图层指针匹配，保留已有查找键） */
    void syncLayerUriKey(QgsMapLayer* layer);

    /**
     * @brief 冻结画布后更换矢量层数据源，避免渲染/定时器重入时拆毁 WFS Provider
     * @param layer 目标图层
     * @param newUri 新 dataSource URI（通常为去掉 filter 后的规范 WFS URI）
     * @return 换源后图层仍有效则为 true
     */
    bool reloadVectorLayerDataSourceSafely(QgsVectorLayer* layer, const QString& newUri);

    /** @brief 该矢量层是否正处于 reloadVectorLayerDataSourceSafely（含 waitWhileRendering / setDataSource） */
    static bool isVectorLayerReloadInProgress(const QgsVectorLayer* layer);

    /** @brief 将已创建的 valid 矢量层加入地图（canvas 渲染）；logicalUriKey 非空时用作 mUriLayers 键（便于与规范 WFS URI 对齐） */
    bool addExistingVectorLayer(QgsVectorLayer* layer, int level = 0, const QString& logicalUriKey = QString());

    /** @brief WFS 无 geometry 层：仅属性表，不加入 canvas（logicalUriKey 通常为 WFS URI） */
    bool addAttributeOnlyLayer(QgsVectorLayer* layer, const QString& logicalUriKey, int level = 0);

    /** @brief customProperty(tnp_attribute_only) 为 true 的业务层 */
    static bool isAttributeOnlyMapLayer(const QgsMapLayer* layer);

    /** @brief 设置本地图层样式持久化上下文；基础数据 caseId 为 0，方案图为真实方案 id */
    void setStyleContext(int projectId, int caseId);
    int styleProjectId() const { return m_styleProjectId; }
    int styleCaseId() const { return m_styleCaseId; }

    // 通过URL添加图层
    QgsVectorLayer* addVectorLayer(const QString &url, const QString &name = QString(), int level = 0, const QString &provider = "ogr");
    QgsRasterLayer* addRasterLayer(const QString &url, const QString &name = QString(), int level = 0, const QString &provider = "gdal");
    // 添加数据库图层
    QgsVectorLayer* addPostGISLayer(const QString& host, int port, const QString& dbName, const QString& schema, const QString& table,
        const QString& userName, const QString& password, const QString& layerName = QString(), int level = 0, const QString& geometryColumn = QString());
    // 创建xyz瓦片图层
    static QgsRasterLayer* createXyzLayer(const TnpXyzConnection& xyzConnection, int level = 0);

    // 设置图层可见性
    void setLayerVisible(QgsMapLayer *layer, bool visible);
    
    // 设置当前图层（只显示该图层）
    void setCurrentLayer(QgsMapLayer *layer);

    // 设置为浏览模式
    void setPanable();
    
    // 启用要素编辑
    void enableEditFeature(bool enable);
    
    // 启用要素添加
    void enableAddFeature(bool enable);

    /** @brief 添加要素时、属性弹窗前的外部预填回调 */
    void setFeatureAttributePrefillHandler(FeatureAttributePrefillFn handler);
    void clearFeatureAttributePrefillHandler();

    /** @brief 添加要素期间锁定当前编辑图层（自动加载辅助层后不切换） */
    void setAddFeatureTargetLayer(QgsMapLayer* layer);
    void restoreAddFeatureTargetLayer();
    
    // 设置选择模式
    void setSelectMode(SelectMode mode);

    /** @brief 当前地图工具为选择工具且正在框选绘制中 */
    bool isSelectToolSelecting() const;
    
    // 设置是否允许鼠标缩放
    void setZoomable(bool zoomable);
    
    // 放大
    void zoomIn();
    
    // 缩小
    void zoomOut();
    
    // 启用要素删除
    void enableDeleteFeature(bool enable);
    
    // 显示符号编辑器
    void showSymbolEditor();
    
    // 显示标注编辑器
    void showLabelEditor();
    
    // 启用当前图层的标注
    void enableLabel(bool enable);
    
    // 设置被选择的要素
    void setSelection(const QgsFeatureIds &ids);
    
    // 获取当前图层
    QgsMapLayer* currentLayer() const { return mCurrentLayer.data(); }

    // 获取地图画布
    QgsMapCanvas* canvas() const;

    // 设置地图控件自带按钮显隐
    void setMapButtonVisible(bool bVisible = true);

    MapStatus currentMapStatus() const;
    void setBottomLeftOverlayWidget(QWidget* widget, int margin = 10);
    /** @brief 图例等左下角 overlay 显隐或尺寸变化后重算位置 */
    void refreshBottomLeftOverlay();
    /** @brief 抓取左下角 overlay（图例）为图像；无可见 overlay 时返回空图像 */
    QImage grabBottomLeftOverlay();
    /** @brief 左下角 overlay 控件（图例），无则返回空指针 */
    QWidget* bottomLeftOverlayWidget() const;

    //更新图层元素符号化颜色，layer为空则更新全部图层
    bool updateSymbolColor(QgsVectorLayer* layer);

    // 通过全量uri选择layer
    QgsMapLayer* selectLayerByUri(const QString& layerUri);
    // 当前选中的图层
    QgsMapLayer* selectedLayer() const { return mSelectLayer.data(); }

    // 保存编辑（当前图层，兼容内置工具栏）
    bool saveEdits();

    /** @brief 保存所有处于编辑态且有未提交修改的图层 */
    bool saveAllEdits();

    /** @brief 确保当前图层进入 QGIS 编辑模式（不 rollBack 其他图层） */
    bool ensureCurrentLayerEditable();

    /** @brief 仅切换 MapTool，不丢弃 editBuffer 中的修改 */
    void deactivateEditTools();

    /** @brief 图层 GIS 编辑栈（多图层未保存修改追踪、地图级 undo/redo） */
    TNGisEditStackManager* editStackManager() const { return m_editStack; }

    // 根据表名获取图层对象
    QgsMapLayer* getLayer(const QString& layerUri) const;

    // 设置工具栏
    void setToolBar(QToolBar* toolBar);
    void setToolBar(QWidget* toolBar);

    // 获取图层在当前地图中的四至范围
    QgsRectangle getLayerExtentInMapCrs(QgsMapLayer* layer);

    /** @brief 合并多个有效图层范围并缩放地图（行为对齐 QGIS「缩放至图层」） */
    void zoomToMergedLayerExtent(const QList<QgsMapLayer*>& layers);

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    /** @brief 缩放地图至指定图层范围（画布 CRS），行为对齐 QGIS「缩放至图层」 */
    void zoomToLayer(QgsMapLayer* layer);
#endif

    // 坐标转换接口，将图层中的几何转换为地图坐标系几何
    template<typename QgsDT>
    static QgsDT transformLayer2Map(const QgsDT& dt, QgsMapLayer* layer);
    // 坐标转换接口，将地图中的几何转换为图层中的几何
    template<typename QgsDT>
    static QgsDT tranformMap2Layer(const QgsDT& dt, QgsMapLayer* layer);

    // 切换底图显隐
    void switchBaseMap(bool bOpen);

    /** @brief 构建导出用图层栈（与 updateCanvasLayers 叠置规则一致） */
    QList<QgsMapLayer*> exportLayerStack(bool includeBasemap = true);

    /** @brief 构建导出用 QgsMapSettings（extent 等由调用方设置） */
    QgsMapSettings exportMapSettings(bool includeBasemap = true);

    enum class SpatialToolType
    {
        None = 0,               // 未启用
        PointSelect,            // 点选工具
        RectSelect              // 框选工具
    };
    /**
     * 激活定制选择工具.
     * 
     * \param type 工具类型
     * \param activate true,激活；false，取消激活，复原之前的工具状态
     */
    void activateSpatialSelectTool(SpatialToolType type, bool activate);
    SpatialToolType activatedSpatialToolType() const { return mSpatialType; }

    /**
     * 激活/取消方案对比四至框选工具.
     *
     * \param activate true 激活并记忆当前工具；false 恢复先前工具
     */
    void activateCompareExtentTool(bool activate);
    TnMapToolCompareExtent* compareExtentTool() const { return mCompareExtentTool; }

    /**
     * 显示提示信息到状态栏.
     * 
     * \param message
     * \param timeout 信息超时时间，超时后会自动清除
     */
    void showMessage(const QString& message, int timeout = 0);

    /**
     * 为矢量图层设置默认符号.
     * 
     * \param layer
     */
    static void setDefaultRendererForVectorLayer(QgsVectorLayer* layer);
signals:
    // 要素编辑完成信号
    void featureEdited(QgsVectorLayer* layer, QgsFeatureId fid);
    // 要素删除信号
    void featureDeleted(QgsVectorLayer* layer, QgsFeatureId fid);
    // 要素添加信号
    void featureAdded(QgsVectorLayer* layer, QgsFeatureId fid);
    // 要素选择更新信号
    void selectionChanged(const QgsVectorLayer* layer, const QgsFeatureIds& selected, const QgsFeatureIds& deselected);
    void mapStatusChanged(const MapStatus& mapStatus);
    void currentLayerChanged(QgsMapLayer* layer);
    /** @brief 图层移除导致添加/编辑/删除交互被强制回退到浏览态 */
    void editInteractionCleared();
public slots:
    void onUpdateSymbolColor();
protected:
    void showEvent(QShowEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
private slots:
    void onPanClicked();
    void onEditClicked();
    void onSaveClicked();
    void onFeatureSelected(QgsVectorLayer* layer, const QgsFeatureIds& fids);
    void onFeatureEdited(QgsVectorLayer* layer, QgsFeatureId fid);
    void onBasemapClicked(bool checked);
    void onUpdateScale(double scale);
    void onCoordinatesXYChanged(const QgsPointXY& p);
    void onSnapPointDetected(TNFeatureAddTool::SnapType type, const QString& typeName);
private:
    void setupMapTools();
    void initToolBar();
    void initStatusBar();
    void updateCanvasLayers();
    QString getGeometryColumnName(const QString& dbName, const QString& tableName,
        const QString& host, const QString& port,
        const QString& user, const QString& password,
        const QString& schema, QVariantMap& fieldAliases);
    bool isLayerExists(QgsMapLayer* targetLayer);
    //更新指定图层元素符号化颜色
    bool updateLayerSymbolColor(QgsVectorLayer* layer);
    void refreshMapStatus(bool forceEmit = false);
    void updateBottomLeftOverlayPosition();
    void notifyCurrentLayerChanged(QgsMapLayer* layer);
    /** @brief 添加要素模式下切换当前层后重建添加工具（不清目标层） */
    void rebindAddFeatureTool();
    /** @brief 从 AppSession 基础配置的 mapurl2/mapurl3 创建底图图层（仅当当前无底图层时） */
    bool ensureBasemapLayersFromSession();
    /** @brief 空白画布时写入三亚初始视野；已有有效范围时不覆盖 */
    void applyDefaultBasemapExtent();
    /** @brief 画布是否已有有限且非空的范围 */
    bool canvasExtentIsSet() const;
    /** @brief 非批量加图层时，仅在图层范围有效时改画布范围 */
    void applyAddedLayerExtent(QgsMapLayer* layer);
    /** @brief 合并有效图层范围并缩放；成功返回 true */
    bool zoomToLayersIfExtentValid(const QList<QgsMapLayer*>& layers);
    /** @brief 取消等待首帧后补缩放 */
    void cancelDeferredLayerExtentZoom();
    /** @brief 仍停在默认视野时，在下一次画布刷新后补一次缩放 */
    void scheduleDeferredLayerExtentZoom(const QList<QgsMapLayer*>& layers);
    /** @brief 首帧刷新后合并当时已有效的图层范围并缩放一次 */
    void onDeferredExtentFirstRefresh();
    /** @brief 用户拖拽或滚轮后放弃默认视野与待补缩放 */
    void noteUserAdjustedExtent();
    /** @brief 移除图层前替换 RuleBased 客户端过滤渲染器，避免析构崩溃 */
    void prepareVectorLayerForRemoval(QgsVectorLayer* layer);
    /** @brief 若添加/编辑/删除以该层为工作目标，则回退到浏览态 */
    void releaseMapToolsForRemovedLayer(QgsMapLayer* layer);
    bool finalizeMapLayerRemoval(QgsMapLayer* layer);

    QgsMapCanvas *mCanvas = nullptr;
    QList<QgsMapLayer*> mLayers;    
    QMap<QString, QgsMapLayer*> mUriLayers;
    QPointer<QgsMapLayer> mCurrentLayer;
    QPointer<QgsMapLayer> mSelectLayer;

    QList<QgsMapLayer*> mBaseMapLayers;

    // 工具栏
    QToolBar* mDefaultToolBar = nullptr;
    QAction* mBrowseAct = nullptr;
    QAction* mEditAct = nullptr;
    QAction* mAddAct = nullptr;
    QAction* mLastCheckAct = nullptr;

    QAction* mBaseMapAct = nullptr;

    // 地图工具
    QgsMapToolPan *mPanTool = nullptr;
    QgsMapToolZoom *mZoomInTool = nullptr;
    QgsMapToolZoom *mZoomOutTool = nullptr;
    TnMapToolSelect* mSelectTool = nullptr;
    TnMapToolSelect* mSelectRectTool = nullptr;
    TnMapToolSelect* mSelectCircleTool = nullptr;
    TnMapToolSelect* mSelectPolygonTool = nullptr;
    TnMapToolSelect* mSpatialSelectTool = nullptr;
    TnMapToolSelect* mSpatialRectSelect = nullptr;
    QgsMapTool* mSpatialLastTool = nullptr; // 针对特殊选择工具使用，记录使用前使用的工具，便于回滚操作
    TnMapToolCompareExtent* mCompareExtentTool = nullptr;
    QgsMapTool* mCompareExtentLastTool = nullptr;
    TNFeatureEditTool *mEditTool = nullptr;
    TNFeatureAddTool* mAddTool = nullptr;
    QgsAdvancedDigitizingDockWidget* mAdvancedDock = nullptr;

    // 当前定制工具类别
    SpatialToolType mSpatialType = SpatialToolType::None;

    // 选择集显示弹窗
    TNSelectionSetDialog* mSelectionDialog = nullptr;

    QHBoxLayout* mToolBarLayout = nullptr;

    // 状态栏
    QgsStatusBar* mStatusBar = nullptr;
    QLabel* mCrsLabel = nullptr;
    QLabel* mCoordLabel = nullptr;
    QLabel* mScaleLabel = nullptr;
    MapStatus mMapStatus;

    QPointer<QWidget> mBottomLeftOverlayWidget;
    int mBottomLeftOverlayMargin = 10;

    bool mZoomable = true;
    bool mEnableDelete = false;
    bool mEnableEditLayer = false;
    bool mEnableAddFeature = false;
    SelectMode mCurrentSelectMode = PointSelect;
    
    bool mIsInited = false;
    /** 画布仍停在三亚默认视野，首帧后可补一次缩放 */
    bool m_defaultExtentActive = false;
    QMetaObject::Connection m_deferredExtentRefreshConnection;
    QList<QPointer<QgsMapLayer>> m_deferredExtentLayers;

    /** TEMP：本地测试 SHP 是否已尝试过加载（行政区 + 地铁线路；测试结束后删除相关代码） */
    bool mDevTestShpLoaded = true;

    TNGisEditStackManager* m_editStack = nullptr;

    FeatureAttributePrefillFn mFeatureAttributePrefillHandler;

    QPointer<QgsMapLayer> mAddFeatureTargetLayer;

    int m_layerBatchDepth = 0;
    int m_styleProjectId = -1;
    int m_styleCaseId = -1;
    bool mReloadingDataSource = false;

    std::function<void()> m_firstRefreshOnComplete;
    QMetaObject::Connection m_firstRefreshConnection;
};

template<typename QgsDT>
inline QgsDT TNMapWidget::transformLayer2Map(const QgsDT& dt, QgsMapLayer* layer)
{
    if (!layer)
    {
        return dt;
    }
    // 获取图层自身坐标系和范围
    QgsCoordinateReferenceSystem layerCrs = layer->crs();
    // 如果图层坐标系与地图坐标系一致，直接返回
    auto mapCrs = QgsProject::instance()->crs();
    if (layerCrs == mapCrs) {
        return dt;
    }

    // 创建坐标转换器（图层坐标系 -> 地图坐标系）
    QgsCoordinateTransform transform(
        layerCrs, 
        mapCrs,
        QgsProject::instance() // 项目（用于获取转换上下文）
    );

    // 转换范围（可能抛出异常，建议捕获）
    try {
        return transform.transform(dt);
    }
    catch (const QgsCsException& e) {
        qCritical() << u8"坐标转换失败：" << e.what();
        return dt;
    }
}

template<typename QgsDT>
inline QgsDT TNMapWidget::tranformMap2Layer(const QgsDT& dt, QgsMapLayer* layer)
{
    if (!layer)
    {
        return dt;
    }
    // 获取图层自身坐标系和范围
    QgsCoordinateReferenceSystem layerCrs = layer->crs();
    // 如果图层坐标系与地图坐标系一致，直接返回
    auto mapCrs = QgsProject::instance()->crs();
    if (layerCrs == mapCrs) {
        return dt;
    }

    // 创建坐标转换器（地图坐标系 -> 图层坐标系）
    QgsCoordinateTransform transform(
        mapCrs,
        layerCrs,
        QgsProject::instance() // 项目（用于获取转换上下文）
    );

    // 转换范围（可能抛出异常，建议捕获）
    try {
        return transform.transform(dt);
    }
    catch (const QgsCsException& e) {
        qCritical() << u8"坐标转换失败：" << e.what();
        return dt;
    }
}
#endif // TNMAPWIDGET_H
