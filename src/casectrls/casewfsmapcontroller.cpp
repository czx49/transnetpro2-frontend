/*****************************************************************//**
 * \file   casewfsmapcontroller.cpp
 * \brief  WFS 2.0 矢量图层：QgsVectorLayer + provider WFS
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "casewfsmapcontroller.h"

#include "../casectrls/casewaylayerhelper.h"
#include "../httpapis/httpapidefines.h"
#include "../publicctrls/messagedialog.h"
#include "../tnmapwidget/layerstylefactory.h"
#include "../tnmapwidget/tnlayerfilterapply.h"
#include "../tnmapwidget/tnlayerfiltertypes.h"
#include "../tnmapwidget/tnmapwidget.h"
#include "../uis/loadingdialog.h"

#include <QAbstractNetworkCache>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QIODevice>
#include <QNetworkCacheMetaData>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>

#include <functional>

#include <qgsdatasourceuri.h>
#include <qgsmapcanvas.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsrulebasedrenderer.h>
#include <qgssettings.h>
#include <qgsvectorlayer.h>

namespace {

const QString kPanelWfsProp = QStringLiteral("tnp_panel_wfs");
const QString kWarmupTypeName = QStringLiteral("__tnp_wfs_caps_warm__");
const int kWfsMemoryCacheDelaySeconds = 600;
const int kCapabilitiesWaitTimeoutMs = 60000;
const int kSchemaWarmIntervalMs = 150;
const int kSchemaWarmUserIdleMs = 300;
const int kSchemaWarmPauseRetryMs = 50;
/** 首刷未完成时暂停 schema warm 的最长等待，防止标志卡住 */
const int kSuspendSchemaWarmFirstPaintTimeoutMs = 60000;

QString normalizedWfsRootUrl()
{
    QString root = fullURL(QStringLiteral("/wfs")).trimmed();
    while (root.endsWith(QLatin1Char('/'))) {
        root.chop(1);
    }
    return root;
}

QUrl capabilitiesRequestUrl()
{
    // Match QGIS QgsWfsCapabilities::requestUrl: baseURL(true) adds SERVICE=WFS first,
    // then REQUEST + VERSION. NAM PreferCache / cache keys must use this exact URL.
    QUrl url(normalizedWfsRootUrl());
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WFS"));
    query.addQueryItem(QStringLiteral("REQUEST"), QStringLiteral("GetCapabilities"));
    query.addQueryItem(QStringLiteral("VERSION"), QStringLiteral("2.0.0"));
    url.setQuery(query);
    return url;
}

/** Formal load URI: url + typename + version + srsname; no skipInitialGetFeature (desktop QGIS). */
QString buildFormalWfsUri(const QString& typeName)
{
    return CaseWfsMap::buildWfsDataSourceUri(typeName);
}

/** Schema warm only: same base params + skipInitialGetFeature to prefill DFT without COUNT=1. */
QString buildSchemaWarmWfsUri(const QString& typeName)
{
    const QString trimmed = typeName.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QgsDataSourceUri uri;
    uri.setParam(QStringLiteral("url"), normalizedWfsRootUrl());
    uri.setParam(QStringLiteral("typename"), trimmed);
    uri.setParam(QStringLiteral("version"), QStringLiteral("2.0.0"));
    uri.setParam(QStringLiteral("srsname"), QStringLiteral("EPSG:4326"));
    uri.setParam(QStringLiteral("skipInitialGetFeature"), QStringLiteral("true"));
    return uri.uri(false);
}

struct WfsLayerLoadItem {
    QPointer<TNMapWidget> map;
    QString displayName;
    QString typeName;
    int level = 0;
    bool visible = true;
    QString layerVal;
    bool isBaseCase = false;
    std::function<void()> rollbackCheckState;
    quint64 loadGeneration = 0;
};

quint64 g_loadGeneration = 0;
QQueue<WfsLayerLoadItem> g_wfsLoadQueue;
bool g_wfsLoadProcessing = false;
bool g_batchActive = false;
bool g_batchSessionStarted = false;
int g_totalInSession = 0;
int g_processedInSession = 0;
QStringList g_failedNames;
QPointer<LoadingDialog> g_loadingDialog;
QPointer<TNMapWidget> g_batchMapWidget;
QList<QPointer<QgsMapLayer>> g_batchLoadedLayers;
std::function<void()> g_loadQueueIdleCallback;
std::function<void(TNMapWidget*)> g_batchLoadFailureReconcileCallback;

enum class CapsWarmPhase {
    Idle,
    Downloading,
    Applying
};

CapsWarmPhase g_capsPhase = CapsWarmPhase::Idle;
bool g_capsWarmSucceeded = false;
bool g_capsRestartRequested = false;
bool g_capsRestartForce = false;
QPointer<QNetworkReply> g_capsReply;
QElapsedTimer g_capsWarmTimer;
bool g_pumpWhenCapsReady = false;
qint64 g_capsWaitStartedMs = 0;
std::function<void(bool)> g_capsFinishedCallback;

QList<QString> g_schemaWarmQueue;
QSet<QString> g_schemaWarmQueued;
QSet<QString> g_schemaWarmDone;
bool g_schemaWarmProcessing = false;
bool g_schemaWarmPumpScheduled = false;
qint64 g_lastUserInputMs = 0;
QPointer<QObject> g_schemaWarmInputWatcher;
bool g_suspendSchemaWarmUntilFirstPaint = false;
QList<QPointer<QgsVectorLayer>> g_deferredClientTypeFilterLayers;
bool g_wfsShuttingDown = false;

void invokeLoadQueueIdleCallbackIfNeeded();
void pumpWfsLayerLoadQueue();
void finishBatchSession();
void beginBatchSessionIfNeeded();
void requestPumpWhenCapabilitiesReady();
void notifyCapabilitiesBecameIdle();
void invokeCapsFinishedCallback();
bool layerNeedsNewWfsCreate(const WfsLayerLoadItem& item);
bool applyWfsLayerInternal(TNMapWidget* map, const QString& displayName, const QString& typeName, int level,
                           bool visible, const QString& layerVal, bool isBaseCase, bool allowRetry);
void scheduleCapabilitiesWarm(bool forceRefresh);
void startCapabilitiesDownload(bool forceRefresh);
void onCapabilitiesDownloadFinished(bool ok, const QByteArray& body, const QString& error);
void refreshWfsCapabilitiesCacheBlocking();
bool removeCapabilitiesUrlFromNamCache();
bool insertCapabilitiesBodyIntoNamCache(const QUrl& capsUrl, const QByteArray& body);
void populateParsedCapabilitiesCache();
void cancelSchemaWarmForTypeName(const QString& typeName);
void ensureSchemaWarmInputWatcher();
void scheduleSchemaWarmPump(int delayMs = kSchemaWarmIntervalMs);
void pumpSchemaWarmQueue();
bool shouldPauseSchemaWarm();
void finishFirstPaintGate();
void armSuspendSchemaWarmUntilFirstPaint(TNMapWidget* map);

class SchemaWarmInputWatcher : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!event) {
            return QObject::eventFilter(watched, event);
        }
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::KeyPress:
        case QEvent::Wheel:
            g_lastUserInputMs = QDateTime::currentMSecsSinceEpoch();
            break;
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }
};

QString layerDisplayLabel(const WfsLayerLoadItem& item)
{
    const QString display = item.displayName.trimmed();
    return display.isEmpty() ? item.typeName.trimmed() : display;
}

QString formatBatchProgressMessage(int current, int total)
{
    return QString::fromUtf8(u8"正在加载图层 (%1/%2)…").arg(current).arg(total);
}

bool isWfsAttributeOnlyLayer(QgsVectorLayer* layer)
{
    return layer && layer->isValid()
           && layer->providerType().compare(QStringLiteral("WFS"), Qt::CaseInsensitive) == 0
           && layer->geometryType() == Qgis::GeometryType::Null
           && layer->fields().count() > 0;
}

bool isCapabilitiesWarmBusy()
{
    return g_capsPhase != CapsWarmPhase::Idle || !g_capsReply.isNull() || g_capsRestartRequested;
}

bool removeCapabilitiesUrlFromNamCache()
{
    QgsNetworkAccessManager* nam = QgsNetworkAccessManager::instance();
    if (!nam) {
        return false;
    }
    QAbstractNetworkCache* cache = nam->cache();
    if (!cache) {
        return false;
    }
    return cache->remove(capabilitiesRequestUrl());
}

bool insertCapabilitiesBodyIntoNamCache(const QUrl& capsUrl, const QByteArray& body)
{
    // Force-write Caps XML into NAM disk cache so provider PreferCache does not re-download
    // when the server marks GetCapabilities as non-cacheable (Cache-Control: no-store/etc.).
    if (capsUrl.isEmpty() || body.isEmpty()) {
        qWarning() << "CaseWfsMap: namCacheInsert skipped empty url/body"
                    << "urlEmpty" << capsUrl.isEmpty() << "bodyBytes" << body.size();
        return false;
    }

    QgsNetworkAccessManager* nam = QgsNetworkAccessManager::instance();
    if (!nam) {
        qWarning() << "CaseWfsMap: namCacheInsert failed cacheNull=true (no NAM)";
        return false;
    }
    QAbstractNetworkCache* cache = nam->cache();
    if (!cache) {
        qWarning() << "CaseWfsMap: namCacheInsert failed cacheNull=true";
        return false;
    }

    cache->remove(capsUrl);

    // Do not call setRawHeaders/setAttributes: Debug app + Release Qt5Network.dll shares
    // QList/QHash COW data across CRT heaps and breaks in ~QNetworkCacheMetaData under the debugger.
    // prepare() only needs url / saveToDisk / expiration for PreferCache to hit the body.
    QNetworkCacheMetaData meta;
    meta.setUrl(capsUrl);
    meta.setSaveToDisk(true);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    meta.setLastModified(now);
    meta.setExpirationDate(now.addSecs(kWfsMemoryCacheDelaySeconds));

    QIODevice* device = cache->prepare(meta);
    if (!device) {
        qWarning() << "CaseWfsMap: namCacheInsert failed prepare"
                    << "capsUrl" << capsUrl.toString() << "bodyBytes" << body.size();
        return false;
    }
    const qint64 written = device->write(body);
    cache->insert(device);
    if (written != body.size()) {
        qWarning() << "CaseWfsMap: namCacheInsert short write"
                    << "written" << written << "bodyBytes" << body.size();
        return false;
    }
    return true;
}

void populateParsedCapabilitiesCache()
{
    // Use a fake typename so getCachedCapabilities fills gCacheCaps then fails typename
    // lookup before DescribeFeatureType — Caps-only warm, matching QGIS Connect.
    // PreferCache should hit the forced NAM insert from the async warm GET.
    QgsSettings settings;
    const int previousDelay =
        settings.value(QStringLiteral("qgis/wfsMemoryCacheDelay"), kWfsMemoryCacheDelaySeconds).toInt();
    // delay=0 forces this create to ignore any stale gCacheCaps entry and rewrite it.
    settings.setValue(QStringLiteral("qgis/wfsMemoryCacheDelay"), 0);

    const QString uri = buildFormalWfsUri(kWarmupTypeName);
    if (!uri.isEmpty()) {
        auto* layer = new QgsVectorLayer(uri, QStringLiteral("tnp_wfs_caps_warm"), QStringLiteral("WFS"));
        delete layer;
    }

    settings.setValue(QStringLiteral("qgis/wfsMemoryCacheDelay"), previousDelay);
}

void ensureSchemaWarmInputWatcher()
{
    if (g_schemaWarmInputWatcher || !qApp) {
        return;
    }
    auto* watcher = new SchemaWarmInputWatcher(qApp);
    qApp->installEventFilter(watcher);
    g_schemaWarmInputWatcher = watcher;
}

bool shouldPauseSchemaWarm()
{
    if (g_suspendSchemaWarmUntilFirstPaint) {
        return true;
    }
    if (isCapabilitiesWarmBusy() || !g_capsWarmSucceeded || g_wfsLoadProcessing || !g_wfsLoadQueue.isEmpty()) {
        return true;
    }
    if (QGuiApplication::mouseButtons() != Qt::NoButton) {
        return true;
    }
    ensureSchemaWarmInputWatcher();
    if (g_lastUserInputMs > 0) {
        const qint64 idleMs = QDateTime::currentMSecsSinceEpoch() - g_lastUserInputMs;
        if (idleMs < kSchemaWarmUserIdleMs) {
            return true;
        }
    }
    return false;
}

void finishFirstPaintGate()
{
    if (g_wfsShuttingDown) {
        g_deferredClientTypeFilterLayers.clear();
        g_suspendSchemaWarmUntilFirstPaint = false;
        return;
    }
    const QList<QPointer<QgsVectorLayer>> layers = g_deferredClientTypeFilterLayers;
    g_deferredClientTypeFilterLayers.clear();
    for (const QPointer<QgsVectorLayer>& layerGuard : layers) {
        if (!layerGuard) {
            continue;
        }
        CaseWayLayer::scheduleDefaultTypeFilter(layerGuard.data());
    }

    if (!g_suspendSchemaWarmUntilFirstPaint) {
        return;
    }
    g_suspendSchemaWarmUntilFirstPaint = false;
    scheduleSchemaWarmPump(0);
}

void armSuspendSchemaWarmUntilFirstPaint(TNMapWidget* map)
{
    if (!map) {
        finishFirstPaintGate();
        return;
    }

    g_suspendSchemaWarmUntilFirstPaint = true;
    map->armAfterFirstCanvasRefresh([]() {
        finishFirstPaintGate();
    });

    QTimer::singleShot(kSuspendSchemaWarmFirstPaintTimeoutMs, qApp, []() {
        if (!g_suspendSchemaWarmUntilFirstPaint) {
            return;
        }
        qWarning() << "CaseWfsMap: first paint suspend timed out, resuming schema warm";
        finishFirstPaintGate();
    });
}

void cancelSchemaWarmForTypeName(const QString& typeName)
{
    const QString trimmed = typeName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    g_schemaWarmQueued.remove(trimmed);
    g_schemaWarmDone.insert(trimmed);
    g_schemaWarmQueue.removeAll(trimmed);
}

void scheduleSchemaWarmPump(int delayMs)
{
    if (g_wfsShuttingDown) {
        return;
    }
    if (g_schemaWarmPumpScheduled) {
        return;
    }
    g_schemaWarmPumpScheduled = true;
    const int ms = delayMs < 0 ? 0 : delayMs;
    QTimer::singleShot(ms, qApp, []() {
        g_schemaWarmPumpScheduled = false;
        pumpSchemaWarmQueue();
    });
}

void pumpSchemaWarmQueue()
{
    if (g_wfsShuttingDown || g_schemaWarmProcessing) {
        return;
    }
    if (g_schemaWarmQueue.isEmpty()) {
        return;
    }
    if (shouldPauseSchemaWarm()) {
        if (isCapabilitiesWarmBusy()) {
            // Resume via notifyCapabilitiesBecameIdle → scheduleSchemaWarmPump.
            return;
        }
        // User interaction / load in progress: retry after a short yield.
        scheduleSchemaWarmPump(kSchemaWarmPauseRetryMs);
        return;
    }

    const QString typeName = g_schemaWarmQueue.takeFirst();
    g_schemaWarmQueued.remove(typeName);
    if (g_schemaWarmDone.contains(typeName)) {
        scheduleSchemaWarmPump(0);
        return;
    }

    g_schemaWarmProcessing = true;
    QElapsedTimer warmTimer;
    warmTimer.start();

    const QString uri = buildSchemaWarmWfsUri(typeName);
    if (!uri.isEmpty()) {
        auto* layer = new QgsVectorLayer(uri, QStringLiteral("tnp_wfs_schema_warm"), QStringLiteral("WFS"));
        const bool valid = layer->isValid();
        delete layer;
        qInfo() << "CaseWfsMap: schema warm typename" << typeName
                << "valid" << valid << "elapsedMs" << warmTimer.elapsed()
                << "skipInitialGetFeature" << true;
    } else {
        qWarning() << "CaseWfsMap: schema warm skipped empty uri for" << typeName;
    }

    g_schemaWarmDone.insert(typeName);
    g_schemaWarmProcessing = false;
    scheduleSchemaWarmPump(kSchemaWarmIntervalMs);
}

void notifyCapabilitiesBecameIdle()
{
    scheduleSchemaWarmPump();

    if (!g_pumpWhenCapsReady || isCapabilitiesWarmBusy()) {
        return;
    }
    g_pumpWhenCapsReady = false;
    if (g_capsWaitStartedMs > 0) {
        const qint64 capsWaitMs = QDateTime::currentMSecsSinceEpoch() - g_capsWaitStartedMs;
        g_capsWaitStartedMs = 0;
        qInfo() << "CaseWfsMap: caps wait finished capsWaitMs" << capsWaitMs;
    }
    // Defer out of the GetCapabilities finished slot so user layer ctor is not nested in Applying.
    QTimer::singleShot(0, qApp, []() { pumpWfsLayerLoadQueue(); });
}

void invokeCapsFinishedCallback()
{
    if (!g_capsFinishedCallback) {
        return;
    }
    const std::function<void(bool)> callback = g_capsFinishedCallback;
    g_capsFinishedCallback = nullptr;
    const bool succeeded = g_capsWarmSucceeded;
    QTimer::singleShot(0, qApp, [callback, succeeded]() {
        if (callback) {
            callback(succeeded);
        }
    });
}

void requestPumpWhenCapabilitiesReady()
{
    if (g_capsWaitStartedMs <= 0) {
        g_capsWaitStartedMs = QDateTime::currentMSecsSinceEpoch();
    }
    g_pumpWhenCapsReady = true;
    if (!isCapabilitiesWarmBusy()) {
        notifyCapabilitiesBecameIdle();
    }
}

bool layerNeedsNewWfsCreate(const WfsLayerLoadItem& item)
{
    if (!item.visible || !item.map) {
        return false;
    }
    const QString uriStr = CaseWfsMap::buildWfsDataSourceUri(item.typeName);
    if (uriStr.isEmpty()) {
        return false;
    }
    return item.map->getLayer(uriStr) == nullptr;
}

void onCapabilitiesDownloadFinished(bool ok, const QByteArray& body, const QString& error)
{
    const qint64 downloadMs = g_capsWarmTimer.elapsed();
    g_capsPhase = CapsWarmPhase::Applying;

    if (ok) {
        // Force NAM disk cache first: auto CacheSaveControl often fails when Caps is non-cacheable.
        // Then populate gCacheCaps via PreferCache (parse only, no second network GetCapabilities).
        const QUrl capsUrl = capabilitiesRequestUrl();
        const bool namCacheInsertOk = insertCapabilitiesBodyIntoNamCache(capsUrl, body);
        if (!namCacheInsertOk) {
            qWarning() << "CaseWfsMap: namCacheInsertOk" << false
                       << "— Applying may re-download Caps";
        }

        QElapsedTimer applyTimer;
        applyTimer.start();
        populateParsedCapabilitiesCache();
        const qint64 applyMs = applyTimer.elapsed();
        g_capsWarmSucceeded = true;
        qInfo() << "CaseWfsMap: WFS capabilities warm succeeded"
                << "downloadMs" << downloadMs
                << "applyMs" << applyMs
                << "totalMs" << g_capsWarmTimer.elapsed()
                << "bodyBytes" << body.size()
                << "namCacheInsertOk" << namCacheInsertOk
                << "typename" << kWarmupTypeName;
    } else {
        g_capsWarmSucceeded = false;
        qWarning() << "CaseWfsMap: GetCapabilities warm failed:" << error
                   << "downloadMs" << downloadMs;
    }

    g_capsPhase = CapsWarmPhase::Idle;

    if (g_capsRestartRequested) {
        const bool force = g_capsRestartForce;
        g_capsRestartRequested = false;
        g_capsRestartForce = false;
        startCapabilitiesDownload(force);
        return;
    }

    notifyCapabilitiesBecameIdle();
    invokeCapsFinishedCallback();
}

void startCapabilitiesDownload(bool forceRefresh)
{
    if (g_capsPhase != CapsWarmPhase::Idle || !g_capsReply.isNull()) {
        // Non-force while busy: keep the in-flight warm (avoids a second full GetCapabilities).
        if (!forceRefresh) {
            return;
        }
        g_capsRestartRequested = true;
        g_capsRestartForce = true;
        return;
    }

    QgsNetworkAccessManager* nam = QgsNetworkAccessManager::instance();
    if (!nam) {
        g_capsWarmSucceeded = false;
        qWarning() << "CaseWfsMap: QgsNetworkAccessManager unavailable for GetCapabilities warm";
        invokeCapsFinishedCallback();
        return;
    }

    if (forceRefresh) {
        removeCapabilitiesUrlFromNamCache();
        // Caps content may have changed; allow schema warm to re-run for known typenames.
        g_schemaWarmDone.clear();
    }

    g_capsPhase = CapsWarmPhase::Downloading;
    g_capsWarmTimer.start();

    const QUrl capsUrl = capabilitiesRequestUrl();
    qInfo() << "CaseWfsMap: GetCapabilities warm GET"
            << "forceRefresh" << forceRefresh;

    QNetworkRequest request(capsUrl);
    // Align with QgsBaseNetworkRequest::sendGET(forceRefresh): AlwaysNetwork + save to NAM disk cache.
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
    QNetworkReply* reply = nam->get(request);
    g_capsReply = reply;

    QObject::connect(reply, &QNetworkReply::finished, qApp, [reply]() {
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = reply->errorString();
        const QByteArray body = ok ? reply->readAll() : QByteArray();
        if (g_capsReply == reply) {
            g_capsReply.clear();
        }
        reply->deleteLater();
        onCapabilitiesDownloadFinished(ok, body, error);
    });
}

void scheduleCapabilitiesWarm(bool forceRefresh)
{
    // Busy handling (including non-force no-op) lives in startCapabilitiesDownload.
    startCapabilitiesDownload(forceRefresh);
}

void refreshWfsCapabilitiesCacheBlocking()
{
    CaseWfsMap::waitForCapabilitiesReady();
    scheduleCapabilitiesWarm(true);
    CaseWfsMap::waitForCapabilitiesReady();
}

void handleLoadFailure(const WfsLayerLoadItem& item)
{
    if (item.rollbackCheckState) {
        item.rollbackCheckState();
    }

    const QString label = layerDisplayLabel(item);
    // Multi-item batch: silent reconcile at session end. Single-item (incl. ordinary checkbox): warn.
    if (g_batchSessionStarted && g_totalInSession > 1) {
        if (!label.isEmpty()) {
            g_failedNames.append(label);
        }
        return;
    }

    if (label.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"图层加载"), QString::fromUtf8(u8"图层加载失败，已取消加载。"));
    } else {
        TnUi::showWarning(QString::fromUtf8(u8"图层加载"),
                          QString::fromUtf8(u8"图层「%1」加载失败，已取消加载。").arg(label));
    }
}

void finishBatchSession()
{
    if (g_wfsShuttingDown) {
        if (g_loadingDialog) {
            g_loadingDialog->closeDialog();
            g_loadingDialog.clear();
        }
        g_failedNames.clear();
        g_batchLoadedLayers.clear();
        g_batchMapWidget.clear();
        g_batchSessionStarted = false;
        g_batchActive = false;
        g_totalInSession = 0;
        g_processedInSession = 0;
        return;
    }
    if (g_batchMapWidget) {
        QList<QgsMapLayer*> loadedLayers;
        loadedLayers.reserve(g_batchLoadedLayers.size());
        for (const QPointer<QgsMapLayer>& layer : g_batchLoadedLayers) {
            if (layer) {
                loadedLayers.append(layer.data());
            }
        }

        if (!loadedLayers.isEmpty()) {
            g_batchMapWidget->zoomToMergedLayerExtent(loadedLayers);
            armSuspendSchemaWarmUntilFirstPaint(g_batchMapWidget.data());
        } else {
            finishFirstPaintGate();
        }

        g_batchMapWidget->endLayerBatchUpdate();

        if (!g_failedNames.isEmpty() && g_batchLoadFailureReconcileCallback && g_batchMapWidget) {
            g_batchLoadFailureReconcileCallback(g_batchMapWidget.data());
        }

        g_batchMapWidget.clear();
    } else {
        finishFirstPaintGate();
    }

    if (g_loadingDialog) {
        g_loadingDialog->closeDialog();
        g_loadingDialog.clear();
    }

    g_failedNames.clear();
    g_batchLoadedLayers.clear();
    g_batchSessionStarted = false;
    g_batchActive = false;
    g_totalInSession = 0;
    g_processedInSession = 0;

    invokeLoadQueueIdleCallbackIfNeeded();
    if (!g_suspendSchemaWarmUntilFirstPaint) {
        scheduleSchemaWarmPump();
    }
}

void invokeLoadQueueIdleCallbackIfNeeded()
{
    if (!g_wfsLoadQueue.isEmpty() || g_wfsLoadProcessing) {
        return;
    }

    if (!g_loadQueueIdleCallback) {
        return;
    }

    std::function<void()> callback = std::move(g_loadQueueIdleCallback);
    g_loadQueueIdleCallback = {};
    if (callback) {
        callback();
    }
}

void beginBatchSessionIfNeeded()
{
    if (g_batchSessionStarted || g_wfsLoadQueue.isEmpty()) {
        return;
    }

    const WfsLayerLoadItem& head = g_wfsLoadQueue.head();
    const bool multiOrPrepared = g_batchActive || g_wfsLoadQueue.size() >= 2;
    const bool creating = layerNeedsNewWfsCreate(head);
    // Single uncheck / already-on-map: no LoadingDialog. Any new WFS create (incl. one item) starts a session.
    if (!multiOrPrepared && !creating) {
        return;
    }

    g_batchSessionStarted = true;
    g_totalInSession = g_wfsLoadQueue.size();
    g_processedInSession = 0;
    g_failedNames.clear();
    g_batchLoadedLayers.clear();

    g_batchMapWidget = head.map;
    if (g_batchMapWidget) {
        g_batchMapWidget->beginLayerBatchUpdate();
    }

    const QString initialMessage = isCapabilitiesWarmBusy()
                                       ? QString::fromUtf8(u8"正在准备 WFS 服务…")
                                       : QString::fromUtf8(u8"正在加载图层…");
    g_loadingDialog = new LoadingDialog(initialMessage, nullptr);
    g_loadingDialog->setAttribute(Qt::WA_DeleteOnClose);
    g_loadingDialog->showNonBlocking();
}

void pumpWfsLayerLoadQueue()
{
    if (g_wfsShuttingDown) {
        return;
    }
    if (g_wfsLoadProcessing) {
        return;
    }

    if (g_wfsLoadQueue.isEmpty()) {
        if (g_batchSessionStarted) {
            finishBatchSession();
        } else {
            invokeLoadQueueIdleCallbackIfNeeded();
        }
        scheduleSchemaWarmPump();
        return;
    }

    beginBatchSessionIfNeeded();
    if (g_wfsLoadQueue.isEmpty()) {
        if (g_batchSessionStarted) {
            finishBatchSession();
        } else {
            invokeLoadQueueIdleCallbackIfNeeded();
        }
        scheduleSchemaWarmPump();
        return;
    }

    const WfsLayerLoadItem& head = g_wfsLoadQueue.head();
    if (head.loadGeneration == g_loadGeneration && layerNeedsNewWfsCreate(head) && isCapabilitiesWarmBusy()) {
        if (g_loadingDialog) {
            g_loadingDialog->setMessage(QString::fromUtf8(u8"正在准备 WFS 服务…"));
        }
        requestPumpWhenCapabilitiesReady();
        return;
    }

    g_wfsLoadProcessing = true;
    const WfsLayerLoadItem item = g_wfsLoadQueue.dequeue();
    if (item.visible) {
        cancelSchemaWarmForTypeName(item.typeName);
    }

    if (item.loadGeneration != g_loadGeneration) {
        ++g_processedInSession;
        g_wfsLoadProcessing = false;
        if (g_wfsLoadQueue.isEmpty()) {
            finishBatchSession();
            return;
        }
        QTimer::singleShot(0, qApp, []() { pumpWfsLayerLoadQueue(); });
        return;
    }

    if (g_loadingDialog) {
        if (g_totalInSession > 1) {
            const int current = g_processedInSession + 1;
            g_loadingDialog->setMessage(formatBatchProgressMessage(current, g_totalInSession));
            g_loadingDialog->setProgress(g_processedInSession, g_totalInSession);
        } else {
            g_loadingDialog->setMessage(QString::fromUtf8(u8"正在加载图层…"));
        }
    }

    if (item.map) {
        const bool ok = applyWfsLayerInternal(item.map, item.displayName, item.typeName, item.level, item.visible,
                                              item.layerVal, item.isBaseCase, true);
        if (!ok) {
            handleLoadFailure(item);
        } else if (item.visible) {
            const QString uriStr = CaseWfsMap::buildWfsDataSourceUri(item.typeName);
            if (QgsMapLayer* layer = item.map->getLayer(uriStr)) {
                if (layer->isValid() && !TNMapWidget::isAttributeOnlyMapLayer(layer)) {
                    g_batchLoadedLayers.append(layer);
                }
            }
        }
    }

    ++g_processedInSession;
    g_wfsLoadProcessing = false;

    if (g_wfsLoadQueue.isEmpty()) {
        finishBatchSession();
        scheduleSchemaWarmPump();
        return;
    }

    QTimer::singleShot(0, qApp, []() { pumpWfsLayerLoadQueue(); });
}

bool applyWfsLayerInternal(TNMapWidget* map, const QString& displayName, const QString& typeName, int level,
                           bool visible, const QString& layerVal, bool isBaseCase, bool allowRetry)
{
    if (!map) {
        return false;
    }
    const QString trimmedType = typeName.trimmed();
    if (trimmedType.isEmpty()) {
        return false;
    }

    const QString uriStr = CaseWfsMap::buildWfsDataSourceUri(trimmedType);
    if (uriStr.isEmpty()) {
        return false;
    }

    const bool shouldFilterWayLayer = CaseWayLayer::shouldApplyDefaultTypeFilter(isBaseCase, layerVal, trimmedType);

    if (!visible) {
        if (QgsMapLayer* existing = map->getLayer(uriStr)) {
            map->removeLayer(existing);
        }
        return true;
    }

    if (QgsMapLayer* existing = map->getLayer(uriStr)) {
        if (shouldFilterWayLayer) {
            if (auto* existingVector = qobject_cast<QgsVectorLayer*>(existing)) {
                const bool hasAnalysisStyle =
                    !existingVector->customProperty(TNLayerFilter::kWayAnalysisStylePropertyKey)
                         .toString()
                         .trimmed()
                         .isEmpty();
                if (!hasAnalysisStyle) {
                    CaseWayLayer::scheduleDefaultTypeFilter(existingVector);
                }
            }
        }
        return true;
    }

    // Caps readiness is guaranteed by pumpWfsLayerLoadQueue (defer until Idle); no processEvents wait here.
    const QString layerTitle = displayName.trimmed().isEmpty() ? trimmedType : displayName.trimmed();
    QString createUri = uriStr;
    if (shouldFilterWayLayer) {
        QgsDataSourceUri filteredUri(uriStr);
        filteredUri.setParam(QStringLiteral("filter"), CaseWayLayer::defaultTypeSubsetExpression());
        createUri = filteredUri.uri(false);
    }
    auto tryCreateWfsLayer = [&]() -> QgsVectorLayer* {
        auto* layer = new QgsVectorLayer(createUri, layerTitle, QStringLiteral("WFS"));
        if (!layer->isValid()) {
            qWarning() << u8"WFS 图层创建失败：" << layer->lastError() << trimmedType;
            delete layer;
            return nullptr;
        }
        return layer;
    };

    QgsVectorLayer* vl = tryCreateWfsLayer();
    if (!vl && allowRetry) {
        refreshWfsCapabilitiesCacheBlocking();
        vl = tryCreateWfsLayer();
    }
    if (!vl) {
        qWarning() << "CaseWfsMap::applyWfsLayer: create WFS layer failed" << trimmedType;
        return false;
    }

    vl->setCustomProperty(kPanelWfsProp, QVariant(true));
    if (shouldFilterWayLayer) {
        vl->setCustomProperty(TNLayerFilter::kTypeServerFilterPropertyKey,
                              CaseWayLayer::defaultTypeSubsetExpression());
    }

    if (isWfsAttributeOnlyLayer(vl)) {
        if (!map->addAttributeOnlyLayer(vl, uriStr, level)) {
            delete vl;
            return false;
        }
        LayerStyleFactory::applyDefaultStyle(vl, layerVal.trimmed(), trimmedType);
        return true;
    }

    if (!map->addExistingVectorLayer(vl, level, uriStr)) {
        delete vl;
        return false;
    }

    LayerStyleFactory::applyDefaultStyle(vl, layerVal.trimmed(), trimmedType);

    if (shouldFilterWayLayer) {
        // 仅做 RuleBased 包装（纯内存，无网络 I/O），首帧即为 type<10 结果；
        // fail-open 存在性校验由首帧门闩 → scheduleDefaultTypeFilter 在工作线程异步完成。
        QElapsedTimer wrapTimer;
        wrapTimer.start();
        const CaseWayLayer::TypeFilterApplyResult result = CaseWayLayer::applyDefaultTypeFilterNow(vl);
        qInfo() << "CaseWfsMap: default type filter wrap" << trimmedType
                << "result" << static_cast<int>(result) << "wrapMs" << wrapTimer.elapsed();
        g_deferredClientTypeFilterLayers.append(vl);
    }
    return true;
}

} // namespace

namespace CaseWfsMap {

void setupWfsProviderCachingPolicy()
{
    QgsSettings settings;
    settings.setValue(QStringLiteral("qgis/wfsMemoryCacheDelay"), kWfsMemoryCacheDelaySeconds);
    scheduleCapabilitiesWarm(false);
}

void invalidateWfsCapabilitiesCache()
{
    removeCapabilitiesUrlFromNamCache();
}

void refreshWfsCapabilitiesCache()
{
#ifndef TNP_ENABLE_AUTO_WFS_CAPS_REFRESH
    return;
#endif
    scheduleCapabilitiesWarm(true);
}

void forceRefreshWfsCapabilitiesCache()
{
    scheduleCapabilitiesWarm(true);
}

void setCapabilitiesWarmFinishedCallback(std::function<void(bool succeeded)> callback)
{
    g_capsFinishedCallback = std::move(callback);
}

void clearCapabilitiesWarmFinishedCallback()
{
    g_capsFinishedCallback = {};
}

void ensureCapabilitiesWarm()
{
    if (g_capsWarmSucceeded) {
        return;
    }
    if (g_capsPhase != CapsWarmPhase::Idle || !g_capsReply.isNull() || g_capsRestartRequested) {
        return;
    }
    scheduleCapabilitiesWarm(false);
}

bool isCapabilitiesWarmSucceeded()
{
    return g_capsWarmSucceeded;
}

void scheduleSchemaWarm(const QStringList& typeNames, bool prioritize)
{
    bool added = false;
    for (const QString& raw : typeNames) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || g_schemaWarmDone.contains(trimmed)) {
            continue;
        }
        if (g_schemaWarmQueued.contains(trimmed)) {
            if (!prioritize) {
                continue;
            }
            g_schemaWarmQueue.removeAll(trimmed);
            g_schemaWarmQueue.prepend(trimmed);
            added = true;
            continue;
        }
        g_schemaWarmQueued.insert(trimmed);
        if (prioritize) {
            g_schemaWarmQueue.prepend(trimmed);
        } else {
            g_schemaWarmQueue.append(trimmed);
        }
        added = true;
    }
    if (added) {
        scheduleSchemaWarmPump();
    }
}

void waitForCapabilitiesReady()
{
    if (!isCapabilitiesWarmBusy()) {
        return;
    }

    QElapsedTimer timer;
    timer.start();
    while (isCapabilitiesWarmBusy()) {
        if (timer.elapsed() > kCapabilitiesWaitTimeoutMs) {
            qWarning() << "CaseWfsMap::waitForCapabilitiesReady timed out after"
                       << kCapabilitiesWaitTimeoutMs << "ms";
            break;
        }
        // Exclude user input so wheel/pan cannot interleave with sync Applying/ctor.
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }
}

void prepareBatchWfsLayerLoad()
{
    g_batchActive = true;
}

void invalidatePendingLoads()
{
    ++g_loadGeneration;
    g_deferredClientTypeFilterLayers.clear();
    g_suspendSchemaWarmUntilFirstPaint = false;
}

void prepareForShutdown()
{
    g_wfsShuttingDown = true;
    ++g_loadGeneration;
    g_wfsLoadQueue.clear();
    g_wfsLoadProcessing = false;
    g_loadQueueIdleCallback = {};
    g_batchLoadFailureReconcileCallback = {};
    g_capsFinishedCallback = {};
    g_deferredClientTypeFilterLayers.clear();
    g_batchLoadedLayers.clear();
    g_suspendSchemaWarmUntilFirstPaint = false;
    g_schemaWarmQueue.clear();
    g_schemaWarmQueued.clear();

    const bool needEndBatch = g_batchSessionStarted;
    QPointer<TNMapWidget> map = g_batchMapWidget;
    g_batchMapWidget.clear();
    g_batchSessionStarted = false;
    g_batchActive = false;
    g_totalInSession = 0;
    g_processedInSession = 0;
    g_failedNames.clear();

    if (g_loadingDialog) {
        g_loadingDialog->closeDialog();
        g_loadingDialog.clear();
    }

    if (needEndBatch && map) {
        map->endLayerBatchUpdate(false);
    }
}

void setLoadQueueIdleCallback(std::function<void()> callback)
{
    g_loadQueueIdleCallback = std::move(callback);
}

void clearLoadQueueIdleCallback()
{
    g_loadQueueIdleCallback = {};
}

void setBatchLoadFailureReconcileCallback(std::function<void(TNMapWidget*)> callback)
{
    g_batchLoadFailureReconcileCallback = std::move(callback);
}

void clearBatchLoadFailureReconcileCallback()
{
    g_batchLoadFailureReconcileCallback = {};
}

QString buildWfsDataSourceUri(const QString& typeName)
{
    const QString trimmed = typeName.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QgsDataSourceUri uri;
    uri.setParam(QStringLiteral("url"), normalizedWfsRootUrl());
    uri.setParam(QStringLiteral("typename"), trimmed);
    uri.setParam(QStringLiteral("version"), QStringLiteral("2.0.0"));
    // Align with QGIS Add preferred CRS / Caps DefaultCRS for this service.
    uri.setParam(QStringLiteral("srsname"), QStringLiteral("EPSG:4326"));
    // Do not set skipInitialGetFeature: let QgsWFSProvider issueInitialGetFeature
    // detect geometry type when DescribeFeatureType is ambiguous (same as desktop QGIS).
    return uri.uri(false);
}

void applyWfsLayer(TNMapWidget* map, const QString& displayName, const QString& typeName, int level, bool visible,
                   const QString& layerVal, bool isBaseCase, std::function<void()> rollbackCheckState)
{
    if (g_wfsShuttingDown || !map) {
        return;
    }
    const QString trimmedType = typeName.trimmed();
    if (trimmedType.isEmpty()) {
        qWarning() << "CaseWfsMap::applyWfsLayer: empty typeName";
        return;
    }

    WfsLayerLoadItem item;
    item.map = map;
    item.displayName = displayName;
    item.typeName = trimmedType;
    item.level = level;
    item.visible = visible;
    item.layerVal = layerVal;
    item.isBaseCase = isBaseCase;
    item.rollbackCheckState = std::move(rollbackCheckState);
    item.loadGeneration = g_loadGeneration;
    // Only claim the typename when a real create is queued; uncheck must not skip future schema warm.
    if (visible) {
        cancelSchemaWarmForTypeName(trimmedType);
    }
    g_wfsLoadQueue.enqueue(item);

    QTimer::singleShot(0, qApp, []() { pumpWfsLayerLoadQueue(); });
}

void removePanelWfsLayers(TNMapWidget* map)
{
    if (!map) {
        return;
    }
    map->removeLayersWithCustomProperty(kPanelWfsProp, QVariant(true), !g_wfsShuttingDown);
}

QString vectorLayerTableKey(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid() || !layer->dataProvider()) {
        return {};
    }

    QgsDataSourceUri uri(layer->dataProvider()->dataSourceUri());
    const QString table = uri.table();
    if (!table.isEmpty()) {
        return table;
    }
    return uri.param(QStringLiteral("typename"));
}

void resetPanelWfsDefaultSymbols(TNMapWidget* map)
{
    if (!map || !map->canvas()) {
        return;
    }

    CaseWayLayer::DefaultTypeFilterSuspendGuard typeFilterGuard;
    QgsMapCanvas* canvas = map->canvas();
    canvas->freeze(true);
    canvas->stopRendering();
    canvas->cancelJobs();
    canvas->waitWhileRendering();

    const QList<QgsMapLayer*> layers = canvas->layers();
    for (QgsMapLayer* layer : layers) {
        auto* vl = qobject_cast<QgsVectorLayer*>(layer);
        if (!vl || !vl->isValid() || !vl->dataProvider()) {
            continue;
        }
        if (!vl->customProperty(kPanelWfsProp).toBool()) {
            continue;
        }

        const QString key = vectorLayerTableKey(vl);
        if (key.contains(QStringLiteral("trace_tmp"))) {
            map->removeLayer(vl->dataProvider()->dataSourceUri());
            continue;
        }

        vl->removeCustomProperty(TNLayerFilter::kWayAnalysisStylePropertyKey);

        if (TNLayerFilter::layerUsesClientDisplayFilter(vl)
            || vl->customProperty(TNLayerFilter::kRendererBackupPropertyKey).isValid()) {
            TNLayerFilter::restoreBaseRendererAfterClientFilter(vl);
        }

        if (!dynamic_cast<QgsRuleBasedRenderer*>(vl->renderer())) {
            const QString resolvedVal = LayerStyleFactory::resolveLayerVal(vl, QString(), key);
            if (!LayerStyleFactory::applyDefaultStyle(vl, resolvedVal, key)) {
                TNMapWidget::setDefaultRendererForVectorLayer(vl);
            }
        }

        vl->triggerRepaint();
        vl->emitStyleChanged();
    }
    canvas->freeze(false);
}

} // namespace CaseWfsMap
