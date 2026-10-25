/*****************************************************************//**
 * \file   casecomparerenderhelper.cpp
 * \brief  双屏对比模式下路段渲染样式（与机动车工作台分析样式一致）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casecomparerenderhelper.h"

#include "casetreemodel.h"
#include "casewaylayerhelper.h"
#include "casewaylayerstyleapply.h"
#include "casewfsmapcontroller.h"
#include "../tnmapwidget/tnmapwidget.h"

#include <QHash>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <memory>

#include <qgsvectorlayer.h>

namespace CaseCompareRender
{

namespace
{

const QString kRoadWayLayerVal = QStringLiteral("road_way");

bool isBaseCase(const QList<CaseItem>& caseList, int caseId)
{
    for (const CaseItem& item : caseList) {
        if (item.id == caseId) {
            return item.isBase == 1;
        }
    }
    return false;
}

WayLayerAnalysisStyle toWayLayerStyle(CompareRenderStyle style)
{
    switch (style) {
        case CompareRenderStyle::TrafficFlow:
            return WayLayerAnalysisStyle::TrafficFlow;
        case CompareRenderStyle::Saturation:
            return WayLayerAnalysisStyle::TrafficSaturation;
        case CompareRenderStyle::None:
        default:
            return WayLayerAnalysisStyle::None;
    }
}

struct CompareApplySlot {
    int generation = 0;
    bool applying = false;
    bool hasPending = false;
    int pendingCaseId = 0;
    CaseTreeModel* pendingModel = nullptr;
    QList<CaseItem> pendingCaseList;
    CompareRenderStyle pendingStyle = CompareRenderStyle::None;
    std::function<void(QgsVectorLayer*, bool applied)> pendingOnDone;
};

QHash<TNMapWidget*, CompareApplySlot> g_compareApplySlots;

void drainPendingCompareApply(TNMapWidget* map)
{
    auto it = g_compareApplySlots.find(map);
    if (it == g_compareApplySlots.end() || !it->hasPending) {
        return;
    }

    const int caseId = it->pendingCaseId;
    CaseTreeModel* model = it->pendingModel;
    const QList<CaseItem> caseList = it->pendingCaseList;
    const CompareRenderStyle style = it->pendingStyle;
    const std::function<void(QgsVectorLayer*, bool applied)> onDone = it->pendingOnDone;
    it->hasPending = false;
    it->pendingOnDone = {};

    QPointer<TNMapWidget> mapGuard(map);
    QTimer::singleShot(0, map, [mapGuard, caseId, model, caseList, style, onDone]() {
        if (!mapGuard || !model) {
            return;
        }
        applyToMap(mapGuard.data(), caseId, model, caseList, style, onDone);
    });
}

} // namespace

CompareRenderStyle styleFromComboIndex(int index)
{
    switch (index) {
        case 1:
            return CompareRenderStyle::Saturation;
        case 2:
            return CompareRenderStyle::TrafficFlow;
        case 0:
        default:
            return CompareRenderStyle::None;
    }
}

bool ensureRoadWayLayer(TNMapWidget* map, int caseId, CaseTreeModel* model, QString* wfsUriOut,
                        QString* errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    if (wfsUriOut) {
        wfsUriOut->clear();
    }

    if (!map || caseId <= 0 || !model) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"地图或方案未就绪。");
        }
        return false;
    }

    QVariantMap meta;
    if (!model->setLayerCheckedByVal(caseId, kRoadWayLayerVal, true, &meta)) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"未找到图层 %1，请确认方案菜单配置。").arg(kRoadWayLayerVal);
        }
        return false;
    }

    const QString typeName = meta.value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层缺少 WFS 类型名。");
        }
        return false;
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(typeName);
    if (wfsUriOut) {
        *wfsUriOut = uri;
    }

    auto* vectorLayer = qobject_cast<QgsVectorLayer*>(map->getLayer(uri));
    return vectorLayer && vectorLayer->isValid();
}

bool findRoadWayLayerUri(int caseId, CaseTreeModel* model, QString* wfsUriOut)
{
    if (wfsUriOut) {
        wfsUriOut->clear();
    }
    if (caseId <= 0 || !model) {
        return false;
    }

    QVariantMap meta;
    if (!model->findLayerUserDataByVal(caseId, kRoadWayLayerVal, &meta)) {
        return false;
    }

    const QString typeName = meta.value(QStringLiteral("typename")).toString().trimmed();
    if (typeName.isEmpty()) {
        return false;
    }

    const QString uri = CaseWfsMap::buildWfsDataSourceUri(typeName);
    if (wfsUriOut) {
        *wfsUriOut = uri;
    }
    return true;
}

void applyToMap(TNMapWidget* map, int caseId, CaseTreeModel* model, const QList<CaseItem>& caseList,
                CompareRenderStyle style, const std::function<void(QgsVectorLayer*, bool applied)>& onDone)
{
    if (!map || caseId <= 0 || !model) {
        if (onDone) {
            onDone(nullptr, false);
        }
        return;
    }

    CompareApplySlot& slot = g_compareApplySlots[map];
    if (slot.applying) {
        slot.pendingCaseId = caseId;
        slot.pendingModel = model;
        slot.pendingCaseList = caseList;
        slot.pendingStyle = style;
        slot.pendingOnDone = onDone;
        slot.hasPending = true;
        return;
    }

    const bool baseCase = isBaseCase(caseList, caseId);
    const WayLayerAnalysisStyle wayStyle = toWayLayerStyle(style);

    QString uri;
    if (style == CompareRenderStyle::None) {
        if (!findRoadWayLayerUri(caseId, model, &uri)) {
            CaseWfsMap::resetPanelWfsDefaultSymbols(map);
            if (map->canvas()) {
                map->canvas()->refresh();
            }
            if (onDone) {
                onDone(nullptr, true);
            }
            return;
        }
    } else {
        QString error;
        ensureRoadWayLayer(map, caseId, model, &uri, &error);
        if (uri.isEmpty()) {
            if (onDone) {
                onDone(nullptr, false);
            }
            return;
        }
    }

    ++slot.generation;
    const int generation = slot.generation;

    QPointer<TNMapWidget> mapGuard(map);
    const auto appliedFlag = std::make_shared<bool>(false);
    const auto busy = std::make_shared<bool>(false);

    const auto applyOnce = [mapGuard, uri, wayStyle, style, baseCase, appliedFlag, busy, onDone,
                            generation]() -> bool {
        if (!mapGuard) {
            return false;
        }
        {
            const auto slotIt = g_compareApplySlots.constFind(mapGuard.data());
            if (slotIt == g_compareApplySlots.cend() || generation != slotIt->generation) {
                return true;
            }
        }
        if (*appliedFlag) {
            return true;
        }
        if (*busy) {
            return false;
        }

        auto* vectorLayer = qobject_cast<QgsVectorLayer*>(mapGuard->getLayer(uri));
        if (!vectorLayer || !vectorLayer->isValid()) {
            return false;
        }

        *busy = true;
        g_compareApplySlots[mapGuard.data()].applying = true;
        struct ApplyGuard {
            QPointer<TNMapWidget> map;
            std::shared_ptr<bool> busyFlag;
            std::shared_ptr<bool> appliedFlag;
            ~ApplyGuard()
            {
                if (busyFlag) {
                    *busyFlag = false;
                }
                if (!map) {
                    return;
                }
                auto it = g_compareApplySlots.find(map.data());
                if (it == g_compareApplySlots.end()) {
                    return;
                }
                it->applying = false;
                if (it->hasPending) {
                    if (appliedFlag) {
                        *appliedFlag = true;
                    }
                    drainPendingCompareApply(map.data());
                }
            }
        } applyGuard{mapGuard, busy, appliedFlag};

        CaseWayLayer::DefaultTypeFilterSuspendGuard typeFilterGuard;

        if (style == CompareRenderStyle::None) {
            auto* layerOnMap = qobject_cast<QgsVectorLayer*>(mapGuard->getLayer(uri));
            if (layerOnMap && layerOnMap->isValid()) {
                CaseWayLayerStyle::restoreDefaultStyle(mapGuard, layerOnMap, baseCase);
            } else {
                CaseWfsMap::resetPanelWfsDefaultSymbols(mapGuard);
            }
            if (mapGuard && g_compareApplySlots.value(mapGuard.data()).hasPending) {
                CaseWfsMap::clearLoadQueueIdleCallback();
                return true;
            }
            vectorLayer = qobject_cast<QgsVectorLayer*>(mapGuard->getLayer(uri));
        } else {
            CaseWfsMap::resetPanelWfsDefaultSymbols(mapGuard);
            if (mapGuard && g_compareApplySlots.value(mapGuard.data()).hasPending) {
                CaseWfsMap::clearLoadQueueIdleCallback();
                return true;
            }
            vectorLayer = qobject_cast<QgsVectorLayer*>(mapGuard->getLayer(uri));
            if (!vectorLayer) {
                return false;
            }
            CaseWayLayerStyle::applyAnalysisStyle(vectorLayer, wayStyle, QStringLiteral("RdYlGn"), false,
                                                 mapGuard.data());
            if (mapGuard && g_compareApplySlots.value(mapGuard.data()).hasPending) {
                CaseWfsMap::clearLoadQueueIdleCallback();
                return true;
            }
        }

        if (mapGuard->canvas()) {
            mapGuard->canvas()->refresh();
        }

        *appliedFlag = true;
        CaseWfsMap::clearLoadQueueIdleCallback();
        if (onDone) {
            onDone(vectorLayer, true);
        }
        return true;
    };

    if (applyOnce()) {
        return;
    }

    const int retryDelays[] = {150, 400, 800, 1500};
    for (int delay : retryDelays) {
        QTimer::singleShot(delay, map, [applyOnce]() { applyOnce(); });
    }

    CaseWfsMap::setLoadQueueIdleCallback([applyOnce]() { applyOnce(); });
}

} // namespace CaseCompareRender
