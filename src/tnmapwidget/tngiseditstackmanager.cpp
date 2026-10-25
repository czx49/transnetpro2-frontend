#include "tngiseditstackmanager.h"

#include "tnmapwidget.h"

#include "../casectrls/casewfseditcommit.h"
#include "../httpapis/appsession.h"

#include <QUndoStack>
#include <QSet>

#include <algorithm>

#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>

TNGisEditStackManager::TNGisEditStackManager(QObject* parent)
    : QObject(parent)
{
}

TNGisEditStackManager::~TNGisEditStackManager()
{
    attachMap(nullptr);
}

void TNGisEditStackManager::forgetLayer(QgsVectorLayer* layer)
{
    unbindLayer(layer);
}

void TNGisEditStackManager::detachAllLayers()
{
    const QList<QPointer<QgsVectorLayer>> layers = m_watchedLayers;
    m_watchedLayers.clear();
    for (const QPointer<QgsVectorLayer>& layer : layers) {
        if (!layer) {
            continue;
        }
        if (QUndoStack* stack = layer->undoStack()) {
            disconnect(stack, nullptr, this, nullptr);
        }
        disconnect(layer.data(), nullptr, this, nullptr);
    }
    m_layerUndoIndex.clear();
}

void TNGisEditStackManager::attachMap(TNMapWidget* map)
{
    if (m_map.data() == map) {
        syncWatchedLayers();
        return;
    }

    if (m_map) {
        disconnect(m_map, nullptr, this, nullptr);
        detachAllLayers();
        clearMapHistory();
    }

    m_map = map;
    if (!m_map) {
        return;
    }

    connect(m_map, &TNMapWidget::featureAdded, this, &TNGisEditStackManager::onMapFeatureAdded);
    connect(m_map, &TNMapWidget::featureEdited, this, &TNGisEditStackManager::onMapFeatureEdited);
    connect(m_map, &TNMapWidget::featureDeleted, this, &TNGisEditStackManager::onMapFeatureDeleted);
    connect(m_map, &TNMapWidget::currentLayerChanged, this, &TNGisEditStackManager::onMapCurrentLayerChanged);

    if (QgsMapCanvas* canvas = m_map->canvas()) {
        connect(canvas, &QgsMapCanvas::layersChanged, this, &TNGisEditStackManager::onCanvasLayersChanged);
    }

    syncWatchedLayers();
}

void TNGisEditStackManager::notifyEditChanged()
{
    emit editStackChanged();
}

bool TNGisEditStackManager::hasUnsavedChanges() const
{
    return !modifiedLayers().isEmpty();
}

QStringList TNGisEditStackManager::modifiedLayerNames() const
{
    QStringList names;
    for (QgsVectorLayer* layer : modifiedLayers()) {
        if (layer) {
            names.append(layer->name());
        }
    }
    return names;
}

bool TNGisEditStackManager::saveAll(QString* summaryOut)
{
    const QList<QgsVectorLayer*> layers = modifiedLayers();
    if (layers.isEmpty()) {
        const QString summary = QString::fromUtf8(u8"当前没有待保存的编辑内容。");
        if (summaryOut) {
            *summaryOut = summary;
        }
        emit saveCompleted(false, summary);
        emit editStackChanged();
        return false;
    }

    QStringList savedNames;
    QStringList failedNames;
    const QString authToken = AppSession::instance().token();
    for (QgsVectorLayer* layer : layers) {
        if (!layer) {
            continue;
        }

        bool layerOk = false;
        QString layerError;
        if (CaseWfsEditCommit::isPanelWfsLayer(layer)) {
            layerOk = CaseWfsEditCommit::commitLayerEdits(layer, authToken, &layerError);
        } else {
            layerOk = layer->commitChanges();
            if (!layerOk) {
                layerError = layer->commitErrors().join(QStringLiteral("; "));
            }
        }

        if (layerOk) {
            savedNames.append(layer->name());
            pruneEntriesForLayer(layer);
        } else {
            failedNames.append(QString::fromUtf8(u8"%1（%2）").arg(layer->name(), layerError));
        }
    }

    const bool ok = failedNames.isEmpty();
    if (ok) {
        clearMapHistory();
    }

    QString summary;
    if (ok) {
        summary = QString::fromUtf8(u8"已成功保存 %1 个图层的编辑内容。").arg(savedNames.size());
    } else if (!savedNames.isEmpty()) {
        summary = QString::fromUtf8(u8"部分保存成功：%1；失败：%2")
                      .arg(savedNames.join(QStringLiteral("、")),
                           failedNames.join(QStringLiteral("、")));
    } else {
        summary = QString::fromUtf8(u8"保存失败：%1").arg(failedNames.join(QStringLiteral("、")));
    }

    if (summaryOut) {
        *summaryOut = summary;
    }
    emit saveCompleted(ok, summary);
    emit editStackChanged();
    return ok;
}

bool TNGisEditStackManager::saveLayer(QgsVectorLayer* layer, QString* summaryOut)
{
    if (!layer) {
        const QString summary = QString::fromUtf8(u8"图层无效，无法保存。");
        if (summaryOut) {
            *summaryOut = summary;
        }
        emit saveCompleted(false, summary);
        emit editStackChanged();
        return false;
    }

    if (!layer->isEditable() || !layer->isModified()) {
        const QString summary = QString::fromUtf8(u8"当前图层没有待保存的编辑内容。");
        if (summaryOut) {
            *summaryOut = summary;
        }
        emit saveCompleted(false, summary);
        emit editStackChanged();
        return false;
    }

    bool layerOk = false;
    QString layerError;
    const QString authToken = AppSession::instance().token();
    if (CaseWfsEditCommit::isPanelWfsLayer(layer)) {
        layerOk = CaseWfsEditCommit::commitLayerEdits(layer, authToken, &layerError);
    } else {
        layerOk = layer->commitChanges();
        if (!layerOk) {
            layerError = layer->commitErrors().join(QStringLiteral("; "));
        }
    }

    if (layerOk) {
        pruneEntriesForLayer(layer);
    }

    QString summary;
    if (layerOk) {
        summary = QString::fromUtf8(u8"已成功保存图层「%1」的编辑内容。").arg(layer->name());
    } else {
        summary = QString::fromUtf8(u8"保存失败：%1（%2）").arg(layer->name(), layerError);
    }

    if (summaryOut) {
        *summaryOut = summary;
    }
    emit saveCompleted(layerOk, summary);
    emit editStackChanged();
    return layerOk;
}

void TNGisEditStackManager::discardAll()
{
    if (!m_map || !m_map->canvas()) {
        return;
    }

    const QList<QgsMapLayer*> layers = m_map->canvas()->layers();
    for (QgsMapLayer* layer : layers) {
        auto* vl = qobject_cast<QgsVectorLayer*>(layer);
        if (vl && vl->isEditable()) {
            vl->rollBack();
        }
    }

    clearMapHistory();
    emit editStackChanged();
}

bool TNGisEditStackManager::canUndo() const
{
    for (int i = m_mapUndo.size() - 1; i >= 0; --i) {
        if (isEntryUndoable(m_mapUndo.at(i).data())) {
            return true;
        }
    }
    return false;
}

bool TNGisEditStackManager::canRedo() const
{
    for (int i = m_mapRedo.size() - 1; i >= 0; --i) {
        if (isEntryRedoable(m_mapRedo.at(i).data())) {
            return true;
        }
    }
    return false;
}

void TNGisEditStackManager::undo()
{
    while (!m_mapUndo.isEmpty()) {
        QgsVectorLayer* layer = m_mapUndo.takeLast().data();
        if (!isEntryUndoable(layer)) {
            continue;
        }

        QUndoStack* stack = layer->undoStack();
        if (!stack) {
            continue;
        }

        m_applyingMapUndoRedo = true;
        stack->undo();
        m_layerUndoIndex.insert(layer, stack->index());
        m_applyingMapUndoRedo = false;
        m_mapRedo.append(layer);
        emit editStackChanged();
        return;
    }

    emit editStackChanged();
}

void TNGisEditStackManager::redo()
{
    while (!m_mapRedo.isEmpty()) {
        QgsVectorLayer* layer = m_mapRedo.takeLast().data();
        if (!isEntryRedoable(layer)) {
            continue;
        }

        QUndoStack* stack = layer->undoStack();
        if (!stack) {
            continue;
        }

        m_applyingMapUndoRedo = true;
        stack->redo();
        m_layerUndoIndex.insert(layer, stack->index());
        m_applyingMapUndoRedo = false;
        m_mapUndo.append(layer);
        emit editStackChanged();
        return;
    }

    emit editStackChanged();
}

QString TNGisEditStackManager::peekUndoLayerName() const
{
    for (int i = m_mapUndo.size() - 1; i >= 0; --i) {
        QgsVectorLayer* layer = m_mapUndo.at(i).data();
        if (isEntryUndoable(layer)) {
            return layer->name();
        }
    }
    return QString();
}

void TNGisEditStackManager::onMapFeatureAdded(QgsVectorLayer* layer, QgsFeatureId)
{
    if (layer) {
        bindLayer(layer);
    }
    emit editStackChanged();
}

void TNGisEditStackManager::onMapFeatureEdited(QgsVectorLayer* layer, QgsFeatureId)
{
    if (layer) {
        bindLayer(layer);
    }
    emit editStackChanged();
}

void TNGisEditStackManager::onMapFeatureDeleted(QgsVectorLayer* layer, QgsFeatureId)
{
    if (layer) {
        bindLayer(layer);
    }
    emit editStackChanged();
}

void TNGisEditStackManager::onMapCurrentLayerChanged(QgsMapLayer* layer)
{
    if (auto* vl = qobject_cast<QgsVectorLayer*>(layer)) {
        bindLayer(vl);
    }
    emit editStackChanged();
}

void TNGisEditStackManager::onCanvasLayersChanged()
{
    syncWatchedLayers();
    emit editStackChanged();
}

void TNGisEditStackManager::onLayerModified()
{
    emit editStackChanged();
}

void TNGisEditStackManager::onUndoStackIndexChanged(int idx)
{
    if (m_applyingMapUndoRedo) {
        return;
    }

    QUndoStack* stack = qobject_cast<QUndoStack*>(sender());
    if (!stack) {
        return;
    }

    QgsVectorLayer* layer = findLayerForUndoStack(stack);
    if (!layer) {
        return;
    }

    const int prev = m_layerUndoIndex.value(layer, 0);
    m_layerUndoIndex.insert(layer, idx);

    if (idx == 0 && !stack->canUndo()) {
        pruneEntriesForLayer(layer);
        emit editStackChanged();
        return;
    }

    if (idx > prev) {
        const int added = idx - prev;
        for (int i = 0; i < added; ++i) {
            m_mapUndo.append(layer);
        }
        m_mapRedo.clear();
        emit editStackChanged();
    }
}

void TNGisEditStackManager::bindLayer(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    for (const QPointer<QgsVectorLayer>& watched : m_watchedLayers) {
        if (watched.data() == layer) {
            return;
        }
    }

    m_watchedLayers.append(layer);
    connect(layer, &QgsVectorLayer::editingStarted, this, &TNGisEditStackManager::onLayerModified, Qt::UniqueConnection);
    connect(layer, &QgsMapLayer::layerModified, this, &TNGisEditStackManager::onLayerModified, Qt::UniqueConnection);
    connect(layer, &QgsVectorLayer::editingStopped, this, &TNGisEditStackManager::onLayerModified, Qt::UniqueConnection);

    if (QUndoStack* stack = layer->undoStack()) {
        connect(stack, &QUndoStack::indexChanged, this, &TNGisEditStackManager::onUndoStackIndexChanged,
                Qt::UniqueConnection);
        m_layerUndoIndex.insert(layer, stack->index());
    }
}

void TNGisEditStackManager::unbindLayer(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }

    if (QUndoStack* stack = layer->undoStack()) {
        disconnect(stack, nullptr, this, nullptr);
    }
    disconnect(layer, nullptr, this, nullptr);
    m_watchedLayers.removeAll(layer);
    m_layerUndoIndex.remove(layer);
    pruneEntriesForLayer(layer);
}

void TNGisEditStackManager::syncWatchedLayers()
{
    if (!m_map || !m_map->canvas()) {
        return;
    }

    QSet<QgsVectorLayer*> currentLayers;
    const QList<QgsMapLayer*> layers = m_map->canvas()->layers();
    for (QgsMapLayer* layer : layers) {
        if (auto* vl = qobject_cast<QgsVectorLayer*>(layer)) {
            currentLayers.insert(vl);
            bindLayer(vl);
        }
    }

    QList<QPointer<QgsVectorLayer>> stale;
    for (const QPointer<QgsVectorLayer>& watched : m_watchedLayers) {
        if (!watched || !currentLayers.contains(watched.data())) {
            stale.append(watched);
        }
    }
    for (const QPointer<QgsVectorLayer>& watched : stale) {
        if (watched) {
            unbindLayer(watched.data());
        }
    }
    m_watchedLayers.erase(std::remove_if(m_watchedLayers.begin(), m_watchedLayers.end(),
                                         [](const QPointer<QgsVectorLayer>& watched) {
                                             return watched.isNull();
                                         }),
                          m_watchedLayers.end());
    pruneEntriesForLayer(nullptr);
}

QList<QgsVectorLayer*> TNGisEditStackManager::modifiedLayers() const
{
    QList<QgsVectorLayer*> result;
    if (!m_map || !m_map->canvas()) {
        return result;
    }

    const QList<QgsMapLayer*> layers = m_map->canvas()->layers();
    for (QgsMapLayer* layer : layers) {
        auto* vl = qobject_cast<QgsVectorLayer*>(layer);
        if (vl && vl->isEditable() && vl->isModified()) {
            result.append(vl);
        }
    }
    return result;
}

bool TNGisEditStackManager::isLayerOnCanvas(QgsVectorLayer* layer) const
{
    if (!layer || !m_map || !m_map->canvas()) {
        return false;
    }
    return m_map->canvas()->layers().contains(layer);
}

bool TNGisEditStackManager::isEntryUndoable(QgsVectorLayer* layer) const
{
    if (!layer || !layer->isValid() || !isLayerOnCanvas(layer) || !layer->isEditable()) {
        return false;
    }
    QUndoStack* stack = layer->undoStack();
    return stack && stack->canUndo();
}

bool TNGisEditStackManager::isEntryRedoable(QgsVectorLayer* layer) const
{
    if (!layer || !layer->isValid() || !isLayerOnCanvas(layer) || !layer->isEditable()) {
        return false;
    }
    QUndoStack* stack = layer->undoStack();
    return stack && stack->canRedo();
}

QgsVectorLayer* TNGisEditStackManager::findLayerForUndoStack(QUndoStack* stack) const
{
    if (!stack) {
        return nullptr;
    }
    for (const QPointer<QgsVectorLayer>& watched : m_watchedLayers) {
        QgsVectorLayer* layer = watched.data();
        if (layer && layer->undoStack() == stack) {
            return layer;
        }
    }
    return nullptr;
}

void TNGisEditStackManager::pruneEntriesForLayer(QgsVectorLayer* layer)
{
    auto shouldDrop = [layer](const QPointer<QgsVectorLayer>& entry) {
        return entry.isNull() || entry.data() == layer;
    };
    m_mapUndo.erase(std::remove_if(m_mapUndo.begin(), m_mapUndo.end(), shouldDrop), m_mapUndo.end());
    m_mapRedo.erase(std::remove_if(m_mapRedo.begin(), m_mapRedo.end(), shouldDrop), m_mapRedo.end());
}

void TNGisEditStackManager::clearMapHistory()
{
    m_mapUndo.clear();
    m_mapRedo.clear();
}
