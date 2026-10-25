/*****************************************************************//**
 * \file   casewfsmapcontroller.h
 * \brief  方案/基础数据侧栏与 WFS（QGIS provider）图层加载
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef CASEWFSMAPCONTROLLER_H
#define CASEWFSMAPCONTROLLER_H

#include <functional>

#include <QString>
#include <QStringList>

class TNMapWidget;

namespace CaseWfsMap {

/** @brief 应用启动时调用：配置 WFS 内存缓存 TTL，并异步预热 GetCapabilities */
void setupWfsProviderCachingPolicy();

/** @brief 清除 NAM 磁盘缓存中的 WFS GetCapabilities URL（不清空整库缓存） */
void invalidateWfsCapabilitiesCache();

/**
 * @brief 图层列表变更后异步强制刷新 Capabilities（NAM 异步 GET + 灌入 gCacheCaps）
 * @note 受 CMake `ENABLE_AUTO_WFS_CAPS_REFRESH`（编译宏 `TNP_ENABLE_AUTO_WFS_CAPS_REFRESH`）控制，默认关闭；
 *       关闭时本函数为 noop。图层实质变更成功路径应调用本函数，勿自行 GET Caps。
 *       不阻塞 UI；开启且 force 时可打断/排队重启进行中的预热。
 *       创建失败同步重试（blocking）不受此开关影响。
 */
void refreshWfsCapabilitiesCache();

/**
 * @brief 手动强制刷新 Capabilities，不受 `ENABLE_AUTO_WFS_CAPS_REFRESH` 开关影响
 * @note 与启动预热相同：主线程 NAM 异步 GET + 灌入 gCacheCaps，不阻塞 UI
 */
void forceRefreshWfsCapabilitiesCache();

/**
 * @brief Capabilities 预热/强制刷新进入 Idle 后执行一次性回调
 * @param callback 参数为最近一次预热是否成功；替换前一次未触发的回调
 */
void setCapabilitiesWarmFinishedCallback(std::function<void(bool succeeded)> callback);
void clearCapabilitiesWarmFinishedCallback();

/**
 * @brief 确保 Capabilities 已预热：已成功或进行中则 noop，否则启动非强制预热
 * @note 不打断进行中的冷预热，供打开项目时 Caps 尚未成功的补触发
 */
void ensureCapabilitiesWarm();

/** @brief 最近一次 Capabilities 预热/刷新是否成功（供打开项目时决定是否补刷） */
bool isCapabilitiesWarmSucceeded();

/**
 * @brief 若 Capabilities 预热/刷新进行中，则 ExcludeUserInputEvents 等待结束后再返回
 * @note 仅用于创建失败同步重试、OD 结果表等同步场景；普通勾选由加载队列挂起等待 caps Idle
 */
void waitForCapabilitiesReady();

/**
 * @brief 对已知 typename 做空闲 Schema/Provider 预热（create+delete，不上图）
 * @param typeNames WFS typename 列表
 * @param prioritize true 时插到队列前部（进入方案详情时优先预热该案图层）
 * @note 使用 skipInitialGetFeature=true 只预填 DescribeFeatureType，不付 COUNT=1；
 *       正式勾选仍走 buildWfsDataSourceUri（不设 skip）。不计入 Caps busy，不弹 LoadingDialog；用户勾选队列优先
 */
void scheduleSchemaWarm(const QStringList& typeNames, bool prioritize = false);

/** @brief 批量同步 WFS 前标记：与普通勾选一样走 LoadingDialog 与地图批量刷新（可提前置位） */
void prepareBatchWfsLayerLoad();

/** @brief 递增加载代次，使队列中尚未处理的旧项在 pump 时被跳过 */
void invalidatePendingLoads();

/** @brief 退出前调用：停掉加载队列与延后回调，并释放图层 QPointer（须在删图层之前） */
void prepareForShutdown();

/** @brief WFS 加载队列全部处理完成后执行一次性回调（队列空闲且非处理中时） */
void setLoadQueueIdleCallback(std::function<void()> callback);
void clearLoadQueueIdleCallback();

/** @brief 批量 WFS 加载存在失败项时，在 finishBatchSession 中调用一次（地图批量更新结束后） */
void setBatchLoadFailureReconcileCallback(std::function<void(TNMapWidget*)> callback);
void clearBatchLoadFailureReconcileCallback();

/**
 * @brief 构建 WFS dataSourceUri（url + typename + version=2.0.0 + srsname=EPSG:4326）
 * @note 不设置 skipInitialGetFeature，与桌面 QGIS 一致：创建时由 Provider 做 COUNT=1 几何探测
 */
QString buildWfsDataSourceUri(const QString& typeName);

void applyWfsLayer(TNMapWidget* map, const QString& displayName, const QString& typeName, int level, bool visible,
                   const QString& layerVal = QString(), bool isBaseCase = false,
                   std::function<void()> rollbackCheckState = {});

void removePanelWfsLayers(TNMapWidget* map);
void resetPanelWfsDefaultSymbols(TNMapWidget* map);

} // namespace CaseWfsMap

#endif
