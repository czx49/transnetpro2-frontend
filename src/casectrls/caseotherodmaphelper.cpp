/*****************************************************************//**
 * \file   caseotherodmaphelper.cpp
 * \brief  other_od 图层地图加载与 demand 分级渲染
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "caseotherodmaphelper.h"

#include "casewfsmapcontroller.h"
#include "layerstylefactory.h"
#include "tnmapwidget.h"

#include <QPointer>
#include <QTimer>

#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>

namespace {

void applyDemandStyleWhenReady(QgsVectorLayer* layer,
                             TNMapWidget* mapWidget,
                             const std::function<void(bool success, const QString& error)>& finish)
{
    if (!mapWidget || !layer || !layer->isValid()) {
        finish(false, QString::fromUtf8(u8"OD 图层加载失败，请重试。"));
        return;
    }

    QgsMapCanvas* canvas = mapWidget->canvas();
    QPointer<TNMapWidget> mapGuard(mapWidget);
    QPointer<QgsVectorLayer> layerGuard(layer);
    QPointer<QgsMapCanvas> canvasGuard(canvas);
    auto started = std::make_shared<bool>(false);
    const auto startApply = [layerGuard, canvasGuard, finish, started]() {
        if (*started || !layerGuard) {
            return;
        }
        *started = true;
        if (LayerStyleFactory::tryApplyOtherOdDemandStyle(layerGuard.data(), canvasGuard.data())) {
            finish(true, QString());
            return;
        }
        LayerStyleFactory::applyOtherOdDemandStyle(layerGuard.data(),
                                                   canvasGuard.data(),
                                                   [finish](bool styled) {
                                                       if (styled) {
                                                           finish(true, QString());
                                                           return;
                                                       }
                                                       finish(false, QString::fromUtf8(u8"应用 OD 渲染样式失败。"));
                                                   },
                                                   true);
    };

    QObject::connect(layer,
                     &QgsVectorLayer::dataChanged,
                     mapWidget,
                     [startApply]() {
                         startApply();
                     });
    QTimer::singleShot(800, layer, startApply);
}

} // namespace

namespace CaseOtherOdMapHelper {

void loadOrReloadAndApplyDemandStyle(
    TNMapWidget* mapWidget,
    const QString& wfsTypeName,
    const std::function<bool()>& triggerLoad,
    const std::function<void(bool success, const QString& error)>& done)
{
    const auto finish = [done](bool ok, const QString& err) {
        if (done) {
            done(ok, err);
        }
    };

    if (!mapWidget) {
        finish(false, QString::fromUtf8(u8"地图未就绪。"));
        return;
    }

    const QString typeName = wfsTypeName.trimmed();
    if (typeName.isEmpty()) {
        finish(false, QString::fromUtf8(u8"OD 图层缺少 WFS 类型名。"));
        return;
    }

    const QString uriStr = CaseWfsMap::buildWfsDataSourceUri(typeName);
    if (uriStr.isEmpty()) {
        finish(false, QString::fromUtf8(u8"OD 图层 WFS 数据源无效。"));
        return;
    }

    QPointer<TNMapWidget> mapGuard(mapWidget);
    const auto applyWhenReady = [finish, mapGuard](QgsVectorLayer* layer) {
        if (!mapGuard) {
            finish(false, QString::fromUtf8(u8"地图未就绪。"));
            return;
        }
        applyDemandStyleWhenReady(layer, mapGuard.data(), finish);
    };

    if (auto* existingLayer = qobject_cast<QgsVectorLayer*>(mapWidget->getLayer(uriStr))) {
        if (!existingLayer->isValid()) {
            finish(false, QString::fromUtf8(u8"OD 图层加载失败，请重试。"));
            return;
        }
        CaseWfsMap::refreshWfsCapabilitiesCache();
        existingLayer->reload();
        applyWhenReady(existingLayer);
        return;
    }

    if (!triggerLoad) {
        finish(false, QString::fromUtf8(u8"OD 图层加载触发未配置。"));
        return;
    }

    CaseWfsMap::setLoadQueueIdleCallback([mapGuard, uriStr, finish, applyWhenReady]() {
        CaseWfsMap::clearLoadQueueIdleCallback();
        if (!mapGuard) {
            finish(false, QString::fromUtf8(u8"地图未就绪。"));
            return;
        }

        auto* layer = qobject_cast<QgsVectorLayer*>(mapGuard->getLayer(uriStr));
        if (!layer || !layer->isValid()) {
            finish(false, QString::fromUtf8(u8"OD 图层加载失败，请重试。"));
            return;
        }
        applyWhenReady(layer);
    });

    if (!triggerLoad()) {
        CaseWfsMap::clearLoadQueueIdleCallback();
        finish(false, QString::fromUtf8(u8"未找到 OD 图层节点，请确认图层树配置。"));
    }
}

} // namespace CaseOtherOdMapHelper
