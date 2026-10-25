#ifndef TN_SELECTION_SET_DIALOG_H
#define TN_SELECTION_SET_DIALOG_H

#include "framelessdialog.h"
#include <QComboBox>
#include <QVBoxLayout>
#include <QHash>
#include "qgsmapcanvas.h"
#include "qgsvectorlayer.h"
#include "tnattributewidget.h"

// 选择集显示弹窗：用于显示不同图层的选择集属性表
class TNSelectionSetDialog : public FramelessDialog
{
    Q_OBJECT
public:
    explicit TNSelectionSetDialog(QWidget* parent = nullptr);
    ~TNSelectionSetDialog() override;

    // 设置地图画布（用于属性表模型过滤）
    void setCanvas(QgsMapCanvas* canvas);

    // 添加或刷新某图层的选择集；若该图层不存在于下拉框，则添加；存在则仅刷新
    void addOrRefreshLayer(QgsVectorLayer* layer, const QgsFeatureIds& currentSelection);

    // 设置当前显示的图层（会切换下拉框选项并刷新属性表）
    void setCurrentLayer(QgsVectorLayer* layer);

protected slots:
    void onLayerComboChanged(int index);

protected:
    void updateAttributeViewForLayer(QgsVectorLayer* layer);
    QString buildSelectionFilter(const QgsFeatureIds& ids) const;

private:
    QComboBox* mLayerCombo = nullptr;
    TNAttributeWidget* mAttrWidget = nullptr;
    QgsMapCanvas* mCanvas = nullptr;

    // 使用图层 id 作为 key，存储图层指针与最近一次选择集
    struct LayerEntry { QgsVectorLayer* layer = nullptr; QgsFeatureIds selection; };
    QHash<QString, LayerEntry> mLayers; // key: layer->id()
};

#endif // TN_SELECTION_SET_DIALOG_H