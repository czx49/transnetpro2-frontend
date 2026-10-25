#include "tnmapwidget.h"
#include "tnlayerfilterapply.h"
#include "tnfeatureedittool.h"
#include "tngiseditstackmanager.h"
#include "tnselectionsetdialog.h"
#include "layerstylepersistence.h"
#include "layerstylerenderer.h"
#include "qgssymbolselectordialog.h"
#include "qgslabelingwidget.h"
#include "qgssinglesymbolrenderer.h"
#include "qgsadvanceddigitizingdockwidget.h"
#include "qgsmessagebar.h"
#include "qgsmessagelog.h"
#include "qgsapplication.h"
#include "qgslayertree.h"
#include "qgslayertreelayer.h"
#include "qgssymbol.h"
#include <qgsstyle.h>
#include "qgswmsdimensiondialog.h"
#include <qgsrulebasedrenderer.h>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFile>
#include <QFileDialog>
#include "../publicctrls/messagedialog.h"
#include <QTimer>
#include <QSet>
#include <QStringList>
#include <QEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QToolBar>
#include <QActionGroup>
#include <algorithm>
#include <qgssymbol.h>
#include <qgslinesymbol.h>
#include <qgsfillsymbol.h>
#include <qgsmarkersymbol.h>
#include <qgsmarkersymbollayer.h>
#include <qgslinesymbollayer.h>
#include <qgsfillsymbollayer.h>
#include "qgsrenderer.h"
#include "qgsgraduatedsymbolrenderer.h"
#include "qgscategorizedsymbolrenderer.h"
// #include "uiglobalstate.h"  // Phase2 暂不使用 UIGlobalState 单例
// #include "symbcolorssave2db.h"  // Phase2 暂不使用
// #include "caseeditpanel.h"  // Phase2 暂不使用
#include "qgsproviderregistry.h"
#include "qgsstatusbar.h"
#include "qgshuesaturationfilter.h"
#include "qgsbrightnesscontrastfilter.h"
#include "qgsrasterpipe.h"

#include "../httpapis/appsession.h"

#define LEVEL_PROPERTY "levelProperty"

namespace {

const QString kAttributeOnlyLayerProp = QStringLiteral("tnp_attribute_only");
QSet<const QgsVectorLayer*> g_reloadingVectorLayers;

bool isWfsNullGeometryLayer(QgsVectorLayer* layer)
{
    return layer && layer->isValid()
           && layer->providerType().compare(QStringLiteral("WFS"), Qt::CaseInsensitive) == 0
           && layer->geometryType() == Qgis::GeometryType::Null
           && layer->fields().count() > 0;
}

} // namespace

namespace
{
bool IsCrsDisplayable(const QgsCoordinateReferenceSystem& crs)
{
    return !crs.authid().trimmed().isEmpty()
        || !crs.description().trimmed().isEmpty();
}

QString FormatCrsStatusText(const QgsCoordinateReferenceSystem& crs)
{
    const QString authId = crs.authid().trimmed();
    if (!authId.isEmpty()) {
        return QString(u8"坐标系：%1").arg(authId);
    }

    const QString description = crs.description().trimmed();
    if (!description.isEmpty()) {
        return QString(u8"坐标系：%1").arg(description);
    }

    return QString(u8"坐标系：未知");
}

void applyFieldChineseAliases(QgsVectorLayer* layer)
{
    if (!layer || !AppSession::instance().hasFieldChinese()) {
        return;
    }

    const QgsFields fields = layer->fields();
    for (int i = 0; i < fields.count(); ++i) {
        const QgsField& field = fields.at(i);
        const QString existingAlias = field.alias();
        if (!existingAlias.isEmpty() && existingAlias != field.name()) {
            continue;
        }

        const QString cn = AppSession::instance().fieldChineseDisplayName(field.name());
        if (!cn.isEmpty()) {
            layer->setFieldAlias(i, cn);
        }
    }
}
}

bool IsFieldRenderable(const QgsField& field);
// Phase2 暂不使用 FiledColorRule 相关功能
// void UpdateLayerFeatures(QgsVectorLayer* layer, const QString& fieldName, const FiledColorRule& filedColorRule);
//void setDefaultRendererForVectorLayer(QgsVectorLayer* layer);

TNMapWidget::TNMapWidget(QWidget *parent) : QFrame(parent)
{
    // 外层容器无边框，避免与业务面板圆角卡片叠加产生双线/凹陷描边
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);

    // 初始化地图控件
    mCanvas = new QgsMapCanvas(this);
    QgsCoordinateReferenceSystem initialCrs = QgsProject::instance()->crs();
    if (!IsCrsDisplayable(initialCrs)) {
        initialCrs = QgsCoordinateReferenceSystem::fromEpsgId(4326);
    }
    mCanvas->setDestinationCrs(initialCrs);
    mCanvas->setCanvasColor(Qt::white);
    mCanvas->enableAntiAliasing(true);
    mCanvas->setWheelFactor(1.2);
    // 对齐 QGIS Desktop 主画布默认，避免缩放/平移重绘闪白
    mCanvas->setCachingEnabled(true);
    mCanvas->setParallelRenderingEnabled(true);
    mCanvas->setPreviewJobsEnabled(true);
    mCanvas->setMapUpdateInterval(250);
    mCanvas->installEventFilter(this);

    // 初始化地图工具
    setupMapTools();

    // 布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // 初始化工具栏
    initToolBar();
    mToolBarLayout = new QHBoxLayout;
    mToolBarLayout->setMargin(0);
    mToolBarLayout->setSpacing(0);
    mToolBarLayout->addWidget(mDefaultToolBar);
    mainLayout->addLayout(mToolBarLayout);

    mainLayout->addWidget(mCanvas);

    m_editStack = new TNGisEditStackManager(this);
    m_editStack->attachMap(this);

    // 状态栏
    initStatusBar();
    mainLayout->addWidget(mStatusBar);
    refreshMapStatus(true);

    // 连接信号
    connect(mEditTool, &TNFeatureEditTool::featureSelected, 
            this, &TNMapWidget::onFeatureSelected);
    connect(mEditTool, &TNFeatureEditTool::featureEdited,
            this, &TNMapWidget::onFeatureEdited);
    connect(mEditTool, &TNFeatureEditTool::featureDeleted,
            this, &TNMapWidget::featureDeleted);
    connect(mCanvas, &QgsMapCanvas::scaleChanged, this, &TNMapWidget::onUpdateScale);
    connect(mCanvas, &QgsMapCanvas::xyCoordinates, this, &TNMapWidget::onCoordinatesXYChanged);

    connect(&AppSession::instance(), &AppSession::baseConfigChanged, this, [this]() {
        if (!mBaseMapAct || !mBaseMapAct->isChecked()) {
            return;
        }
        if (!mBaseMapLayers.isEmpty()) {
            return;
        }
        if (!ensureBasemapLayersFromSession()) {
            return;
        }
        applyDefaultBasemapExtent();
        mCanvas->stopRendering();
        updateCanvasLayers();
        mCanvas->refresh();
        refreshMapStatus(true);
    });

    connect(&AppSession::instance(), &AppSession::fieldChineseChanged, this, [this]() {
        if (!mCanvas) {
            return;
        }
        mCanvas->stopRendering();
        for (QgsMapLayer* mapLayer : mLayers) {
            if (auto* vectorLayer = qobject_cast<QgsVectorLayer*>(mapLayer)) {
                applyFieldChineseAliases(vectorLayer);
            }
        }
        mCanvas->refresh();
    });

    QTimer::singleShot(100, this, [=]() {
        //addPostGISLayer("127.0.0.1", 5432, "test_pg", "test1", "boua1", "postgres", "123123", "BOUA");
        //addPostGISLayer("211.149.178.166", 5432, "urban", "user_project", "project17_user1_road_point", "uproject", "uproject753159", "project17_user1_road_point", "geometry");
        //addPostGISLayer("211.149.178.166", 5432, "urban", "user_project", "project17_user1_road_point", "uproject", "uproject753159", "project17_user1_road_point");
        });
}

TNMapWidget::~TNMapWidget()
{
    deactivateEditTools();
    if (mCanvas && mPanTool) {
        mCanvas->setMapTool(mPanTool);
    }

    delete mSelectionDialog;
    mSelectionDialog = nullptr;

    if (mCanvas) {
        mCanvas->stopRendering();
    }
    activateSpatialSelectTool(SpatialToolType::RectSelect, false);
    activateCompareExtentTool(false);
    if (m_editStack && mCanvas) {
        m_editStack->discardAll();
    }
    if (m_editStack) {
        m_editStack->attachMap(nullptr);
    }

    cancelDeferredLayerExtentZoom();
    mCurrentLayer.clear();
    mSelectLayer.clear();
    mLayers.clear();
    mUriLayers.clear();
}

void TNMapWidget::clearMap(bool bKeepBaseMap)
{
    if (!mCanvas)
    {
        return;
    }
    mCanvas->stopRendering();
    enableEditFeature(false);
    mCanvas->clearCache();
    mCurrentLayer.clear();
    mSelectLayer.clear();
    notifyCurrentLayerChanged(nullptr);
    QgsProject::instance()->removeMapLayers(mLayers);
    // QgsProject拥有图层所有权，因此不用delete释放
    mLayers.clear();
    mUriLayers.clear();
    updateCanvasLayers();
    refreshMapStatus();

    if (bKeepBaseMap && mBaseMapLayers.size() && mBaseMapAct && mBaseMapAct->isChecked())
    {
        applyDefaultBasemapExtent();
        mCanvas->refresh();
    }
}

void TNMapWidget::setupMapTools()
{
    mPanTool = new QgsMapToolPan(mCanvas);
    mZoomInTool = new QgsMapToolZoom(mCanvas, false);
    mZoomOutTool = new QgsMapToolZoom(mCanvas, true);
    mAdvancedDock = new QgsAdvancedDigitizingDockWidget(mCanvas, this);
    mSelectTool = new TnMapToolSelect(mCanvas, mAdvancedDock);
    mSelectTool->setSelectionMode(TnMapToolSelect::SelectionMode::Point);
    // 要素选择工具：单击点选（10 像素容差）、拖拽框选
    mSelectTool->setPointDragAsRect(true);
    mSelectRectTool = new TnMapToolSelect(mCanvas, mAdvancedDock);
    mSelectRectTool->setSelectionMode(TnMapToolSelect::SelectionMode::Rectangle);
    mSelectCircleTool = new TnMapToolSelect(mCanvas, mAdvancedDock);
    mSelectCircleTool->setSelectionMode(TnMapToolSelect::SelectionMode::Circle);
    mSelectPolygonTool = new TnMapToolSelect(mCanvas, mAdvancedDock);
    mSelectPolygonTool->setSelectionMode(TnMapToolSelect::SelectionMode::Polygon);
    mSpatialSelectTool = new TnMapToolSelect(mCanvas, mAdvancedDock);
    mSpatialSelectTool->setSelectionMode(TnMapToolSelect::SelectionMode::Point);
    mSpatialRectSelect = new TnMapToolSelect(mCanvas, mAdvancedDock);
    mSpatialRectSelect->setSelectionMode(TnMapToolSelect::SelectionMode::Rectangle);
    mCompareExtentTool = new TnMapToolCompareExtent(mCanvas);
    mEditTool = new TNFeatureEditTool(mCanvas);

    // 设置默认为平移工具
    mCanvas->setMapTool(mPanTool);
    mAdvancedDock->close();

    // 选择集显示弹窗：在任一选择工具触发选择变化时更新并显示
    auto onSelectionChanged = [this](const QgsVectorLayer* layer, const QgsFeatureIds& /*selected*/, const QgsFeatureIds& /*deselected*/) {
        if (!layer) return;
        // 创建/复用弹窗
        if (!mSelectionDialog)
        {
            mSelectionDialog = new TNSelectionSetDialog(nullptr);
            mSelectionDialog->setCanvas(mCanvas);
        }
        // 添加或刷新图层项
        mSelectionDialog->addOrRefreshLayer(const_cast<QgsVectorLayer*>(layer), layer->selectedFeatureIds());
        // 设置当前图层
        mSelectionDialog->setCurrentLayer(const_cast<QgsVectorLayer*>(layer));
        // 显示弹窗（一次选择可能触发多次，重复显示不影响）
        mSelectionDialog->show();
        mSelectionDialog->raise();
        mSelectionDialog->activateWindow();
    };

    connect(mSelectTool, &TnMapToolSelect::selectionChanged, this, onSelectionChanged);
    connect(mSelectRectTool, &TnMapToolSelect::selectionChanged, this, onSelectionChanged);
    connect(mSelectCircleTool, &TnMapToolSelect::selectionChanged, this, onSelectionChanged);
    connect(mSelectPolygonTool, &TnMapToolSelect::selectionChanged, this, onSelectionChanged);
    connect(mSpatialSelectTool, &TnMapToolSelect::selectionChanged, this, &TNMapWidget::selectionChanged);
    connect(mSpatialRectSelect, &TnMapToolSelect::selectionChanged, this, &TNMapWidget::selectionChanged);
}

void TNMapWidget::initToolBar()
{
    mDefaultToolBar = new QToolBar(this);
    mDefaultToolBar->setFixedHeight(32);
    mDefaultToolBar->setProperty("styleClass", "mapToolBar");

    QActionGroup* actionGroup = new QActionGroup(mDefaultToolBar);

    mBrowseAct = mDefaultToolBar->addAction("");
    actionGroup->addAction(mBrowseAct);
    mBrowseAct->setToolTip(u8"浏览");
    mDefaultToolBar->widgetForAction(mBrowseAct)->setProperty("styleClass", "BowseAction");
    mBrowseAct->setCheckable(true);
    mBrowseAct->setChecked(true);

    mEditAct = mDefaultToolBar->addAction("");
    actionGroup->addAction(mEditAct);
    mEditAct->setToolTip(u8"要素编辑");
    mDefaultToolBar->widgetForAction(mEditAct)->setProperty("styleClass", "EditAction");
    mEditAct->setCheckable(true);
    
    mAddAct = mDefaultToolBar->addAction("");
    actionGroup->addAction(mAddAct);
    mAddAct->setToolTip(u8"添加要素");
    mDefaultToolBar->widgetForAction(mAddAct)->setProperty("styleClass", "AddFeatureAction");
    mAddAct->setCheckable(true);
    mAddAct->setVisible(false);

    mBaseMapAct = mDefaultToolBar->addAction("", this, &TNMapWidget::onBasemapClicked);
    mBaseMapAct->setToolTip(u8"底图");
    mDefaultToolBar->widgetForAction(mBaseMapAct)->setProperty("styleClass", "BaseMapAction");
    mBaseMapAct->setCheckable(true);
    mBaseMapAct->setChecked(true);

    auto saveAction = mDefaultToolBar->addAction("", this, [this]() {
        onSaveClicked();
        });
    saveAction->setToolTip(u8"保存编辑");
    mDefaultToolBar->widgetForAction(saveAction)->setProperty("styleClass", "SaveAction");

    mLastCheckAct = mBrowseAct;
    connect(actionGroup, &QActionGroup::triggered, this, [=](QAction* action) {
        if (action == mBrowseAct && mLastCheckAct != action)
        {
            onPanClicked();
            mLastCheckAct = action;
        }
        else if(action == mEditAct)
        {
            if (action == mLastCheckAct)
            {
                mBrowseAct->setChecked(true);
                onPanClicked();
                mLastCheckAct = mBrowseAct;
            }
            else
            {
                onEditClicked();
                mLastCheckAct = mEditAct;
            }
        }
        });

    auto openFile = [=]() {
        QString fileName = QFileDialog::getOpenFileName(this,
            u8"打开 GIS 文件", "",
            u8"GIS 文件（*.shp *.geojson *.tif *.gpkg *.dxf *.osm）");

        if (!fileName.isEmpty())
        {
            if (addVectorLayer(fileName))
            {
                QgsMessageBar::createMessage(u8"提示", u8"添加图层成功。");
            }
            else
            {
                QgsMessageBar::createMessage(u8"提示", u8"添加图层失败。");
            }
        }
    };

    connect(mAddAct, &QAction::triggered, this, [=]() {
        enableAddFeature(true);
    });
}

void TNMapWidget::initStatusBar()
{
    mStatusBar = new QgsStatusBar(this);

    // 创建显示控件
    mCoordLabel = new QLabel(u8"坐标：--,--");
    mScaleLabel = new QLabel(u8"比例尺：1:--");
    mCrsLabel = new QLabel(u8"坐标系：EPSG:4326");
    QWidget* statusWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(statusWidget);
    layout->addWidget(mCoordLabel);
    layout->addSpacing(20);
    layout->addWidget(mScaleLabel);
    layout->addSpacing(20);
    layout->addWidget(mCrsLabel);
    layout->setContentsMargins(5, 0, 5, 0);

    mStatusBar->addPermanentWidget(statusWidget);
}

void TNMapWidget::beginLayerBatchUpdate()
{
    if (m_layerBatchDepth++ == 0 && mCanvas) {
        mCanvas->stopRendering();
    }
}

void TNMapWidget::armAfterFirstCanvasRefresh(const std::function<void()>& onComplete)
{
    m_firstRefreshOnComplete = onComplete;
}

void TNMapWidget::endLayerBatchUpdate(bool refreshCanvas)
{
    if (m_layerBatchDepth <= 0) {
        return;
    }
    if (--m_layerBatchDepth == 0) {
        updateCanvasLayers();
        refreshMapStatus();
        if (mCanvas) {
            const std::function<void()> onComplete = m_firstRefreshOnComplete;
            m_firstRefreshOnComplete = {};
            if (refreshCanvas && onComplete) {
                if (m_firstRefreshConnection) {
                    QObject::disconnect(m_firstRefreshConnection);
                    m_firstRefreshConnection = {};
                }
                m_firstRefreshConnection = QObject::connect(
                    mCanvas, &QgsMapCanvas::mapCanvasRefreshed, this,
                    [this, onComplete]() {
                        if (m_firstRefreshConnection) {
                            QObject::disconnect(m_firstRefreshConnection);
                            m_firstRefreshConnection = {};
                        }
                        onComplete();
                    });
            }
            if (refreshCanvas) {
                mCanvas->refresh();
            } else if (onComplete) {
                // 不刷新则永远等不到 mapCanvasRefreshed，就地放行等待首帧的逻辑
                onComplete();
            }
        }
    }
}

bool TNMapWidget::isAttributeOnlyMapLayer(const QgsMapLayer* layer)
{
    return layer && layer->customProperty(kAttributeOnlyLayerProp).toBool();
}

void TNMapWidget::setStyleContext(int projectId, int caseId)
{
    m_styleProjectId = projectId;
    m_styleCaseId = caseId;
}

bool TNMapWidget::addExistingVectorLayer(QgsVectorLayer* layer, int level, const QString& logicalUriKey)
{
    if (!layer || !layer->isValid() || isLayerExists(layer)) {
        return false;
    }
    layer->setProperty(LEVEL_PROPERTY, level);
    addLayer(layer);

    // WFS forceInitialGetFeature 等会使 provider URI 与规范 URI 不同；用 logicalUriKey 保持查找一致。
    const QString key = logicalUriKey.trimmed();
    if (!key.isEmpty() && layer->dataProvider()) {
        const QString providerUri = layer->dataProvider()->dataSourceUri();
        if (key != providerUri) {
            for (auto it = mUriLayers.begin(); it != mUriLayers.end();) {
                if (it.value() == layer && it.key() != key) {
                    it = mUriLayers.erase(it);
                } else {
                    ++it;
                }
            }
            mUriLayers.insert(key, layer);
        }
    }
    return true;
}

bool TNMapWidget::addAttributeOnlyLayer(QgsVectorLayer* layer, const QString& logicalUriKey, int level)
{
    if (!layer || !isWfsNullGeometryLayer(layer)) {
        return false;
    }
    const QString uriKey = logicalUriKey.trimmed();
    if (uriKey.isEmpty() || isLayerExists(layer) || mUriLayers.contains(uriKey)) {
        return false;
    }

    layer->setProperty(LEVEL_PROPERTY, level);
    layer->setCustomProperty(kAttributeOnlyLayerProp, QVariant(true));

    if (m_layerBatchDepth == 0) {
        mCanvas->stopRendering();
    }

    mUriLayers.insert(uriKey, layer);
    mLayers.insert(0, layer);
    QgsProject::instance()->addMapLayer(layer, false);

    applyFieldChineseAliases(layer);

    if (m_layerBatchDepth == 0) {
        updateCanvasLayers();
        refreshMapStatus();
    }

    return true;
}

void TNMapWidget::addLayer(QgsVectorLayer *layer)
{
    if (!layer || !layer->isValid() || isLayerExists(layer))
        return;
    setDefaultRendererForVectorLayer(layer);

    if (m_layerBatchDepth == 0) {
        mCanvas->stopRendering();
    }
    mUriLayers.insert(layer->dataProvider()->dataSourceUri(), layer);
    mLayers.insert(0, layer); // 最前面的在最上面
    QgsProject::instance()->addMapLayer(layer, false);
    const QPointer<QgsVectorLayer> layerGuard(layer);
    connect(layer, &QgsMapLayer::dataSourceChanged, this, [this, layerGuard]() {
        if (layerGuard) {
            syncLayerUriKey(layerGuard.data());
        }
    });
    if (mEnableEditLayer || mEnableAddFeature)
    {
        layer->startEditing();
    }
    if (m_layerBatchDepth == 0) {
        updateCanvasLayers();
    }

    const bool keepTargetCurrent =
        mAddFeatureTargetLayer && layer != mAddFeatureTargetLayer.data();

    if (keepTargetCurrent) {
        restoreAddFeatureTargetLayer();
    } else {
        mCurrentLayer = layer;
        mSelectLayer = layer;
        mCanvas->setCurrentLayer(layer);
        notifyCurrentLayerChanged(layer);
        if (mEnableAddFeature) {
            rebindAddFeatureTool();
        }
    }
    updateLayerSymbolColor(layer);
    applyFieldChineseAliases(layer);

    if (m_layerBatchDepth == 0) {
        if (!keepTargetCurrent) {
            applyAddedLayerExtent(layer);
        }
        refreshMapStatus();
    }
}

void TNMapWidget::addLayer(QgsRasterLayer *layer)
{
    if (!layer || !layer->isValid() || isLayerExists(layer))
        return;

    if (m_layerBatchDepth == 0) {
        mCanvas->stopRendering();
    }
    mUriLayers.insert(layer->dataProvider()->dataSourceUri(), layer);
    mLayers.insert(0, layer); // 最前面的在最上面
    QgsProject::instance()->addMapLayer(layer, false);
    if (m_layerBatchDepth == 0) {
        updateCanvasLayers();
    }

    mCurrentLayer = layer;
    mSelectLayer = layer;
    mCanvas->setCurrentLayer(layer);
    notifyCurrentLayerChanged(layer);

    if (m_layerBatchDepth == 0) {
        applyAddedLayerExtent(layer);
        refreshMapStatus();
    }
}

void TNMapWidget::syncLayerUriKey(QgsMapLayer* layer)
{
    if (!layer || !layer->dataProvider()) {
        return;
    }

    const QString newUri = layer->dataProvider()->dataSourceUri();
    if (!newUri.isEmpty()) {
        mUriLayers.insert(newUri, layer);
    }
}

bool TNMapWidget::isVectorLayerReloadInProgress(const QgsVectorLayer* layer)
{
    return layer && g_reloadingVectorLayers.contains(layer);
}

bool TNMapWidget::reloadVectorLayerDataSourceSafely(QgsVectorLayer* layer, const QString& newUri)
{
    if (!layer || !mCanvas || newUri.trimmed().isEmpty()) {
        return false;
    }
    if (mReloadingDataSource || g_reloadingVectorLayers.contains(layer)) {
        return false;
    }

    QStringList preservedKeys;
    for (auto it = mUriLayers.constBegin(); it != mUriLayers.constEnd(); ++it) {
        if (it.value() == layer) {
            preservedKeys.append(it.key());
        }
    }

    mReloadingDataSource = true;
    g_reloadingVectorLayers.insert(layer);
    struct ReloadingGuard {
        QgsVectorLayer* layer = nullptr;
        bool* flag = nullptr;
        ~ReloadingGuard()
        {
            if (layer) {
                g_reloadingVectorLayers.remove(layer);
            }
            if (flag) {
                *flag = false;
            }
        }
    } reloadingGuard{layer, &mReloadingDataSource};

    mCanvas->freeze(true);
    mCanvas->cancelJobs();
    mCanvas->waitWhileRendering();

    layer->setDataSource(newUri, layer->name(), layer->providerType(), false);

    for (const QString& key : preservedKeys) {
        mUriLayers.insert(key, layer);
    }
    mUriLayers.insert(newUri, layer);
    if (layer->dataProvider()) {
        const QString providerUri = layer->dataProvider()->dataSourceUri();
        if (!providerUri.isEmpty()) {
            mUriLayers.insert(providerUri, layer);
        }
    }

    mCanvas->freeze(false);
    return layer->isValid();
}

void TNMapWidget::prepareVectorLayerForRemoval(QgsVectorLayer* layer)
{
    if (!layer || !mCanvas) {
        return;
    }

    if (m_layerBatchDepth == 0) {
        mCanvas->stopRendering();
        mCanvas->waitWhileRendering();
    }

    const bool needsRestore = TNLayerFilter::layerUsesClientDisplayFilter(layer)
        || layer->customProperty(TNLayerFilter::kRendererBackupPropertyKey).isValid()
        || dynamic_cast<QgsRuleBasedRenderer*>(layer->renderer());

    if (!needsRestore) {
        return;
    }

    TNLayerFilter::restoreBaseRendererAfterClientFilter(layer);
}

void TNMapWidget::releaseMapToolsForRemovedLayer(QgsMapLayer* layer)
{
    if (!layer) {
        return;
    }

    const bool addBoundToLayer =
        (mAddTool && mAddTool->layer() == layer) || (mAddFeatureTargetLayer.data() == layer);

    QgsMapLayer* canvasCurrent = mCanvas ? mCanvas->currentLayer() : nullptr;
    const bool editOrDeleteOnLayer =
        (mEnableEditLayer || mEnableDelete)
        && (mCurrentLayer.data() == layer || canvasCurrent == layer);

    if (!addBoundToLayer && !editOrDeleteOnLayer) {
        return;
    }

    deactivateEditTools();
    mAddFeatureTargetLayer.clear();
    clearFeatureAttributePrefillHandler();
    emit editInteractionCleared();
}

bool TNMapWidget::finalizeMapLayerRemoval(QgsMapLayer* layer)
{
    if (!layer) {
        return false;
    }

    // 必须在改写 mCurrentLayer / 销毁图层之前判断并释放交互工具
    releaseMapToolsForRemovedLayer(layer);

    mLayers.removeAll(layer);
    for (auto it = mUriLayers.begin(); it != mUriLayers.end();) {
        if (it.value() == layer) {
            it = mUriLayers.erase(it);
        } else {
            ++it;
        }
    }
    // 批量删除时交由 endLayerBatchUpdate 统一 setLayers，避免 N 次全量刷新
    if (m_layerBatchDepth == 0) {
        updateCanvasLayers();
    }
    if (mCurrentLayer == layer || mSelectLayer == layer) {
        QgsMapLayer* nextLayer = mLayers.isEmpty() ? nullptr : mLayers.first();
        mCurrentLayer = nextLayer;
        mSelectLayer = nextLayer;
        mCanvas->setCurrentLayer(nextLayer);
        notifyCurrentLayerChanged(nextLayer);
    }

    if (auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer)) {
        if (vectorLayer->isEditable()) {
            vectorLayer->rollBack();
        }
        if (dynamic_cast<QgsRuleBasedRenderer*>(vectorLayer->renderer())) {
            qWarning() << "TNMapWidget: RuleBased renderer could not be restored on" << layer->name()
                       << "- proceeding with layer removal anyway";
        }
        if (m_editStack) {
            m_editStack->forgetLayer(vectorLayer);
        }
    }

    QgsProject::instance()->removeMapLayer(layer);

    if (m_layerBatchDepth == 0) {
        refreshMapStatus();
        mCanvas->refresh();
    }
    return true;
}

void TNMapWidget::removeLayersWithCustomProperty(const QString& key, const QVariant& value, bool refreshCanvas)
{
    if (!mCanvas || key.isEmpty()) {
        return;
    }
    QList<QgsMapLayer*> toRemove;
    for (QgsMapLayer* layer : mLayers) {
        if (!layer) {
            continue;
        }
        if (layer->customProperty(key) == value) {
            toRemove.append(layer);
        }
    }
    if (toRemove.isEmpty()) {
        return;
    }

    // 先阻塞收完 parallel/preview job，再从画布摘层，最后才析构图层。
    // 否则 QFutureWatcher 完成回调会命中已释放的 WFS provider。
    mCanvas->freeze(true);
    mCanvas->cancelJobs();
    mCanvas->waitWhileRendering();

    for (QgsMapLayer* layer : toRemove) {
        if (auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer)) {
            prepareVectorLayerForRemoval(vectorLayer);
        }
        releaseMapToolsForRemovedLayer(layer);
        mLayers.removeAll(layer);
        for (auto it = mUriLayers.begin(); it != mUriLayers.end();) {
            if (it.value() == layer) {
                it = mUriLayers.erase(it);
            } else {
                ++it;
            }
        }
        if (mCurrentLayer == layer || mSelectLayer == layer) {
            QgsMapLayer* nextLayer = mLayers.isEmpty() ? nullptr : mLayers.first();
            mCurrentLayer = nextLayer;
            mSelectLayer = nextLayer;
            mCanvas->setCurrentLayer(nextLayer);
            notifyCurrentLayerChanged(nextLayer);
        }
        if (auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer)) {
            if (vectorLayer->isEditable()) {
                vectorLayer->rollBack();
            }
            if (m_editStack) {
                m_editStack->forgetLayer(vectorLayer);
            }
        }
    }

    updateCanvasLayers();

    for (QgsMapLayer* layer : toRemove) {
        QgsProject::instance()->removeMapLayer(layer);
    }

    if (refreshCanvas) {
        mCanvas->freeze(false);
        refreshMapStatus();
        mCanvas->refresh();
    } else {
        mCanvas->stopRendering();
    }
}

bool TNMapWidget::removeLayer(QgsMapLayer* layer)
{
    if (mLayers.contains(layer))
    {
        if (m_layerBatchDepth == 0) {
            mCanvas->stopRendering();
        }
        if (auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer)) {
            prepareVectorLayerForRemoval(vectorLayer);
        }
        return finalizeMapLayerRemoval(layer);
    }
    return false;
}

bool TNMapWidget::removeLayer(const QString& layerUri)
{
    QgsMapLayer* layer = getLayer(layerUri);
    if (layer)
    {
        if (m_layerBatchDepth == 0) {
            mCanvas->stopRendering();
        }
        if (auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer)) {
            prepareVectorLayerForRemoval(vectorLayer);
        }
        return finalizeMapLayerRemoval(layer);
    }
    return false;
}

QgsVectorLayer* TNMapWidget::addVectorLayer(const QString &url, const QString &name, int level, const QString &provider)
{
    QgsVectorLayer *layer = new QgsVectorLayer(url, name, provider);
    qDebug() << "add Layer url: " << url << ", name: " << name << ", level: " << level << ", provider: " << provider;
    layer->setProperty(LEVEL_PROPERTY, level);
    if (layer->isValid())
    {
        addLayer(layer);
        return layer;
    }
    else
    {
        qDebug() << u8"Qgs图层创建失败！错误：" << layer->lastError();
        delete layer;
        return nullptr;
    }
}

QgsRasterLayer* TNMapWidget::addRasterLayer(const QString &url, const QString &name, int level, const QString &provider)
{
    QgsRasterLayer *layer = new QgsRasterLayer(url, name, provider);
    layer->setProperty(LEVEL_PROPERTY, level);
    if (layer->isValid())
    {
        addLayer(layer);
        return layer;
    }
    else
    {
        delete layer;
        return nullptr;
    }
}

QgsVectorLayer* TNMapWidget::addPostGISLayer(const QString& host, int port, const QString& dbName, const QString& schema
    , const QString& table, const QString& userName, const QString& password, const QString& layerName, int level, const QString& geometryColumn)
{
    // 1.校验地图画布
    if (mCanvas == nullptr) {
        return nullptr;
    }

    QString geomColumn = geometryColumn;
    QVariantMap fieldAliases;
    if (geomColumn.isEmpty())
    {
        geomColumn = getGeometryColumnName(dbName, table, host, QString::number(port), userName, password, schema, fieldAliases);
    }

    if (dbName.isEmpty() || table.isEmpty()) {
        TnUi::showWarning(u8"打开 PostGIS 图层", u8"数据库名、表名不能为空！");
        //m_lastError = "数据库名、表名或几何字段不能为空";
        return nullptr;
    }

    // 2. 构建PostGIS连接信息（使用QgsDataSourceUri更安全）
    QgsDataSourceUri uri;
    uri.setConnection(host, QString::number(port), dbName, userName, password, QgsDataSourceUri::SslDisable);
    uri.setDataSource(schema, table, geomColumn);

    uri.setUseEstimatedMetadata(true);  // 对大数据表加速加载

    // 3. 创建矢量图层（PostGIS使用"postgres"提供器）
    qDebug() << "add PostGIS layer: " << uri.uri();
    //QString url = "dbname='test_pg' host=127.0.0.1 port=5432 user='postgres' password='<YOUR_DATABASE_PASSWORD>'sslmode=disable key='FID' srid=4490 type=MultiPolygonZ checkPrimaryKeyUnicity='1' table=\"test1\".\"boua1\"(geom)";
    //QgsVectorLayer* layer = new QgsVectorLayer(url, /*layerName.isEmpty() ? table : layerName*/table, "postgres");
    QgsVectorLayer* layer = new QgsVectorLayer(uri.uri(), layerName.isEmpty() ? table : layerName, "postgres");
    layer->setProperty(LEVEL_PROPERTY, level);

    // 4. 验证图层有效性
    if (!layer->isValid()) {
        qDebug() << "add PostGIS layer failed: " << layer->error().message();
        TnUi::showWarning(u8"打开 PostGIS 图层", QString(u8"图层无效：%1。").arg(layer->error().message()));
        delete layer;
        return nullptr;
    }

    // 添加别名属性，用于属性表显示
    auto layerFields = layer->fields();
    for (auto fieldName : fieldAliases.keys())
    {
        if (!fieldName.isEmpty() && fieldAliases[fieldName].isValid())
        {
            layer->setFieldAlias(layerFields.indexFromName(fieldName), fieldAliases[fieldName].toString());
        }
    }
    QJsonObject aliasObj;
    aliasObj.fromVariantMap(fieldAliases);
    layer->setProperty("AttributeTableAlias", aliasObj);

    if (layer->geometryType() == Qgis::GeometryType::Line)
    {
        const auto& fields = layer->fields();
        int typeIndex = fields.indexFromName("type");
        if (typeIndex != -1)
        {
            layer->setSubsetString("\"type\" < 10");
        }
    }

    // 5. 添加图层到地图画布
    this->addLayer(layer);

    qDebug() << u8"成功添加 PostGIS 图层：" << layerName;
    return layer;
}

QgsRasterLayer* TNMapWidget::createXyzLayer(const TnpXyzConnection& conn, int level)
{
    if (conn.url.isNull() || conn.url.isEmpty())
    {
        qWarning() << u8"创建 XYZ 图层失败，数据源 URL 为空。";
        return nullptr;
    }
    QVariantMap parts;
    parts.insert("type", "xyz");
    parts.insert("url", conn.url);
    if (conn.zMin >= 0)
    {
        parts.insert("zmin", conn.zMin);
    }
    if (conn.zMax >= 0)
    {
        parts.insert("zmax", conn.zMax);
    }
    if (!conn.referer.isNull())
    {
        parts.insert("referer", conn.referer);
    }
    if (!conn.username.isEmpty())
    {
        parts.insert("username", conn.username);
    }
    if (!conn.password.isEmpty())
    {
        parts.insert("password", conn.password);
    }
    if (!conn.authCfg.isEmpty())
    {
        parts.insert("authcfg", conn.authCfg);
    }
    if (conn.tilePixelRatio > 0)
    {
        parts.insert("tilePixelRatio", conn.tilePixelRatio);
    }
    if (!conn.interpretation.isEmpty())
    {
        parts.insert("interpretation", conn.interpretation);
    }

    QString encodeUri = QgsProviderRegistry::instance()->encodeUri("wms", parts);
    encodeUri = QgsHttpHeaders::PARAM_PREFIX + encodeUri;
    qDebug() << u8"创建 XYZ 图层，编码后的 URL 为：" << encodeUri;

    QgsRasterLayer* layer = new QgsRasterLayer(encodeUri, conn.name.isEmpty() ? "xyz tiles" : conn.name, "wms");
    layer->setProperty(LEVEL_PROPERTY, level);
    if (!layer->isValid()) {
        // qCritical() << "Layer load failed:" << layer->error().message();
        // qCritical() << "Dirver error:" << layer->dataProvider()->error().message();
        delete layer;
        return nullptr;
    }
    if (layer->pipe()) {
        QgsRasterPipe* pipe = layer->pipe();
        if (pipe) {
            // 亮度/对比度 恢复默认
            QgsBrightnessContrastFilter* bc = pipe->brightnessFilter();
            if (!bc) {
                bc = new QgsBrightnessContrastFilter();
                pipe->set(bc);
            }
            bc->setBrightness(25); // 亮度：25
            bc->setContrast(-7);   // 对比度--7
            bc->setGamma(1.0);     // 伽马：1.00

            // 色相 / 饱和度 色彩校正
            QgsHueSaturationFilter* hs = pipe->hueSaturationFilter();
            if (!hs) {
                hs = new QgsHueSaturationFilter();
                pipe->set(hs);
            }
            hs->setSaturation(-7);                         // 饱和度--7
            hs->setColorizeOn(true);                       // 开启色彩化
            hs->setColorizeColor(QColor(255, 255, 255));   // 色彩化颜色：白色
            hs->setColorizeStrength(30);                   // 色彩化浓度：30%
        }
    }
    return layer;
}

void TNMapWidget::setLayerVisible(QgsMapLayer *layer, bool visible)
{
    if (!layer) return;

    layer->setSubLayerVisibility(layer->name(), visible);
    mCanvas->refresh();
}

void TNMapWidget::setCurrentLayer(QgsMapLayer *layer)
{
    if (!layer || !mLayers.contains(layer)) return;

    mCurrentLayer = layer;
    mSelectLayer = layer;

    updateCanvasLayers();
    mCanvas->setCurrentLayer(layer);
    notifyCurrentLayerChanged(layer);

    // 如果添加要素工具处于活动状态，则重新创建它以用于新图层
    if (mEnableAddFeature) {
        rebindAddFeatureTool();
    }
    
    // 更新图层可见性
    //for (QgsMapLayer *l : mLayers)
    //{
    //    l->setSubLayerVisibility(l->name(), l == layer);
    //}
    
    mCanvas->refresh();
    refreshMapStatus();
}

void TNMapWidget::setPanable()
{
    deactivateEditTools();
    mCanvas->setMapTool(mPanTool);
}

bool TNMapWidget::ensureCurrentLayerEditable()
{
    QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(mCanvas->currentLayer());
    if (!vl) {
        return false;
    }
    if (!vl->isEditable()) {
        if (!vl->startEditing()) {
            return false;
        }
    }
    return true;
}

void TNMapWidget::deactivateEditTools()
{
    if (mAddTool) {
        mCanvas->unsetMapTool(mAddTool);
        delete mAddTool;
        mAddTool = nullptr;
    }

    mEnableAddFeature = false;
    mEnableDelete = false;
    mEnableEditLayer = false;

    if (mEditTool) {
        mEditTool->setEnableDelete(false);
    }

    if (mCanvas && mPanTool) {
        mCanvas->setMapTool(mPanTool);
    }

    if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
        mBrowseAct->setChecked(true);
        mEditAct->setChecked(false);
        mAddAct->setChecked(false);
    }
}

void TNMapWidget::enableEditFeature(bool enable)
{
    if (enable) {
        if (!ensureCurrentLayerEditable()) {
            if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
                mEditAct->setChecked(false);
                mBrowseAct->setChecked(true);
            }
            return;
        }

        deactivateEditTools();
        mEnableEditLayer = true;
        mCanvas->setMapTool(mEditTool);

        if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
            mBrowseAct->setChecked(false);
            mEditAct->setChecked(true);
        }
        return;
    }

    deactivateEditTools();
    mCanvas->setMapTool(mPanTool);
}

void TNMapWidget::setFeatureAttributePrefillHandler(FeatureAttributePrefillFn handler)
{
    mFeatureAttributePrefillHandler = std::move(handler);
    if (mAddTool && mFeatureAttributePrefillHandler) {
        mAddTool->setAttributePrefillHandler(mFeatureAttributePrefillHandler);
    }
}

void TNMapWidget::clearFeatureAttributePrefillHandler()
{
    mFeatureAttributePrefillHandler = nullptr;
    if (mAddTool) {
        mAddTool->setAttributePrefillHandler({});
    }
}

void TNMapWidget::setAddFeatureTargetLayer(QgsMapLayer* layer)
{
    mAddFeatureTargetLayer = layer;
}

void TNMapWidget::restoreAddFeatureTargetLayer()
{
    QgsMapLayer* targetLayer = mAddFeatureTargetLayer.data();
    if (!targetLayer || !mLayers.contains(targetLayer)) {
        return;
    }

    if (mCurrentLayer == targetLayer) {
        if (mEnableAddFeature) {
            rebindAddFeatureTool();
        }
        return;
    }

    setCurrentLayer(targetLayer);
}

void TNMapWidget::rebindAddFeatureTool()
{
    if (!mEnableAddFeature || !mCanvas) {
        return;
    }

    QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(mCanvas->currentLayer());
    if (!vl || !vl->isValid()) {
        return;
    }

    if (!vl->isEditable() && !vl->startEditing()) {
        return;
    }

    if (mAddTool) {
        // 旧工具层已失效时仍须先卸下，避免悬空 deactivate；QPointer 层指针可安全失效
        mCanvas->unsetMapTool(mAddTool);
        delete mAddTool;
        mAddTool = nullptr;
    }

    mAddTool = new TNFeatureAddTool(mCanvas, vl, mAdvancedDock, TNFeatureAddTool::layerCaptureMode(vl));
    if (mFeatureAttributePrefillHandler) {
        mAddTool->setAttributePrefillHandler(mFeatureAttributePrefillHandler);
    }
    connect(mAddTool, &TNFeatureAddTool::featureAdded, this, &TNMapWidget::featureAdded);
    connect(mAddTool, &TNFeatureAddTool::snapPointDetected, this, &TNMapWidget::onSnapPointDetected);
    mCanvas->setMapTool(mAddTool);
}

void TNMapWidget::enableAddFeature(bool enable)
{
    if (enable)
    {
        if (!ensureCurrentLayerEditable())
        {
            if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
                mAddAct->setChecked(false);
            }
            return;
        }

        QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(mCanvas->currentLayer());
        if (!vl)
        {
            if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
                mAddAct->setChecked(false);
            }
            return;
        }

        deactivateEditTools();
        mEnableAddFeature = true;

        if (mAddTool)
        {
            mCanvas->unsetMapTool(mAddTool);
            delete mAddTool;
        }

        mAddTool = new TNFeatureAddTool(mCanvas, vl, mAdvancedDock, TNFeatureAddTool::layerCaptureMode(vl));
        if (mFeatureAttributePrefillHandler) {
            mAddTool->setAttributePrefillHandler(mFeatureAttributePrefillHandler);
        }
        connect(mAddTool, &TNFeatureAddTool::featureAdded, this, &TNMapWidget::featureAdded);
        connect(mAddTool, &TNFeatureAddTool::snapPointDetected, this, &TNMapWidget::onSnapPointDetected);
        mCanvas->setMapTool(mAddTool);

        if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
            mBrowseAct->setChecked(false);
            mAddAct->setChecked(true);
        }
    }
    else
    {
        if (mAddTool)
        {
            mCanvas->unsetMapTool(mAddTool);
            delete mAddTool;
            mAddTool = nullptr;
        }

        mEnableAddFeature = false;
        mCanvas->setMapTool(mPanTool);

        if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
            mBrowseAct->setChecked(true);
            mAddAct->setChecked(false);
        }
    }
}

void TNMapWidget::setSelectMode(SelectMode mode)
{
    mCurrentSelectMode = mode;
    
    switch (mode)
    {
        case PointSelect:
            mCanvas->setMapTool(mSelectTool);
            break;
        case RectangleSelect:
            mCanvas->setMapTool(mSelectRectTool);
            break;
        case CircleSelect:
            mCanvas->setMapTool(mSelectCircleTool);
            break;
        case PolygonSelect:
            mCanvas->setMapTool(mSelectPolygonTool);
            break;
    }
}

bool TNMapWidget::isSelectToolSelecting() const
{
    if (!mCanvas) {
        return false;
    }
    auto* selectTool = qobject_cast<TnMapToolSelect*>(mCanvas->mapTool());
    return selectTool && selectTool->isSelecting();
}

void TNMapWidget::setZoomable(bool zoomable)
{
    mZoomable = zoomable;
    mCanvas->setWheelFactor(1.2);
    //mCanvas->setWheelAction(zoomable ? Qgis::WheelAction::Zoom : Qgis::WheelAction::NoAction, 1.2);
}

void TNMapWidget::zoomIn()
{
    mCanvas->zoomIn();
}

void TNMapWidget::zoomOut()
{
    mCanvas->zoomOut();
}

void TNMapWidget::enableDeleteFeature(bool enable)
{
    if (enable) {
        if (!ensureCurrentLayerEditable()) {
            return;
        }

        deactivateEditTools();
        mEnableDelete = true;
        mEnableEditLayer = true;
        mEditTool->setEnableDelete(true);
        mCanvas->setMapTool(mEditTool);

        if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
            mBrowseAct->setChecked(false);
            mEditAct->setChecked(true);
        }
        return;
    }

    mEnableDelete = false;
    if (mEditTool) {
        mEditTool->setEnableDelete(false);
    }
    mEnableEditLayer = false;
    mCanvas->setMapTool(mPanTool);

    if (mDefaultToolBar && mDefaultToolBar->isVisible()) {
        mBrowseAct->setChecked(true);
        mEditAct->setChecked(false);
    }
}

void TNMapWidget::showSymbolEditor()
{
    if (QgsVectorLayer *vl = qobject_cast<QgsVectorLayer*>(mSelectLayer.data()))
    {
        QgsSingleSymbolRenderer *renderer = 
            dynamic_cast<QgsSingleSymbolRenderer*>(vl->renderer());
        
        if (!renderer) return;
        
        std::unique_ptr<QgsSymbol> symbol(renderer->symbol()->clone());
        
        QgsSymbolSelectorDialog dialog(symbol.get(), QgsStyle::defaultStyle(), vl, this);
        dialog.setWindowTitle(u8"符号编辑器");
        
        if (dialog.exec() == QDialog::Accepted)
        {
            renderer->setSymbol(symbol.release());
            vl->triggerRepaint();
        }
    }
}

void TNMapWidget::showLabelEditor()
{
    if (QgsVectorLayer *vl = qobject_cast<QgsVectorLayer*>(mSelectLayer.data()))
    {
        QgsLabelingWidget *labelWidget = new QgsLabelingWidget(vl, mCanvas, this);
        labelWidget->setWindowTitle(u8"标注编辑器");
        labelWidget->show();
    }
}

void TNMapWidget::enableLabel(bool enable)
{
    if (QgsVectorLayer *vl = qobject_cast<QgsVectorLayer*>(mSelectLayer.data()))
    {
        QgsAbstractVectorLayerLabeling* labeling = vl->labeling();
        QgsPalLayerSettings settings;
        
        if (labeling)
        {
            settings = dynamic_cast<QgsVectorLayerSimpleLabeling*>(labeling)->settings();
        }
        
        settings.drawLabels = enable;
        labeling->setSettings(&settings);
        vl->setLabeling(labeling);
        vl->triggerRepaint();
    }
}

void TNMapWidget::setSelection(const QgsFeatureIds &ids)
{
    if (QgsVectorLayer *vl = qobject_cast<QgsVectorLayer*>(mSelectLayer.data()))
    {
        vl->selectByIds(ids);
        mCanvas->flashFeatureIds(vl, ids);
    }
}

QgsMapCanvas* TNMapWidget::canvas() const
{
    return mCanvas;
}

void TNMapWidget::setMapButtonVisible(bool bVisible)
{
    if (!bVisible)
    {
        enableEditFeature(false);
    }

    if (bVisible)
    {
        mDefaultToolBar->show();
    }
    else
    {
        mDefaultToolBar->close();
    }
}

MapStatus TNMapWidget::currentMapStatus() const
{
    return mMapStatus;
}

void TNMapWidget::setBottomLeftOverlayWidget(QWidget* widget, int margin)
{
    if (widget == mBottomLeftOverlayWidget) {
        mBottomLeftOverlayMargin = qMax(0, margin);
        updateBottomLeftOverlayPosition();
        return;
    }

    if (mBottomLeftOverlayWidget) {
        mBottomLeftOverlayWidget->hide();
        mBottomLeftOverlayWidget->deleteLater();
        mBottomLeftOverlayWidget.clear();
    }

    mBottomLeftOverlayMargin = qMax(0, margin);
    if (!widget) {
        return;
    }

    widget->setParent(mCanvas);
    widget->show();
    widget->raise();
    mBottomLeftOverlayWidget = widget;
    updateBottomLeftOverlayPosition();
}

void TNMapWidget::refreshBottomLeftOverlay()
{
    updateBottomLeftOverlayPosition();
}

QImage TNMapWidget::grabBottomLeftOverlay()
{
    if (mBottomLeftOverlayWidget.isNull() || !mBottomLeftOverlayWidget->isVisible()) {
        return QImage();
    }
    // 抓取前先刷新浮层几何，避免图例内容变化后 sizeHint 过期导致抓到被裁剪的图
    updateBottomLeftOverlayPosition();
    QWidget* overlay = mBottomLeftOverlayWidget.data();
    QImage image = overlay->grab().toImage();
    const QSize rawSize = image.size();
    const double rawDpr = image.devicePixelRatio() > 0 ? image.devicePixelRatio() : 1.0;
    // 无条件归一到控件逻辑尺寸 + DPR 1.0：grab 结果与 QImage::scaled 都会保留
    // devicePixelRatio，绘制端再按 dpi 缩放时一旦语义不一致就会出现放大/截断，
    // 这里以控件 size() 为准做确定性缩放，后续链路不再感知 DPR
    const QSize logicalSize = overlay->size();
    if (!logicalSize.isEmpty() && image.size() != logicalSize) {
        image = image.scaled(logicalSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    image.setDevicePixelRatio(1.0);
    qInfo() << QString::fromUtf8(u8"抓取地图图例：控件size=%1x%2, sizeHint=%3x%4, grab=%5x%6(dpr=%7), 归一=%8x%9")
                   .arg(overlay->size().width()).arg(overlay->size().height())
                   .arg(overlay->sizeHint().width()).arg(overlay->sizeHint().height())
                   .arg(rawSize.width()).arg(rawSize.height())
                   .arg(rawDpr)
                   .arg(image.width()).arg(image.height());
    return image;
}

QWidget* TNMapWidget::bottomLeftOverlayWidget() const
{
    return mBottomLeftOverlayWidget.data();
}

QgsMapLayer* TNMapWidget::selectLayerByUri(const QString& layerUri)
{
    QgsMapLayer* layer = getLayer(layerUri);
    if (!layer) {
        return nullptr;
    }

    mSelectLayer = layer;
    mCurrentLayer = layer;
    mCanvas->setCurrentLayer(layer);
    notifyCurrentLayerChanged(layer);

    if (mEnableAddFeature) {
        rebindAddFeatureTool();
    }

    refreshMapStatus();

    return layer;
}

QgsMapLayer* TNMapWidget::getLayer(const QString& layerUri) const
{
    // 先按完整 URI 直接匹配
    if (mUriLayers.contains(layerUri)) return mUriLayers.value(layerUri);

    auto extractTypeName = [](const QgsDataSourceUri& ds) -> QString {
        QString typeName = ds.param(QStringLiteral("typename")).trimmed();
        if (typeName.isEmpty()) {
            typeName = ds.table().trimmed();
        }
        if (typeName.isEmpty() && !ds.schema().isEmpty() && !ds.table().isEmpty()) {
            typeName = ds.schema() + QLatin1Char('.') + ds.table();
        }
        return typeName;
    };

    const QgsDataSourceUri queryDs(layerUri);
    const QString queryTypeName = extractTypeName(queryDs);

    for (auto it = mUriLayers.constBegin(); it != mUriLayers.constEnd(); ++it) {
        QString uri = it.key();
        QgsDataSourceUri ds(uri);
        QString t = ds.table();
        if (t.isEmpty()) t = uri;
        if (t.compare(layerUri, Qt::CaseInsensitive) == 0) return it.value();
        QString full = ds.schema().isEmpty() ? ds.table() : ds.schema() + "." + ds.table();
        // 再按 schema.table 或仅表名匹配
        if (!full.isEmpty() && full.compare(layerUri, Qt::CaseInsensitive) == 0) return it.value();

        // WFS：URI 可能仅差 skip/forceInitialGetFeature 等参数，按 typename 对齐
        if (!queryTypeName.isEmpty()) {
            const QString storedTypeName = extractTypeName(ds);
            if (!storedTypeName.isEmpty()
                && storedTypeName.compare(queryTypeName, Qt::CaseInsensitive) == 0) {
                return it.value();
            }
        }
    }
    return nullptr;
}

bool TNMapWidget::saveEdits()
{
    if (QgsVectorLayer *vl = qobject_cast<QgsVectorLayer*>(mCanvas->currentLayer()))
    {
        if (vl->isEditable() && vl->isModified())
        {
            if (vl->commitChanges())
            {
                TnUi::showInformation(u8"保存成功", u8"成功保存对图层的修改。");
                if (m_editStack) {
                    m_editStack->notifyEditChanged();
                }
                return true;
            }
            else
            {
                TnUi::showCritical(u8"保存失败",
                    QString(u8"无法保存对图层：%1 的修改").arg(vl->commitErrors().join(", "))
                );
                return false;
            }
        }
    }

    return false;
}

bool TNMapWidget::saveAllEdits()
{
    if (!m_editStack) {
        return saveEdits();
    }
    return m_editStack->saveAll(nullptr);
}

void TNMapWidget::setToolBar(QToolBar* toolBar)
{
    if (toolBar)
    {
        toolBar->setParent(this);
        mDefaultToolBar->close();
        mToolBarLayout->removeWidget(mDefaultToolBar);
        mToolBarLayout->setMargin(0);
        mToolBarLayout->addWidget(toolBar);
    }
}

void TNMapWidget::setToolBar(QWidget* toolBar)
{
    if (toolBar)
    {
        toolBar->setParent(this);
        mDefaultToolBar->close();
        mToolBarLayout->removeWidget(mDefaultToolBar);
        mToolBarLayout->setMargin(0);
        mToolBarLayout->addWidget(toolBar);
    }
}

#ifdef TNP_ENABLE_ZOOM_TO_LAYER
void TNMapWidget::zoomToLayer(QgsMapLayer* layer)
{
    if (!mCanvas || !layer || !layer->isValid()) {
        return;
    }

    // 信任 provider/GetCapabilities 四至，避免 updateExtents() 触发额外 WFS GetFeature
    QgsRectangle ext = getLayerExtentInMapCrs(layer);
    if (ext.isEmpty() || !ext.isFinite()) {
        showMessage(QString::fromUtf8(u8"该图层暂无有效范围。"), 3000);
        return;
    }

    mCanvas->zoomToFeatureExtent(ext);
    m_defaultExtentActive = false;
    cancelDeferredLayerExtentZoom();
    refreshMapStatus(true);
}
#endif

void TNMapWidget::zoomToMergedLayerExtent(const QList<QgsMapLayer*>& layers)
{
    if (!mCanvas || layers.isEmpty()) {
        return;
    }

    if (zoomToLayersIfExtentValid(layers)) {
        cancelDeferredLayerExtentZoom();
        return;
    }
    if (m_defaultExtentActive) {
        scheduleDeferredLayerExtentZoom(layers);
    }
}

QgsRectangle TNMapWidget::getLayerExtentInMapCrs(QgsMapLayer* layer)
{
    if (!layer)
    {
        return QgsRectangle();
    }
    // 获取图层自身坐标系和范围
    QgsCoordinateReferenceSystem layerCrs = layer->crs();
    QgsRectangle layerExtent = layer->extent();

    // 如果图层坐标系与地图坐标系一致，直接返回
    auto mapCrs = QgsProject::instance()->crs();
    if (layerCrs == mapCrs) {
        return layerExtent;
    }

    // 创建坐标转换器（图层坐标系 -> 地图坐标系）
    QgsCoordinateTransform transform(
        layerCrs, 
        mapCrs,
        QgsProject::instance() // 项目（用于获取转换上下文）
    );

    // 转换范围（可能抛出异常，建议捕获）
    try {
        return transform.transform(layerExtent);
    }
    catch (const QgsCsException& e) {
        qCritical() << u8"坐标转换失败：" << e.what();
        return QgsRectangle(); // 返回空值
    }
}

void TNMapWidget::switchBaseMap(bool bOpen)
{
    mBaseMapAct->setChecked(bOpen);
    onBasemapClicked(bOpen);
}

void TNMapWidget::activateSpatialSelectTool(SpatialToolType type, bool activate)
{
    if (activate)
    {
        if (!mSpatialLastTool)
        {
            mSpatialLastTool = mCanvas->mapTool();
        }
        mSpatialType = type;
        switch (type)
        {
        case TNMapWidget::SpatialToolType::PointSelect:
            mCanvas->setMapTool(mSpatialSelectTool);
            break;
        case TNMapWidget::SpatialToolType::RectSelect:
            mCanvas->setMapTool(mSpatialRectSelect);
            break;
        default:
            break;
        }
    }
    else
    {
        mCanvas->setMapTool(mSpatialLastTool);
        mSpatialLastTool = nullptr;
        mSpatialType = SpatialToolType::None;
    }
}

void TNMapWidget::activateCompareExtentTool(bool activate)
{
    if (!mCanvas || !mCompareExtentTool) {
        return;
    }

    if (activate) {
        if (!mCompareExtentLastTool) {
            mCompareExtentLastTool = mCanvas->mapTool();
        }
        mCanvas->setMapTool(mCompareExtentTool);
        return;
    }

    if (mCompareExtentLastTool) {
        mCanvas->setMapTool(mCompareExtentLastTool);
    } else if (mPanTool) {
        mCanvas->setMapTool(mPanTool);
    }
    mCompareExtentLastTool = nullptr;
}

void TNMapWidget::showMessage(const QString& message, int timeout)
{
    mStatusBar->showMessage(message, timeout);
}

void TNMapWidget::refreshMapStatus(bool forceEmit)
{
    if (!mCanvas) {
        return;
    }

    QgsCoordinateReferenceSystem statusCrs = mCanvas->mapSettings().destinationCrs();
    if (!IsCrsDisplayable(statusCrs)) {
        statusCrs = QgsProject::instance()->crs();
    }
    if (!IsCrsDisplayable(statusCrs) && mCanvas->currentLayer()) {
        statusCrs = mCanvas->currentLayer()->crs();
    }
    if (!IsCrsDisplayable(statusCrs) && mCurrentLayer) {
        statusCrs = mCurrentLayer->crs();
    }
    if (!IsCrsDisplayable(statusCrs) && !mLayers.isEmpty()) {
        statusCrs = mLayers.first()->crs();
    }

    const QString crsText = FormatCrsStatusText(statusCrs);

    if (mCrsLabel) {
        mCrsLabel->setText(crsText);
    }

    MapStatus nextStatus;
    nextStatus.crsText = crsText;
    nextStatus.coordText = mCoordLabel ? mCoordLabel->text() : QString();
    nextStatus.scaleText = mScaleLabel ? mScaleLabel->text() : QString();

    if (forceEmit || nextStatus != mMapStatus) {
        mMapStatus = nextStatus;
        emit mapStatusChanged(mMapStatus);
    }
}

void TNMapWidget::updateBottomLeftOverlayPosition()
{
    if (!mCanvas || !mBottomLeftOverlayWidget) {
        return;
    }

    const QSize overlaySize = mBottomLeftOverlayWidget->sizeHint().isValid()
        ? mBottomLeftOverlayWidget->sizeHint()
        : mBottomLeftOverlayWidget->size();
    if (overlaySize.isEmpty()) {
        return;
    }

    const int x = mBottomLeftOverlayMargin;
    const int y = mCanvas->height() - overlaySize.height() - mBottomLeftOverlayMargin;
    mBottomLeftOverlayWidget->setGeometry(x, qMax(0, y), overlaySize.width(), overlaySize.height());
    mBottomLeftOverlayWidget->raise();
}

void TNMapWidget::notifyCurrentLayerChanged(QgsMapLayer* layer)
{
    emit currentLayerChanged(layer);
}

bool TNMapWidget::updateSymbolColor(QgsVectorLayer* layer)
{
    if (layer)
    {
        return updateLayerSymbolColor(layer);
    }
    
    // 隐藏所有图层
    foreach(QgsMapLayer * tmplayer, mLayers) {
        if (!tmplayer) continue;

        updateLayerSymbolColor(qobject_cast<QgsVectorLayer*>(tmplayer));
    }

    return true;
}

bool TNMapWidget::updateLayerSymbolColor(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid() || layer->type() != Qgis::LayerType::Vector)
    {
        return false;
    }

    if (m_styleProjectId <= 0 || m_styleCaseId < 0) {
        return false;
    }

    const QString layerKey = LayerStylePersistence::layerStorageKey(layer);
    if (layerKey.isEmpty()) {
        return false;
    }

    QString fieldName;
    FieldColorRule rule;
    QString errorMessage;
    if (LayerStylePersistence::instance().loadLatestFieldColorRule(
            m_styleProjectId, m_styleCaseId, layerKey, &fieldName, &rule, &errorMessage)) {
        if (LayerStyleRenderer::applyFieldColorRule(layer, fieldName, rule, &errorMessage)) {
            return true;
        }
        qWarning() << QString::fromUtf8(u8"恢复字段样式失败：%1").arg(errorMessage);
        return false;
    }

    return false;
}

void TNMapWidget::updateCanvasLayers()
{
    mCanvas->setLayers(exportLayerStack(mBaseMapAct && mBaseMapAct->isChecked()));
}

QList<QgsMapLayer*> TNMapWidget::exportLayerStack(bool includeBasemap)
{
    struct LayerOrderEntry {
        QgsMapLayer* layer = nullptr;
        int mLayersIndex = 0;
        int level = 0;
    };

    QList<LayerOrderEntry> entries;
    entries.reserve(mLayers.size());
    for (int i = 0; i < mLayers.size(); ++i) {
        QgsMapLayer* layer = mLayers.at(i);
        if (!layer) {
            continue;
        }
        if (isAttributeOnlyMapLayer(layer)) {
            continue;
        }
        bool ok = false;
        int level = layer->property(LEVEL_PROPERTY).toInt(&ok);
        if (!ok) {
            level = 0;
        }
        entries.append({layer, i, level});
    }

    std::sort(entries.begin(), entries.end(), [](const LayerOrderEntry& a, const LayerOrderEntry& b) {
        if (a.level != b.level) {
            return a.level < b.level;
        }
        return a.mLayersIndex < b.mLayersIndex;
    });

    QList<QgsMapLayer*> curLayers;
    curLayers.reserve(entries.size() + mBaseMapLayers.size());
    for (const LayerOrderEntry& e : entries) {
        curLayers.append(e.layer);
    }

    if (includeBasemap && mBaseMapAct && mBaseMapAct->isChecked()) {
        ensureBasemapLayersFromSession();
        for (QgsMapLayer* bl : mBaseMapLayers) {
            curLayers.append(bl);
        }
    }

    return curLayers;
}

QgsMapSettings TNMapWidget::exportMapSettings(bool includeBasemap)
{
    QgsMapSettings settings = mCanvas->mapSettings();
    settings.setLayers(exportLayerStack(includeBasemap));
    return settings;
}

QString TNMapWidget::getGeometryColumnName(const QString& dbName, const QString& tableName,
    const QString& host, const QString& port,
    const QString& user, const QString& password,
    const QString& schema, QVariantMap& fieldAliases)
{
    QString connName = "geom_conn_" + QString::number(QRandomGenerator().generate64());
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connName);
    db.setHostName(host);
    db.setPort(port.toInt());
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);

    if (db.open()) {
        QSqlQuery query(db);
        // 先校验区别
        QString aliasesSql = QString(
            "SELECT a.attname, d.description "
            "FROM pg_class c "
            "JOIN pg_attribute a ON a.attrelid = c.oid "
            "LEFT JOIN pg_description d ON d.objoid = a.attrelid AND d.objsubid = a.attnum "
            "WHERE c.relname = '%1' AND a.attnum > 0 AND NOT a.attisdropped"
        ).arg(tableName);
        if (query.exec(aliasesSql)) {
            while (query.next()) {
                QString fieldName = query.value(0).toString();
                QString alias = query.value(1).toString();
                if (!alias.isEmpty())
                    fieldAliases[fieldName] = alias;
            }
        }

        // 查询 geometry_columns
        QString sql = QString(
            "SELECT f_geometry_column FROM geometry_columns "
            "WHERE f_table_schema='%1' AND f_table_name='%2' LIMIT 1"
        ).arg(schema, tableName);

        if (query.exec(sql) && query.next()) {
            QString geomCol = query.value(0).toString();
            qDebug() << db.connectionName() << " Geometry field======>" << geomCol;
            db.close();
            QSqlDatabase::removeDatabase(connName);
            return geomCol;
        }

        // 如果 geometry_columns 没查到，再查 information_schema.columns
        sql = QString(
            "SELECT column_name FROM information_schema.columns "
            "WHERE table_schema='%1' AND table_name='%2' AND udt_name IN ('geometry','geography') LIMIT 1"
        ).arg(schema, tableName);

        if (query.exec(sql) && query.next()) {
            QString geomCol = query.value(0).toString();
            db.close();
            QSqlDatabase::removeDatabase(connName);
            return geomCol;
        }
        db.close();
    }

    QSqlDatabase::removeDatabase(connName);
    return QString();
}

bool TNMapWidget::isLayerExists(QgsMapLayer* targetLayer)
{
    QString targetUri = targetLayer->dataProvider()->dataSourceUri();
    if (targetLayer->type() == Qgis::LayerType::Vector)
    {
        // 清除密码字段
        QgsDataSourceUri uri(targetUri);
        uri.setPassword("");
        targetUri = uri.uri();
        if (targetUri.isEmpty())
        {
            targetUri = targetLayer->dataProvider()->dataSourceUri();
        }
    }

    // 遍历所有图层，对比数据源URI
    foreach(QgsMapLayer * layer, mLayers) {
        if (!layer) continue;

        // 获取图层的数据源 URI（标准化处理）
        QString layerUri = layer->dataProvider()->dataSourceUri();

        // 特殊处理：PostGIS图层的 URI 可能包含密码等动态信息，需要过滤敏感字段
        if (layer->type() == Qgis::LayerType::Vector) {
            QgsDataSourceUri uri(layerUri);
            // 清除密码等动态字段（避免因密码变化导致误判）
            uri.setPassword("");
            layerUri = uri.encodedUri();
            if (layerUri.isEmpty())
            {
                layerUri = layer->dataProvider()->dataSourceUri();
            }
        }

        // 3. 对比目标URI与图层URI（忽略大小写）
        if (layerUri.compare(targetUri, Qt::CaseInsensitive) == 0) {
            return true; // 找到匹配的图层
        }
    }

    return false; // 未找到匹配的图层
}

void TNMapWidget::onPanClicked()
{
    if (mEnableEditLayer)
    {
        enableEditFeature(false);
    }
    if (mEnableDelete)
    {
        mEditTool->setEnableDelete(false);
        mEnableDelete = false;
    }
    if (mEnableAddFeature)
    {
        enableAddFeature(false);
    }
}

void TNMapWidget::showEvent(QShowEvent* e)
{
    if (!mIsInited)
    {
        if (mBaseMapLayers.empty())
        {
            onBasemapClicked(true);
        }
        if (!mBaseMapLayers.empty())
        {
            applyDefaultBasemapExtent();
        }

        // ===== TEMP：默认加载本地测试 SHP（先行政区、后地铁线，后者在上层；测试完成后删除本段及 mDevTestShpLoaded）=====
        if (!mDevTestShpLoaded) {
            mDevTestShpLoaded = true;
            const QString adminShp = QString::fromUtf8(u8"D:/work/workspace/data/成都shp/成都行政区.shp");
            const QString metroShp = QString::fromUtf8(u8"D:/work/workspace/data/成都shp/地铁线路.shp");
            if (QFile::exists(adminShp)) {
                addVectorLayer(adminShp, QString::fromUtf8(u8"成都行政区(测试)"), 0, QStringLiteral("ogr"));
            }
            if (QFile::exists(metroShp)) {
                addVectorLayer(metroShp, QString::fromUtf8(u8"地铁线路(测试)"), 0, QStringLiteral("ogr"));
            }
        }
        // ===== END TEMP =====

        mCanvas->refresh();

        mIsInited = true;
    }

    updateBottomLeftOverlayPosition();
    refreshMapStatus(true);

    return QFrame::showEvent(e);
}

bool TNMapWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mCanvas && event) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest) {
            updateBottomLeftOverlayPosition();
        }
        if (event->type() == QEvent::Wheel) {
            noteUserAdjustedExtent();
        } else if (event->type() == QEvent::MouseMove) {
            const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
            if (mouseEvent->buttons() != Qt::NoButton) {
                noteUserAdjustedExtent();
            }
        }
    }

    return QFrame::eventFilter(watched, event);
}

void TNMapWidget::onEditClicked()
{
    enableEditFeature(true);
}

void TNMapWidget::onSaveClicked()
{
    if (saveEdits())
    {
        enableEditFeature(false);
    }
}

void TNMapWidget::onFeatureSelected(QgsVectorLayer *layer, const QgsFeatureIds &fids)
{
    // 可以在这里处理要素选择后的逻辑
}

void TNMapWidget::onFeatureEdited(QgsVectorLayer *layer, QgsFeatureId fid)
{
    emit featureEdited(layer, fid);
}

bool TNMapWidget::ensureBasemapLayersFromSession()
{
    if (!mBaseMapLayers.isEmpty()) {
        return false;
    }

    const QSharedPointer<BaseConfigJson> cfg = AppSession::instance().baseConfig();
    if (cfg.isNull()) {
        qWarning() << u8"[TNMapWidget] 基础配置未就绪，跳过底图加载。";
        return false;
    }

    const QString url2 = cfg->j_mapurl2();
    const QString url3 = cfg->j_mapurl3();
    if (url2.isEmpty() && url3.isEmpty()) {
        qWarning() << u8"[TNMapWidget] mapurl2/mapurl3 均为空，跳过底图加载。";
        return false;
    }

    const auto tryAdd = [this](const QString& url, const QString& name) -> bool {
        if (url.isEmpty()) {
            return false;
        }
        const TnpXyzConnection conn{ url, name, 0, 18 };
        QgsRasterLayer* layer = createXyzLayer(conn);
        if (!layer) {
            qWarning() << u8"[TNMapWidget] 底图图层创建失败：" << name;
            return false;
        }
        mBaseMapLayers.append(layer);
        QgsProject::instance()->addMapLayer(layer, false);
        return true;
    };

    // 与一期一致：先追加的层在列表靠前，最终在画布中位于下层；mapurl2 在下、mapurl3（注记）在上
    tryAdd(url3, QString::fromUtf8(u8"底图标注"));
    tryAdd(url2, QString::fromUtf8(u8"底图"));

    return !mBaseMapLayers.isEmpty();
}

bool TNMapWidget::canvasExtentIsSet() const
{
    if (!mCanvas) {
        return false;
    }
    const QgsRectangle extent = mCanvas->extent();
    return extent.isFinite() && !extent.isEmpty();
}

void TNMapWidget::applyAddedLayerExtent(QgsMapLayer* layer)
{
    if (!mCanvas || !layer) {
        return;
    }

    const QgsRectangle extent = getLayerExtentInMapCrs(layer);
    if (extent.isFinite() && !extent.isEmpty()) {
        mCanvas->setExtent(extent);
        m_defaultExtentActive = false;
        cancelDeferredLayerExtentZoom();
        return;
    }

    if (!canvasExtentIsSet()) {
        mCanvas->zoomToFullExtent();
    }
}

bool TNMapWidget::zoomToLayersIfExtentValid(const QList<QgsMapLayer*>& layers)
{
    if (!mCanvas || layers.isEmpty()) {
        return false;
    }

    QgsRectangle combined;
    bool hasValid = false;
    for (QgsMapLayer* layer : layers) {
        if (!layer || !layer->isValid()) {
            continue;
        }
        // 信任 provider/GetCapabilities 四至，避免 updateExtents() 触发额外 WFS GetFeature
        const QgsRectangle extent = getLayerExtentInMapCrs(layer);
        if (extent.isEmpty() || !extent.isFinite()) {
            continue;
        }
        if (!hasValid) {
            combined = extent;
            hasValid = true;
        } else {
            combined.combineExtentWith(extent);
        }
    }

    if (!hasValid) {
        return false;
    }

    mCanvas->zoomToFeatureExtent(combined);
    m_defaultExtentActive = false;
    refreshMapStatus(true);
    return true;
}

void TNMapWidget::cancelDeferredLayerExtentZoom()
{
    if (m_deferredExtentRefreshConnection) {
        QObject::disconnect(m_deferredExtentRefreshConnection);
        m_deferredExtentRefreshConnection = {};
    }
    m_deferredExtentLayers.clear();
}

void TNMapWidget::scheduleDeferredLayerExtentZoom(const QList<QgsMapLayer*>& layers)
{
    cancelDeferredLayerExtentZoom();
    if (!m_defaultExtentActive || !mCanvas) {
        return;
    }

    for (QgsMapLayer* layer : layers) {
        if (!layer) {
            continue;
        }
        m_deferredExtentLayers.append(layer);
    }
    if (m_deferredExtentLayers.isEmpty()) {
        return;
    }

    m_deferredExtentRefreshConnection = connect(
        mCanvas, &QgsMapCanvas::mapCanvasRefreshed,
        this, &TNMapWidget::onDeferredExtentFirstRefresh);
}

void TNMapWidget::onDeferredExtentFirstRefresh()
{
    const QList<QPointer<QgsMapLayer>> layers = m_deferredExtentLayers;
    cancelDeferredLayerExtentZoom();
    if (!m_defaultExtentActive) {
        return;
    }

    QList<QgsMapLayer*> pending;
    pending.reserve(layers.size());
    for (const QPointer<QgsMapLayer>& layer : layers) {
        if (layer) {
            pending.append(layer.data());
        }
    }
    if (pending.isEmpty()) {
        return;
    }
    zoomToLayersIfExtentValid(pending);
}

void TNMapWidget::noteUserAdjustedExtent()
{
    m_defaultExtentActive = false;
    cancelDeferredLayerExtentZoom();
}

void TNMapWidget::applyDefaultBasemapExtent()
{
    if (!mCanvas || mBaseMapLayers.isEmpty() || canvasExtentIsSet()) {
        return;
    }
    const double sanyaWest = 12178013;
    const double sanyaEast = 12214973;
    const double sanyaSouth = 2056117;
    const double sanyaNorth = 2077360;
    QgsRectangle sanyaExtent(sanyaWest, sanyaSouth, sanyaEast, sanyaNorth);
    sanyaExtent = transformLayer2Map(sanyaExtent, mBaseMapLayers[0]);
    if (sanyaExtent.isEmpty() || !sanyaExtent.isFinite()) {
        return;
    }
    m_defaultExtentActive = true;
    mCanvas->setExtent(sanyaExtent);
}

void TNMapWidget::onBasemapClicked(bool checked)
{
    if (checked)
    {
        ensureBasemapLayersFromSession();

        if (mBaseMapLayers.size())
        {
            mCanvas->stopRendering();
            updateCanvasLayers();
            mCanvas->refresh();
        }
    }
    else
    {
        if (mBaseMapLayers.size())
        {
            mCanvas->stopRendering();
            updateCanvasLayers();
            mCanvas->refresh();
        }
    }
}

void TNMapWidget::onUpdateScale(double scale)
{
    QString scaleStr = QString(u8"比例尺：1:%1").arg(QString::number(scale, 'f', 2));
    mScaleLabel->setText(scaleStr);
    refreshMapStatus();
}

void TNMapWidget::onCoordinatesXYChanged(const QgsPointXY& p)
{
    QString posStr = QString(u8"坐标：%1°,%2°").arg(QString::number(p.x(), 'f', 3)).arg(QString::number(p.y(), 'f', 3));
    mCoordLabel->setText(posStr);
    refreshMapStatus();
}

void TNMapWidget::onSnapPointDetected(TNFeatureAddTool::SnapType type, const QString& typeName)
{
    Q_UNUSED(typeName);
    QString msg = u8"添加要素工具，捕捉到点：%1。";
    switch (type)
    {
    case TNFeatureAddTool::SnapNone:
        msg = u8"添加要素工具，当前未捕捉到点。";
        break;
    case TNFeatureAddTool::SnapPointFeature:
        msg = msg.arg(u8"点要素");
        break;
    case TNFeatureAddTool::SnapLineStart:
        msg = msg.arg(u8"线起点");
        break;
    case TNFeatureAddTool::SnapLineMiddle:
        msg = msg.arg(u8"线中点");
        break;
    case TNFeatureAddTool::SnapLineEnd:
        msg = msg.arg(u8"线终点");
        break;
    case TNFeatureAddTool::SnapLineVertex:
        msg = msg.arg(u8"线折点");
        break;
    default:
        msg = u8"添加要素工具，当前未捕捉到点。";
        break;
    }

    showMessage(msg, 1500);
}

void TNMapWidget::onUpdateSymbolColor()
{
    updateSymbolColor(nullptr);
}

// 判断字段是否适合QGIS渲染
bool IsFieldRenderable(const QgsField& field)
{
    QVariant::Type type = static_cast<QVariant::Type>(field.type());

    // 适合渲染的常见类型：数值、字符串、布尔
    switch (type)
    {
        case QVariant::Int:
        case QVariant::Double:
        case QVariant::LongLong:
        case QVariant::UInt:
        case QVariant::ULongLong:
        case QVariant::String:
        case QVariant::Bool:
            return true;

        // 不适合常规渲染的类型（二进制、日期时间等视需
        case QVariant::ByteArray:    // 二进制数据
        case QVariant::Date:         // 日期（可渲染，但需特殊处理）
        case QVariant::Time:         // 时间（可渲染，但需特殊处理）
        case QVariant::DateTime:     // 日期时间（可渲染，但需特殊处理）
        case QVariant::Invalid:      // 无效类型
        default:
            return false;
    }
}

void SetSymbolConfig(QgsSymbol* symbol, Qgis::GeometryType geomType, double opacity, double lineWidth)
{
    // 设置不透明度（0-1之间，1为完全不透明）
    symbol->setOpacity(qBound(0.0, opacity / 100.0, 1.0));  // 假设opacity是double类型，例如0.5表示半透明

    // 根据几何类型设置线宽（线和面的边界线需要线宽，点符号的边框也可设置）
    if (geomType == Qgis::GeometryType::Line)
    {
        // 线符号：直接设置线宽（单位：毫米）
        QgsLineSymbol* lineSymbol = static_cast<QgsLineSymbol*>(symbol);
        lineSymbol->setWidth(lineWidth);  // lineWidth是double类型，例如0.5
    }
    else if (geomType == Qgis::GeometryType::Polygon)
    {
        QgsFillSymbol* fillSymbol = dynamic_cast<QgsFillSymbol*>(symbol);
        if (!fillSymbol)
        {
            qDebug() << u8"符号转换失败：不是QgsFillSymbol类型";
            return; // 或处理错误逻辑
        }

        // 遍历所有符号层，确保是线类型后再设置线宽
        for (int i = 0; i < fillSymbol->symbolLayerCount(); ++i)
        {
            QgsSymbolLayer* layer = fillSymbol->symbolLayer(i);
            qDebug() << layer->type();
            qDebug() << layer->layerType();
            if (layer->type() == Qgis::SymbolType::Line)
            {
                //QgsLineSymbolLayer* lineLayer = static_cast<QgsLineSymbolLayer*>(layer);
                //lineLayer->setWidth(lineWidth);

                QgsSimpleLineSymbolLayer* lineLayer = dynamic_cast<QgsSimpleLineSymbolLayer*>(layer);
                if (lineLayer)
                {
                    lineLayer->setWidth(lineWidth);
                    break; // 假设只需要设置第一个线符号层
                }
            }
            else if (layer->type() == Qgis::SymbolType::Fill)
            {
                QgsSimpleFillSymbolLayer* simpleFillLayer = dynamic_cast<QgsSimpleFillSymbolLayer*>(layer);
                if (simpleFillLayer)
                {
                    simpleFillLayer->setStrokeWidth(lineWidth); // 设置线宽
                    break;
                }
            }
        }

        // 面符号：设置边界线宽（面的边框线）
        // 面符号的边界线是一个线符号层，获取第一个线符号层并设置线宽
        /*if (fillSymbol->symbolLayerCount() > 0)
        {
            QgsLineSymbolLayer* borderLayer = static_cast<QgsLineSymbolLayer*>(fillSymbol->symbolLayer(0));
            borderLayer->setWidth(lineWidth);  // 设置面的边框线宽
        }*/
    }
    else if (geomType == Qgis::GeometryType::Point)
    {
        // 点符号：如果点是线框样式，可设置边框线宽（根据实际符号样式调整）
        QgsMarkerSymbol* markerSymbol = static_cast<QgsMarkerSymbol*>(symbol);
        if (markerSymbol->symbolLayerCount() > 0)
        {
            // 假设点符号的第一个层是线框层（如SimpleMarker）
            QgsSimpleMarkerSymbolLayer* markerLayer = static_cast<QgsSimpleMarkerSymbolLayer*>(markerSymbol->symbolLayer(0));
            markerLayer->setStrokeWidth(lineWidth);  // 设置点的边框线宽
        }
    }
}

// Phase2 暂不使用 FiledColorRule 相关功能，注释掉以下函数
/*
bool isDoubleEqual(double a, double b, double epsilon = 1e-6) {
    // 计算绝对值差值，小于等于阈值则认为相等
    return qAbs(a - b) <= epsilon;
}

// 根据字段颜色刷新图层元素
void UpdateLayerFeatures(QgsVectorLayer* layer, const QString& fieldName, const FiledColorRule& filedColorRule)
{
    //1. 参数校验
    if (!layer || !layer->isValid() || layer->type() != Qgis::LayerType::Vector ||
        fieldName.isEmpty() || !(filedColorRule.Invalid()))
        return;

    int fieldIndex = layer->fields().indexFromName(fieldName);
    if (fieldIndex == -1)
        return;
    QgsField field = layer->fields().field(fieldIndex);

    QgsFeatureRenderer* renderer = nullptr;
    // 获取图层的几何类型
    Qgis::GeometryType geomType = QgsWkbTypes::geometryType(layer->wkbType());

    //3. 处理数字类型字段
    if (filedColorRule.category == FiledType::Number)
    {
        // 使用QgsRendererRange列表（QGIS 2.x常用类型）
        QList<QgsRendererRange> ranges;// 保存「值域颜色」规则
        for (const ColorRule& entry : filedColorRule.colors)
        {
            qDebug() << "entry " << entry.toJsonString();

            QColor color(entry.color);// 解析颜色（如#aa55ff）
            QgsSymbol* symbol = QgsSymbol::defaultSymbol(geomType);
            symbol->setColor(color);
            SetSymbolConfig(symbol, geomType, filedColorRule.opacity, filedColorRule.lineWidth);

            if (isDoubleEqual(entry.min,entry.max))// 处理单元格格式
            {
                ranges.append(QgsRendererRange(entry.min, entry.min, symbol, QString("%1").arg(entry.min)));
            }
            else// 处理范围格式
            {
                // 创建对应几何类型的符号（点/线/面）并设置颜色
                // 直接构造QgsRendererRange（兼容QGIS 2.x）
                // 添加范围规则（值在[minVal, maxVal]时使用该符号）
                ranges.append(QgsRendererRange(entry.min, entry.max, symbol, QString("%1 - %2").arg(entry.min).arg(entry.max)));
            }
        }

        if (!ranges.isEmpty())
        {
            // 构造分级渲染器（QGIS 2.x构造方式）
            //核心：用QgsGraduatedSymbolRenderer按数值范围匹配颜色
            renderer = new QgsGraduatedSymbolRenderer(fieldName, ranges);
        }
    }
    //4. 处理布尔型/字符串型字段
    else if (filedColorRule.category == FiledType::Bool || filedColorRule.category == FiledType::String)
    {
        QList<QgsRendererCategory> categories;// 保存「值颜色」规则

        for (const ColorRule& entry : filedColorRule.colors)
        {
            qDebug() << "entry " << entry.toJsonString();
            QColor color(entry.color);// 解析颜色（如#aa55ff）
            QVariant val = entry.Value(filedColorRule.category);

            // 创建符号并设置颜色
            QgsSymbol* symbol = QgsSymbol::defaultSymbol(geomType);
            symbol->setColor(color);
            SetSymbolConfig(symbol, geomType, filedColorRule.opacity, filedColorRule.lineWidth);

            // 添加分类规则（值等于val时使用该符号）
            categories.append(QgsRendererCategory(val, symbol, val.toString()));
        }

        if (!categories.isEmpty())
        {
            // 创建分类渲染器
            //核心：用QgsCategorizedSymbolRenderer按值精确匹配颜色
            renderer = new QgsCategorizedSymbolRenderer(fieldName, categories);
        }
    }

    //5. 应用渲染器并刷新图层
    if (renderer)
    {
        layer->setRenderer(renderer);// 将滤镜应用到图层
        layer->triggerRepaint();// 触发重绘，使新样式生效
    }
}
*/

/**
 * @brief 为 QgsVectorLayer 设置默认渲染符号（按点、线、面自动适配）
 * @param layer 待设置的矢量图层（必须是点/线/面类型，且已初始化）
 */
void TNMapWidget::setDefaultRendererForVectorLayer(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid())
    {
        qWarning() << u8"无效的矢量图层，无法设置渲染符号！";
        return;
    }

    // 1. 获取图层的几何类型（点/线/面）
    Qgis::GeometryType geomType = layer->geometryType();
    QgsSymbol* defaultSymbol = nullptr;

    // 2. 根据几何类型创建对应的默认符号
    switch (geomType)
    {
    case Qgis::GeometryType::Point:
    {
        // 点图层：默认圆形符号
        defaultSymbol = QgsMarkerSymbol::createSimple(
            QVariantMap{
                {"name", "circle"},       // 符号形状（圆形，可选：square、triangle等）
                {"color", "255,90,95,255"}, // 颜色 #ff5a5f
                {"size", "1.6"},            // 大小（单位：pt）
                {"outline_color", "0,0,0,255"}//, // 边框颜色
                //{"outline_width", "0.5"}  // 边框宽度
            }
        );
        break;
    }
    case Qgis::GeometryType::Line:
    {
        // 线条图层：默认实线
        defaultSymbol = QgsLineSymbol::createSimple(
            QVariantMap{
                {"color", "0,0,0,255"}, // 线条颜色（纯黑）
                {"width", "0.6"},         // 线宽（单位：pt）
                {"line_style", "solid"}   // 线型（实线，可选：dash、dot等）
            }
        );
        break;
    }
    case Qgis::GeometryType::Polygon:
    {
        // 面图层：默认填充
        defaultSymbol = QgsFillSymbol::createSimple(
            QVariantMap{
                {"color", "241,241,241,135"}, // 填充色
                {"outline_color", "0,0,0,255"}, // 边框颜色
                {"outline_width", "0.4"},  // 边框宽度
                {"style", "solid"}         // 填充样式（实线）
            }
        );
        break;
    }

    default:
        qWarning() << u8"不支持的几何类型，不能设置默认符号！";
        return;
    }

    // 3. 创建单一符号渲染器，并绑定到图层
    if (defaultSymbol)
    {
        QgsSingleSymbolRenderer* renderer = new QgsSingleSymbolRenderer(defaultSymbol);
        layer->setRenderer(renderer); // 图层会自动接管渲染器和符号的生命周期，无需手动释放

        // 4. 刷新图层，使渲染生效（关键！否则可能不显示）
        //layer->triggerRepaint();
        //layer->emitStyleChanged(); // 通知QGIS画布更新样式
    }
}

