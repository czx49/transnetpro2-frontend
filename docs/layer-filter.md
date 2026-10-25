# 图层过滤与定义查询

Phase2 中矢量图层的**显示过滤**（Definition Query）统一经 `TNLayerFilter` 命名空间处理。WFS 与 PostGIS 采用不同实现路径，与 Phase1 对齐的业务规则（基础方案路段层默认 `"type" < 10`）保持不变。

**相关源码：**

| 模块 | 路径 |
|------|------|
| 过滤分发 | `src/tnmapwidget/tnlayerfilterapply.h/.cpp` |
| WFS 客户端过滤 | `src/tnmapwidget/tnlayerclientfilter.h/.cpp` |
| 按属性过滤 UI | `src/tnmapwidget/tnlayerattributefilterdialog.h/.cpp` |
| SQL / 子句构建 | `src/tnmapwidget/tnlayerfiltersqlbuilder.h/.cpp` |
| 路段层默认过滤 | `src/casectrls/casewaylayerhelper.h/.cpp` |
| 移除前消毒 | `src/tnmapwidget/tnmapwidget.cpp` → `prepareVectorLayerForRemoval` |

样式规则见 [`layer-style-factory.md`](layer-style-factory.md)；方案侧栏与工作台联动见 [`case-module.md`](case-module.md)。

---

## 1. 设计原则

| 数据源 | 过滤方式 | 说明 |
|--------|----------|------|
| **WFS（常规）** | 客户端渲染过滤 | 不向 URI **回写** `cql_filter` / `filter`；用 `QgsRuleBasedRenderer` 根规则 `setFilterExpression` 包装当前渲染器 |
| **WFS（基础方案路段层）** | 客户端 RuleBased + 可选 URI `filter` | 显示过滤必须包装 `QgsRuleBasedRenderer`；创建 URI 可带 `"type" < 10` 作首包提示，**不得**用服务端标记冒充已过滤 |
| **PostGIS** | `QgsVectorLayer::setSubsetString` | 标准 SQL WHERE 子句，由服务端 subset 生效 |

统一入口：`TNLayerFilter::applyDefinitionQuery(layer, subsetClause)` — 内部按 `providerType()` 分流至 `applyClientLayerFilter` 或 PostGIS subset。

**WFS 换源约束：** 活图层上 `setDataSource` 必须经 `TNMapWidget::reloadVectorLayerDataSourceSafely`（`freeze` → `cancelJobs` → `waitWhileRendering`），并保留规范 URI 查找键。换源全程由 `isVectorLayerReloadInProgress` 标记，禁止嵌套访问 Provider。无所属地图时只清 `subsetString`，不换 Provider。

---

## 2. WFS 客户端过滤机制

### 2.1 持久化键

| customProperty | 含义 |
|----------------|------|
| `tnp_layer_display_filter` | 客户端过滤已真正包装到渲染器后写入的 QGIS 表达式（如 `"type" < 10`）；禁止在未 wrap 时写入 |
| `tnp_layer_renderer_backup_v1` | 应用过滤前渲染器的 XML 备份（非 RuleBased 时写入） |
| `tnp_wfs_type_server_filter` | 创建时已下推到 WFS URI 的默认 type 过滤（仅作首包提示，不代表画布已筛选） |
| `tnp_way_analysis_style` | 工作台/双屏分析分级样式占用标记（值为字段名；恢复默认样式时清除） |
| `tnp_way_type_filter_verified` | 路段默认 type 过滤 fail-open 存在性校验已完成（值为表达式；`clearTypeFilter` 时清除） |

### 2.2 核心 API

```cpp
namespace TNLayerFilter {
    void ensureWfsUriWithoutServerFilter(QgsVectorLayer* layer, TNMapWidget* map = nullptr);
    bool wrapRendererWithDisplayFilter(QgsVectorLayer* layer, const QString& displayFilter, QString* errorOut);
    void restoreBaseRendererAfterClientFilter(QgsVectorLayer* layer);
    ApplyDefinitionQueryResult applyClientLayerFilter(...);
    ApplyDefinitionQueryResult clearClientLayerFilter(...);
    QString displayFilterExpression(const QgsVectorLayer* layer);
    bool layerUsesClientDisplayFilter(const QgsVectorLayer* layer);
}
```

流程概要：

1. 规范查找 URI 仍是干净的 `CaseWfsMap::buildWfsDataSourceUri`（`url` + `typename` + `version` + `srsname`）。基础方案路段层**创建**时可额外带 `filter`，但 `mUriLayers` 以规范 URI 为键。
2. 应用客户端过滤前 `backupRendererIfNeeded`；通过 QGIS `QgsRuleBasedRenderer::convertFromRenderer` 将当前渲染器包装为带根过滤规则的 RuleBased 树。**不**把该表达式写回 URI。
3. `ensureWfsUriWithoutServerFilter` **只剥不补**：仅当已去掉 `tnp_wfs_type_server_filter` 且 URI 仍带 filter 时，经 `reloadVectorLayerDataSourceSafely` 换源；`syncLayerUriKey` 只追加新键、不丢掉规范键。
4. 清除过滤或移除图层时 `restoreBaseRendererAfterClientFilter`：从 XML 备份恢复，或回退几何默认符号。

---

## 3. 基础方案路段层默认 type 过滤

### 3.1 适用图层

| `val` | 分析模块 |
|-------|----------|
| `road_way` | 机动车 |
| `bus_way` | 公交 |
| `slow_road_way` | 慢行 |

识别：`CaseWayLayer::isWayLayerVal` / `isWayLayerTypeName`（typename 后缀匹配，兼容工程前缀）。

**条件**：仅 **基础方案**（`isBaseCase == true`）勾选加载时应用，表达式与 Phase1 一致：

```
"type" < 10
```

### 3.2 调度与就绪

`CaseWfsMap::applyWfsLayer(..., isBaseCase)` 对基础方案路段层在**创建 URI** 上可下推 `filter="type" < 10`，并写入 `tnp_wfs_type_server_filter`（本服务 GetFeature 主要文档化 `cql_filter=key=value`，OGC FILTER 不一定生效）。**显示真相是客户端 RuleBased**：

- 加入地图并 `applyDefaultStyle` 后立刻 `CaseWayLayer::applyDefaultTypeFilterNow`：**仅做 RuleBased 包装（纯内存，无任何 `getFeatures` / 网络 I/O）**，首帧即为过滤结果；建层路径上禁止同步要素探测，否则主线程阻塞等待 GetFeature、画布 250ms 渐进刷新失效，表现为“等待后一次性全部显示”；
- 返回值 `TypeFilterApplyResult`：`NotReady`（未就绪/换源中/挂起中）、`Skipped`（已有分析样式或已 fail-open 回退）、`AlreadyApplied`（已包装，**不重绘**）、`Applied`（本次包装，可 `triggerRepaint`）；
- 首帧门闩对 deferred 层一律 `scheduleDefaultTypeFilter`；图层已在图上且无分析样式标记时同样调度（不以 `tnp_wfs_type_server_filter` 代替）；
- `scheduleDefaultTypeFilter` 延迟到下一事件循环，`NotReady` 时按 **200 / 500 / 1000 / 2000 / 4000 ms** 重试；换源中/挂起中 50ms 后同次重试；
- 包装状态判断 `typeFilterWrapped`：`tnp_layer_display_filter` 匹配，**且**当前为 `QgsRuleBasedRenderer` 且根规则表达式包含 `"type" < 10`；
- 若图层已有 `tnp_way_analysis_style`，默认 type 过滤直接跳过，避免覆盖饱和度/流量分级；
- **fail-open（异步、每层一次）**：`scheduleDefaultTypeFilter` 在包装确认后调用 `scheduleFailOpenCheck`：主线程构造 `QgsVectorLayerFeatureSource` 快照与两条 `setLimit(1)+NoGeometry` 请求，`QtConcurrent::run` 在工作线程探测，`QFutureWatcher`（parent `qApp`）回主线程；回调用 `QPointer` 校验图层，若期间换源中/挂起中/已有分析样式/包装已被清除则丢弃结果；否则写入 `tnp_way_type_filter_verified`，且**无匹配要素但图层有数据**时回退过滤保留全量显示。`clearTypeFilter` 清除该标记；已回退的层在下次 `applyDefaultTypeFilterNow` 返回 `Skipped`，避免“回退→再包装→再回退”抖动。

### 3.3 分析上图时解除 type 限制

工作台点击 **路网流量分布** / **交通饱和度** 的「查看详情」或触发地图联动时，`CasePanel::applyAnalysisResultOnMap`（双屏底栏走 `CaseCompareRender::applyToMap`）：

1. `setLayerCheckedByVal` 勾选对应路段层；
2. `resetPanelWfsDefaultSymbols`：`freeze` + `cancelJobs` + `waitWhileRendering` 后恢复业务 WFS 默认符号（跳过 `trace_tmp`；**不**在函数内 `refresh`）；
3. `CaseWayLayer::clearTypeFilter(layer, map)` 清除 type 标记，并在冻结画布后剥离 URI 上的服务端 filter；
4. `LayerStyleFactory::applyTrafficFlowStyle` 或 `applyTrafficSaturationStyle` 应用分级样式，并写入 `tnp_way_analysis_style`；
5. 图层未就绪时由定时重试 + WFS idle 回调再次尝试。

`waitWhileRendering` / `setDataSource` 会泵入嵌套事件循环（含用户点击）。上图请求必须跨点击串行化：

- 进行中再次点击只覆盖 pending（`module` / `resultKey` / `resultName`），不新开 `applyOnce`；当前这次结束后再应用最后一次点击。
- 每次新启动上图递增 generation，过期 timer / idle 回调直接返回。
- 双屏 `applyToMap` 按地图实例同样串行（左右图互不阻塞）。
- `TNMapWidget::reloadVectorLayerDataSourceSafely` 记录换源中图层；`ensureWfsUriWithoutServerFilter` 与默认 type 过滤在换源期间不访问 Provider。
- 分析突变期间 `CaseWayLayer::DefaultTypeFilterSuspendGuard` 挂起默认 type 过滤，避免与分级样式抢渲染器。

**数据按钮**（`showAnalysisDataDialog`）与上图共用 `wayLayerOnStandardMap` 勾选并定位 WFS 路段层，经 `DataBrowserDialog::loadVectorLayer` 展示属性表；不调用 `clearTypeFilter` 或分级样式，避免覆盖已应用的地图渲染。

---

## 4. 按属性过滤（工具栏）

地图工具栏 **「按属性过滤」** → `CaseMapToolController::runAttributeFilter` → `TNLayerAttributeFilterDialog`。

- 构建 SQL/QGIS 表达式子句，经 `applyDefinitionQuery` 应用；
- WFS 层走客户端过滤，PostGIS 层走 subset；
- 「清除查询」传入空子句以恢复；
- 「验证」临时应用后恢复，不影响最终状态直至用户确认。

---

## 5. 图层移除与 RuleBased 稳定性

取消勾选或批量移除 WFS 业务层时，`TNMapWidget::removeLayer` 在 `finalizeMapLayerRemoval` 之前调用 `prepareVectorLayerForRemoval`：

1. `stopRendering` + `waitWhileRendering`；
2. 若存在客户端过滤、渲染器备份或当前为 RuleBased → `restoreBaseRendererAfterClientFilter`；
3. 再从工程中移除图层。

`CaseWfsMap::resetPanelWfsDefaultSymbols` 在分析上图前先停净渲染，再对全部 `tnp_panel_wfs` 标记层恢复默认符号并清除分析样式标记；调用方负责最终 `refresh`。

---

## 6. 验证清单

1. **基础方案**勾选 `road_way` / `bus_way` / `slow_road_way`：地图仅显示 `type < 10` 路段，取消勾选不崩溃。
2. 三模块工作台 **路网流量分布** / **交通饱和度**：对应路段层分级着色（`volume` / `saturation_edit`），含公交 `bus_way`。
3. 慢行路网现状分析后点机动车饱和度、机动车路网现状分析后点路网流量分布：上色且不崩溃。
4. 双屏对比底栏：无渲染 ↔ 饱和度 ↔ 流量，左右图均不崩溃。
5. 分析上图后取消勾选路段层再勾选：恢复默认符号；基础方案仍为 `type < 10`。
6. 工具栏 **按属性过滤**：WFS 与 PostGIS 层均可增删条件；清除后恢复全量。
7. **分析数据**弹窗（`DataBrowserDialog`）：读取 WFS 路段层属性，列配置与一期一致；不触发地图样式变更。
8. PostGIS 矢量层 subset 行为与 Phase1 一致，不受 WFS 客户端路径影响。
