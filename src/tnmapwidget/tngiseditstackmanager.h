#ifndef TNGISEDITSTACKMANAGER_H
#define TNGISEDITSTACKMANAGER_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVector>

#include "qgsfeatureid.h"

class QUndoStack;
class TNMapWidget;
class QgsMapLayer;
class QgsVectorLayer;

class TNGisEditStackManager : public QObject
{
    Q_OBJECT

public:
    explicit TNGisEditStackManager(QObject* parent = nullptr);
    ~TNGisEditStackManager() override;

    void attachMap(TNMapWidget* map);
    void forgetLayer(QgsVectorLayer* layer);
    void notifyEditChanged();

    bool hasUnsavedChanges() const;
    QStringList modifiedLayerNames() const;

    bool saveAll(QString* summaryOut = nullptr);
    /** @brief 保存单个图层的编辑（与 saveAll 相同提交路径） */
    bool saveLayer(QgsVectorLayer* layer, QString* summaryOut = nullptr);
    void discardAll();

    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    /**
     * 查看下一步可撤回的图层名
     *
     * 从地图级历史栈顶向下找第一个有效栈点，不改栈。
     * @return 图层名；无有效栈点时返回空串
     */
    QString peekUndoLayerName() const;

signals:
    void editStackChanged();
    void saveCompleted(bool ok, const QString& summary);

private slots:
    void onMapFeatureAdded(QgsVectorLayer* layer, QgsFeatureId fid);
    void onMapFeatureEdited(QgsVectorLayer* layer, QgsFeatureId fid);
    void onMapFeatureDeleted(QgsVectorLayer* layer, QgsFeatureId fid);
    void onMapCurrentLayerChanged(QgsMapLayer* layer);
    void onCanvasLayersChanged();
    void onLayerModified();
    void onUndoStackIndexChanged(int idx);

private:
    void bindLayer(QgsVectorLayer* layer);
    void unbindLayer(QgsVectorLayer* layer);
    void detachAllLayers();
    void syncWatchedLayers();
    QList<QgsVectorLayer*> modifiedLayers() const;
    bool isLayerOnCanvas(QgsVectorLayer* layer) const;
    bool isEntryUndoable(QgsVectorLayer* layer) const;
    bool isEntryRedoable(QgsVectorLayer* layer) const;
    QgsVectorLayer* findLayerForUndoStack(QUndoStack* stack) const;
    void pruneEntriesForLayer(QgsVectorLayer* layer);
    void clearMapHistory();

    QPointer<TNMapWidget> m_map;
    QList<QPointer<QgsVectorLayer>> m_watchedLayers;
    QVector<QPointer<QgsVectorLayer>> m_mapUndo;
    QVector<QPointer<QgsVectorLayer>> m_mapRedo;
    QHash<QgsVectorLayer*, int> m_layerUndoIndex;
    bool m_applyingMapUndoRedo = false;
};

#endif // TNGISEDITSTACKMANAGER_H
