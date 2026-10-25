#ifndef TNP_ATTRIBUTE_WIDGET_H
#define TNP_ATTRIBUTE_WIDGET_H
#include <QWidget>
#include "qgsmapcanvas.h"
#include "qgsattributetableview.h"
#include "qgsattributetablefiltermodel.h"
#include "qgsattributetablemodel.h"
#include "qgsvectorlayer.h"
#include "qgsexpression.h"
#include "qgsvectorlayercache.h"
#include "qgsfeatureid.h"
#include "tnfilterheaderview.h"

class QProgressDialog;

class TNAttributeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TNAttributeWidget(QWidget* parent = nullptr);
    virtual ~TNAttributeWidget();

    // 设置地图画布
    void setCanvas(QgsMapCanvas* canvas);

    // 设置当前图层（不可见时仅绑定图层，延迟到 showEvent / 可见时再加载表）
    void setCurrentLayer(QgsVectorLayer* layer);

    // 获取当前图层
    QgsVectorLayer* currentLayer() const;

    // 刷新属性表内容
    void refreshTable();

    // 根据要素ID选择属性表行
    bool selectFeatureById(int fid, bool clearExisting = true);

    // 获取当前选中的要素ID列表
    QList<QgsFeatureId> selectedFeatureIds() const;

    // 只显示选中要素
    void setFilteredFeatures(const QgsFeatureIds& ids);

    // 设置过滤条件（支持QGIS表达式）
    void setFilter(const QString& filterString);

    // 清除过滤条件
    void clearFilter();

    // 按列排序
    void sortByColumn(int column, Qt::SortOrder order = Qt::AscendingOrder);

    // 导出属性表为CSV文件
    bool exportToCsv(const QString& filePath);

    // 清空表
    void clearTable();

    /** @brief 二期浅色属性表外观（方案详情/基础数据「数据」页与选择集弹窗一致） */
    void setPhase2ChromeEnabled(bool enabled);
    bool phase2ChromeEnabled() const { return m_phase2Chrome; }

    /**
     * @brief 是否允许双击单元格内联编辑（默认关闭；数据页开启）
     */
    void setInlineEditingEnabled(bool enabled);
    bool inlineEditingEnabled() const { return m_inlineEditingEnabled; }

protected:
    void showEvent(QShowEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    /** @brief 按表头内容重算列宽；forceFull 为 true 时忽略当前模式强制按内容策略重算 */
    void resetColumnWidth(bool forceFull = true);

signals:
    // 要素选择变化信号
    void featureSelectionChanged(const QList<QgsFeatureId>& selectedIds);
private slots:
    // 处理选择变化
    void slotSelectionChanged();
    // 列过滤变化
    void onColumnFilterChanged(int column, const QString& value);
    void onLoadProgress(int i, bool& cancel);
    void onLoadFinished();
private:
    enum class ColumnWidthMode
    {
        Content,
        Fill
    };

    // 释放资源
    void releaseResources();

    /** @brief 若表内容相对当前图层已过期，则同步构建并加载属性表 */
    void ensureTableLoaded();

    /** @brief 在已有模型上执行 loadLayer，并处理加载中的图层切换 */
    void runLoadLayerWithProgress();

    void cancelLoadProgress();
    void destroyLoadProgress();

    // 初始化表格组件
    void initTableComponents();
    void applyPhase2VisualChrome();
    void applyDefaultVisualChrome();
    void applyInlineEditingTriggers();
    bool ensureLayerEditableForInlineEdit();
    int preferredColumnWidth(int column) const;
    int tableContentWidth() const;

    QgsMapCanvas* mMapCanvas = nullptr; // 地图画布（过滤模型必需）
    QgsAttributeTableView* mTableView = nullptr; // 属性表视图
    QgsAttributeTableModel* mTableModel = nullptr; // 属性表模型
    QgsAttributeTableFilterModel* mFilterModel = nullptr; // 过滤模型
    QgsVectorLayerCache* mLayerCache = nullptr; // 图层缓存
    QgsVectorLayer* mCurrentLayer = nullptr; // 当前关联图层
    QString mCurrentFilterExpression; // 当前过滤表达式（用于刷新时恢复）

    bool mInited = false;
    bool mTableDirty = false;
    bool mLoading = false;
    quint64 mLoadGeneration = 0;
    quint64 mActiveLoadGeneration = 0;
    QProgressDialog* mProgressDlg = nullptr;

    TNFilterHeaderView* mHeaderView = nullptr;
    QMap<int, QString> mColumnFilters;

    bool m_phase2Chrome = false;
    bool m_inlineEditingEnabled = false;
    ColumnWidthMode m_columnWidthMode = ColumnWidthMode::Content;
};

#endif // !TNP_ATTRIBUTE_WIDGET_H
