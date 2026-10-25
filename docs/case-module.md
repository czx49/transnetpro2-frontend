# 方案模块

方案侧栏、基础数据、地图 WFS、导入导出与右侧业务工作台的行为说明。接口字段见 [`api.md`](api.md)。

## 图层树结构

API 的 `menu[]`（`GET /api/project/detail`）与 `list[]`（`GET /api/case/list`）均为**图层组 + `children[]` 图层**。

| 场景 | 树结构 |
|------|--------|
| 基础数据 | **所有图层 → 图层组 → 图层**（「工具」分支独立） |
| 方案侧栏 | **方案 → 图层组 → 图层** |

### CaseMenuHelper

- `parseLayerGroupsFromJsonArray` — 从 `QJsonObject` / `QJsonArray` 读 `children`（避免 `MenuJson::j_children()` 在方案列表链路为空）
- `parseLayerGroupsFromCaseJson` — 读 `CaseJson` 的 `list[]`
- `buildBaseDataLayerGroups` — 基础数据树
- `flattenLayerGroups` — 扁平 `LayerItem` 供 WFS 遍历

### CaseTreeModel / CaseTreeView

- 方案节点 `UserRole` = `caseId`
- 图层组 `nodeKind=group`、`groupValue`；叶子 `nodeKind=layer` + `typename` / `level` / `val` / `category`
- 图层组**三态勾选**，联动子图层 WFS
- **单击已勾选图层行**（非复选框）→ `schemeLayerActivated` → `TNMapWidget::selectLayerByUri`（对齐一期方案详情图层单击）
- `setExpandsOnDoubleClick(false)`；单击组行正文切换展开（非复选框/分支区）；分支箭头为 `arrow_right` / `arrow_down` 自定义 PNG（`Phase2Ui::darkSidebarTreeBranchStyleRules`）
- `modelReset` 后 `collapseAll()` 默认全部收起；方案根节点 `CaseTreeItemDelegate` 圆角行
- 方案根右键：打开/编辑/重命名/克隆/删除；**图层叶子**右键：导入、导出 Excel/TXT

### BaseDataTableView

- **所有图层 → 图层组 → 图层** + **工具** 分支；分支箭头与方案树相同（四套 PNG + `Phase2Ui::darkSidebarTreeBranchStyleRules`）
- **工具白名单**（`CasePanel::buildBaseToolNodes`）：OSM数据下载 → OD数据生成 → 基础数据分析；末尾**固定追加**客户端节点 **生成人口数据**（不依赖 API `tools[]` 匹配，整树重建后仍在）
- 叶子勾选 → `layerStateChangedWithData` → **`m_baseMapWidget`** WFS

### OD 数据生成（基础数据工具）

- **入口**：基础数据侧栏「工具 → OD数据生成」→ `CasePanel::runBaseDataOdGeneration`
- **类型确认**：调用算法前弹出 `CaseOdTypeDialog`，选择 **机动车**（`motor`）或 **慢行**（`slow`）；取消后不调用接口
- **调用**：`HttpApis::grpcRunAlg` → `GET /api/grpc/runalg`，Query 带 `type=tool`、`type2=trip_model_all`、`case_id=0`、`json={"od_mode":"motor"|"slow"}`
- **成功后**：勾选并加载 `other_od` 图层并应用 OD 渲染样式，见 [`layer-style-factory.md`](layer-style-factory.md)

### 生成人口数据（基础数据工具）

- **入口**：基础数据侧栏「工具 → 生成人口数据」→ `CasePanel::runBaseDataPopGeneration`
- **图层**：自动查找道路组（`groupValue=road`）下 `val=road_community` 的小区图层并勾选加载至 `m_baseMapWidget`（`BaseDataTableView::findLayerUserDataByValAndGroup`）；WFS 队列空闲后通过 `QTimer::singleShot` 延迟打开配置弹窗（避免在 WFS 加载回调链内嵌套 `exec()`）
- **栅格数据**：`{appDir}/data/chn_pop_2024_CN_100m_R2025A_v1.tif`（与可执行文件同级；若在 `depends` 子目录运行则上溯一级）；单波段，计算使用波段 1
- **配置弹窗**（`CasePopGenerationDialog`，简化版 QGIS 分区统计）：
  - **小区图层**、**人口数据**两行只读展示，编辑框固定显示 `小区图层`、`2024人口数据`（不暴露 WFS 图层名或栅格文件名）
  - 统计方式固定为**总和**，关联字段固定 **`area_id`**，输出字段固定 **`pop_sum`**（前缀 `pop`），不在 UI 展示
  - 运行中禁用「运行」按钮并显示「计算中…/提交中…」，不使用 `LoadingDialog`；失败在弹窗内联红色提示
- **计算**（`CasePopZonalStatHelper`）：`QgsZonalStatistics::calculateStatistics` 按小区多边形对栅格 band1 求和
- **提交**：`HttpApis::projectPopData` → `POST /api/project/popdata`，body `{ project_id, data: [{ area_id, pop_sum }] }`
- **成功后**：配置弹窗 `accept()` 返回；`CasePanel::finishPopGenerationSuccess`（`QTimer` 延迟 slot）弹出成功提示，用户确认后再刷新树/reload；WFS idle 回调仅 schedule `openPopGenerationDialog`，避免在 WFS 加载 lambda 内嵌套 `exec()`

### 生成交通小区（基础数据工具）

- **入口**：基础数据侧栏「工具 → 生成交通小区」→ `CasePanel::runBaseDataCommunityGeneration`（服务端 `tools[]`，`type=openarea`）
- **运行前图层**：**仅**勾选加载道路组 `val=road_way`；**不**加载 `road_community`（TAZ 由本地生成后提交服务端写入）
- **配置弹窗**（`CaseCommunityGenerationDialog`）：只读展示项目与人口数据（`2024人口数据`），不再展示「路段图层」和「目标表」；可编辑格网边长/缓冲半宽/目标人口/上限系数；可选「是否覆盖现有小区图层」（未勾选时仅本地预估、不提交服务端）
- **计算**（`CaseCommunityGenHelper`，后台线程）：缓冲填洞 → fishnet → zonal sum → 裁剪 → 等距水滴扩张（优先队列区域生长）→ union；米制 CRS 计算，提交 EPSG:4326 GeoJSON
- **提交**：`HttpApis::projectGenCommunity` → `POST /api/project/gen_community`；`data[]` 含 `area_id`/`pop_sum`/`num_grids`/`seed_oid`/`geometry`（GeoJSON），**不含** `traffic_generation_volume`
- **成功后**：`finishCommunityGenerationSuccess` → 刷新 detail/WFS → 勾选加载或 `reload()` `road_community`
- 详见 [`gen-traffic-community.md`](gen-traffic-community.md)

### CaseItem / 刷新

- `caseLayerGroups` + `caseLayers`（扁平）
- `CasePanel::refreshCasesFromGlobalState` 使用 `parseLayerGroupsFromCaseJson`；`list[].children[].value` → WFS typename
- **工具后就地更新**（方案详情 + 基础数据）：`CaseMapToolController` 成功回调分别走 `refreshCurrentCaseLayerTreeInPlace`（`UIG::fetchCurrentProjectCasesQuiet` + `CaseTreeModel::syncCaseLayerGroups`）与 `refreshBaseDataLayerTreeInPlace`（`UIG::fetchCurrentProjectDetailQuiet` + `BaseDataTableView::syncLayerGroups`），**不**触发整页重置 / `ProjectOpenReady`，保留当前页、勾选态与已加载 WFS；新图层节点默认未勾选，辅助层在树同步 continuation 内 `setLayerCheckedByTypename` 自动加载

## WFS 与导入导出

### CaseWfsMapController

- WFS 2.0：`buildWfsDataSourceUri`（`url` + `typename` + `version=2.0.0` + `srsname=EPSG:4326`；**不在 URI 附加 filter/cql_filter**；**不**设 `skipInitialGetFeature`）
- `applyWfsLayer(..., isBaseCase)` / `removePanelWfsLayers` — `customProperty("tnp_panel_wfs")` 标记业务层
- **无 geometry WFS 层**：`geometryType()==Null` 且 `fields` 非空时，`addAttributeOnlyLayer` 注册（`customProperty("tnp_attribute_only")`），**不上 canvas**，数据视图可通过 WFS URI 查看属性表
- **Capabilities / Schema 预热策略**（避免间歇性 `Could not find typename in capabilities`，并减少首勾卡顿）：
  - 启动时 `setupWfsProviderCachingPolicy()`：设置 `qgis/wfsMemoryCacheDelay=600`，并**异步冷预热** GetCapabilities（主线程 NAM 异步 GET：`AlwaysNetwork` + `CacheSaveControl=true`；URL 对齐 QGIS `QgsWfsCapabilities::requestUrl` = `baseURL(true)` + `REQUEST` + `VERSION`，即 `SERVICE=WFS&REQUEST=GetCapabilities&VERSION=2.0.0`）
  - Caps 下载成功后**强制写入 NAM 磁盘缓存**（不依赖服务端 Cache-Control 是否允许自动缓存），再用假 typename `__tnp_wfs_caps_warm__` 建临时层灌入 `gCacheCaps`（typename 不在 Caps → 在 DFT 前失败）；Applying 阶段 `PreferCache` 命中后**只付 Caps 解析**，避免二次全量下载；日志含 `downloadMs` / `applyMs` / `namCacheInsertOk` / `bodyBytes`
  - **Schema 预热** `scheduleSchemaWarm`：Caps 成功且 Idle 后，用 **仅 warm 路径** URI（含 `skipInitialGetFeature=true` + `srsname=EPSG:4326`）串行 `create+delete`（不上图、不弹 Loading），提前支付 `DescribeFeatureType`，**不付** COUNT=1；用户勾选队列优先，Schema warm 不计入 Caps busy
  - **预热范围**：打开项目 / 基础数据整树重建只预热**基础数据** typename；进入方案详情或方案树就地同步时预热**当前方案**（可顺带基础数据），**不**扫全部方案，避免列表页空闲卡顿
  - **让出主线程**：项间间隔约 150ms；鼠标按下或距最近用户输入（移动/点击/滚轮/按键）不足约 300ms 时暂停并短间隔重试
  - 触发：`ProjectOpenReady` 收集基础数据 typename；进入方案详情时**优先**预热该案图层
  - 正式勾选 URI：`buildWfsDataSourceUri`（`url` + `typename` + `version=2.0.0` + `srsname=EPSG:4326`；**不**设 `skipInitialGetFeature`，与桌面 QGIS 一致）：创建时 Provider 同步 `issueInitialGetFeature`（COUNT=1）探测几何类型；日志含 `capsWaitMs` / `createElapsedMs` / `addToMapMs`
  - **Force refresh** `refreshWfsCapabilitiesCache()`：图层列表实质变更或业务成功后立刻刷 Caps（导入、期望线、道路溯源、`syncCaseLayerGroups` / `syncLayerGroups` 检测到 typename 集合变化等）。受 CMake **`ENABLE_AUTO_WFS_CAPS_REFRESH`**（编译宏 `TNP_ENABLE_AUTO_WFS_CAPS_REFRESH`）控制，**默认关闭**；关闭时函数为 noop。开启后进行中的预热可被 force 排队重启
  - **手动刷新** `forceRefreshWfsCapabilitiesCache()`：地图工具栏「刷新数据」使用，**不受**上述 CMake 开关影响；与启动预热相同走主线程 NAM 异步 GET。Caps 本身无项目过滤，完成后对当前基础数据或当前方案 typename 做 Schema warm（`setCapabilitiesWarmFinishedCallback` 一次性回调）
  - **约定**：后续凡图层信息变更（服务端新增/变更 typename）的成功路径，统一调用 `CaseWfsMap::refreshWfsCapabilitiesCache()`，勿自行 GET Caps；是否真正发起网络刷新由上述 CMake 开关决定
  - **Ensure** `ensureCapabilitiesWarm()`：打开项目（`ProjectOpenReady` → `refreshBaseDataTreeFromProjectDetail`）在 Caps 尚未成功且 typename 未变时使用；已成功或 busy 则 noop，**不打断**启动时的冷预热，避免第二轮全量 GetCapabilities 把勾选等待拉到 20s+
  - 非强制 `scheduleCapabilitiesWarm(false)` 在 Caps busy 时直接忽略，不再设 `g_capsRestartRequested`
  - **普通勾选**：Caps 预热未完成时加载队列**挂起**（不 `processEvents` 忙等），caps Idle 后下一 tick 再创建；等待中 Loading 文案为「正在准备 WFS 服务…」
  - `waitForCapabilitiesReady()` **仅**用于创建失败同步重试、OD 结果表等同步场景，且 `ExcludeUserInputEvents`（避免滚轮与 Applying/ctor 交织）
  - 普通勾选/取消、打开方案批量加载 **不** refresh；WFS 创建失败时**同步**强制刷新一次并重试 1 次（blocking 路径**不受** `ENABLE_AUTO_WFS_CAPS_REFRESH` 影响）
  - 批量同步（`applyCaseTreeWfsToMap`）前 `prepareBatchWfsLayerLoad()` 可提前置位会话；不 refresh
- **加载队列空闲回调**：`setLoadQueueIdleCallback` / `clearLoadQueueIdleCallback` — WFS 串行队列全部处理完成后执行一次性回调（供「生成人口数据」等在图层加载后打开弹窗）
- **批量加载与地图刷新**：
  - 勾选/取消（含组勾选）经 **串行队列** 处理；**任意新建** WFS（含单层）或 `prepareBatchWfsLayerLoad` / 多队列项时显示 `LoadingDialog`，并用 `beginLayerBatchUpdate` 抑制创建期反复 refresh
  - `TNMapWidget::beginLayerBatchUpdate` / `endLayerBatchUpdate` 合并 canvas 刷新；`removeLayersWithCustomProperty` 与 WFS 批量会话共用
  - 会话结束后，合并本次成功加载**有几何**图层的 provider/Capabilities extent 并 `zoomToFeatureExtent`（客户端不再 `updateExtents`）；无有效范围且仍停在三亚时，在该批首帧刷新后合并有效范围缩放一次；拖拽或滚轮后不再拉回。已有视图不被默认范围覆盖
  - 多图层会话全部完成后 **不** 弹出成功/失败个数汇总；失败项仍通过 reconcile 静默回滚侧栏勾选
  - `QgsVectorLayer` 创建仍在主线程（QGIS 约束）；日志含 caps Applying / 图层 create 耗时
- **加载失败 UX**：
  - 单图层（会话仅 1 项）：失败时 `TnUi::showWarning` 提示，并通过 `setLayerCheckedByTypenameQuiet` 回滚勾选
  - 多图层批量：结束后 **不** 汇总弹窗；失败项静默回滚勾选（基础数据 `reconcileCheckedLayersWithMap`）
- 基础方案路段层（`road_way` / `bus_way` / `slow_road_way`）加载后立刻 `CaseWayLayer::applyDefaultTypeFilterNow`（**仅**RuleBased 包装，建层路径无要素探测，保证渐进渲染），首帧后 `scheduleDefaultTypeFilter` 做状态核对与一次性异步 fail-open 校验，已包装不重绘（URI `filter` 只作可选首包提示）— 见 [`layer-filter.md`](layer-filter.md)
- `resetPanelWfsDefaultSymbols` — 分析上图前恢复业务 WFS 默认符号并清除客户端过滤状态
- 加载成功后 **`LayerStyleFactory::applyDefaultStyle`** — 见 [`layer-style-factory.md`](layer-style-factory.md)

**排查说明：** QGIS WFS Provider 在创建图层前用 GetCapabilities 校验 typename。若命中过期缓存（尤其服务端刚发布 `*_centeroid_lines` 等新图层），会报上述 WARN 且图层无效。开启 `ENABLE_AUTO_WFS_CAPS_REFRESH=ON` 后，图层列表变更调用 `refreshWfsCapabilitiesCache()`（异步强制刷新）可稳定加载；无论开关是否开启，创建失败重试都会同步强制刷新一次。

### CaseDrcsvHelper

从图层 `val` 或 `typename` 后缀推导 `POST /api/files/drcsv` 的 `table`（如 `road_way`）。

### CasePanel 地图路由

| 场景 | 地图 |
|------|------|
| 方案树勾选 + 详情在前 | `CaseDetailPanel` 标准/双屏（按 `caseId` 与对比方案 id） |
| 基础数据勾选 | 仅 `m_baseMapWidget` |
| 返回方案列表 | **不清** WFS |
| 切换方案 / `ProjectOpenReady` | 清理或重建 |

- `syncSchemeDetailWfsFromCaseTree` — 进入详情或切换对比/工具栏时按树勾选同步 WFS
- **导入**：`HttpApis::uploadCSV` → `POST /api/files/drcsv`（`type` 小写 `csv`/`txt`/`xlsx`；`case_id` 方案 id 或基础数据 `0`）。基础数据导入成功后**不**调用 `reloadCurrentProjectCases`，以免重建树导致分组折叠并清掉已上图图层；方案导入仍刷新方案列表
- **导出**：`HttpApis::exportProjectTable` → `GET /api/project/export`（同上 `case_id`）；`FileDownloader` 下载

## CaseDetailPanel

- 标准视图 + 双屏对比；共用 `CaseMapToolbarWidget`
- 嵌入时 `setBuiltInSchemeSidebarVisible(false)`；`embeddedSchemeTreeView()` 供外层接导入/导出信号
- 双屏：对比方案变更时重建左右 WFS
- 双屏底栏渲染样式下拉（无样式 / 饱和度 / 流量分布）：对左右 `road_way` 应用与机动车工作台路网分析一致的分级渲染；默认无样式

## 地图工具栏与属性

- 工具栏「撤回修改」（保存编辑左侧）：对当前地图编辑栈做一次 undo；空栈与双屏对比下禁用；tooltip 显示下一撤回图层名；标准视图 Ctrl+Z 在按钮可用时等效点击
- `CaseMapAttributeWidget` ← `currentLayerChanged`
- 工具栏「按属性过滤」→ `TNLayerAttributeFilterDialog`（见 [`layer-filter.md`](layer-filter.md)）
- 工具栏「符号」→ `CaseSymbolStyleDialog`（字段分级渲染与本地持久化，见 [`field-symbolization.md`](field-symbolization.md)）
- 工具栏「清除现有符号化」→ `CaseWfsMap::resetPanelWfsDefaultSymbols`，将当前地图业务 WFS 层恢复几何/工厂默认样式；基础方案路段层再恢复 `"type" < 10`；`trace_tmp` 从地图移除并 Quiet 反勾选树节点；图例隐藏。不删除 `layerstyles.db` 中已存字段样式
- 工具栏「保存画布」→ `CaseMapExportDialog` 弹窗取参 → `CaseMapToolController::exportMapWithOptions`（手动模式，成功弹提示）
- 工具栏「刷新数据」（保存画布右侧）→ `CaseWfsMap::forceRefreshWfsCapabilitiesCache` + 当前项目 Schema warm
- 工具栏「工作台」↔ `CaseWorkPanel::setExpanded` / `workbenchExpandedChanged`

### 期望线 / 道路溯源（方案详情 + 基础数据）

| 按钮 Tooltip | API | 前置条件 | 成功后 |
|--------------|-----|----------|--------|
| 生成期望线 | `GET /api/case/create_expect_line` | 面图层；侧栏树已勾选；`tableName` = 节点 `value` | 方案详情 / 基础数据：就地合并辅助图层节点（保留勾选、不清 WFS）后 **自动勾选加载**「道路图层_期望线」「道路图层_质心节点」；随后通过 `CasePanel::applyOtherOdStyleAfterDesireline` 加载当前上下文 `other_od` 并应用 demand 分级样式（地图渲染由 `CaseOtherOdMapHelper` 与 OD 数据生成共享，业务链路独立） |
| 道路溯源 | `GET /api/case/get_trace` | 线图层含 `link_id`；点选路段取 `link_id`；`tableName` = 节点 `value` | 方案详情 / 基础数据：就地合并图层树（保留勾选、不清 WFS）后 **自动勾选加载**辅助图层「道路溯源」（`*_trace_tmp`），并按 `path_flow` 5 级着色，**图例同步**分级色块与区间并显示；「清除现有符号化」同步移除地图层并 Quiet 反勾选树节点，图例隐藏 |

- 基础数据地图传 `case_id=0`（与导入/导出一致）
- `tableName` 由 `CaseMapLayerResolve` 从侧栏已勾选节点的 API `value` 解析，不用 `val` 键
- 道路溯源点选：空选可重试；非目标图层选择会被忽略
- 辅助图层 typename 由 `CaseAuxLayerHelper` 推导：期望线从源面图层 `tableName`（`{table}_centeroid_lines` / `_centeroid_to_line`），道路溯源从源线图层经 `CaseWayLayer::deriveTraceTmpTypeName`；加载走侧栏勾选 → WFS 串行队列。期望线符号为几何默认（点红/线蓝）；道路溯源经 `LayerStyleFactory::applyPathTraceStyle`（`path_flow`），样式应用后由 `CaseMapToolContext::syncMapLegendFromGraduatedLayer` 同步图例（方案详情 `syncSchemeDetailLegendFromWayLayer` / 基础数据 `syncBaseMapLegendFromLayer`）
- 期望线成功后 OD 图层样式：`CaseMapToolController::runDesireline` 在辅助图层勾选完成后调用 `CaseMapToolContext::onDesirelineSucceeded`（由 `CasePanel` 注入）；基础数据查找 `menu[]` 中 `other_od`，方案详情查找当前方案 `list[]` 中 `other_od`

## CaseWorkPanel（业务工作台）

嵌入 `CaseDetailPanel` 右侧；无 Tab。**壳层**负责折叠按钮、标题与 `QStackedWidget` 会话切换；**CaseWorkSessionPage** 持有单个 `(caseId, isBase, module)` 的完整工作流状态。

### 会话缓存

| 键 | 含义 |
|----|------|
| `{caseId}\|{isBase}\|chooser` | 未选分析模块时的模块选择页 |
| `{caseId}\|{isBase}\|{module}` | 已选模块的工作流会话（`AnalysisModule` 枚举整型） |

- `setCurrentCase`：**不**停止其它会话的 SSE/HTTP；切换 stack；刷新标题；若该方案曾选过模块则恢复对应会话，否则显示 chooser；flush 同 `caseId` 的 pending `analysisResultViewed`
- `setAnalysisModule` / 模块按钮：按需 `ensureSession` 并显示；**允许**在其它会话 busy 时切换模块
- `prepareForShutdown` / 关闭项目 Tab：遍历所有会话释放 gRPC 流
- `clearSessionsForCase(caseId)`：删除方案后清理该 id 的全部会话与 chooser
- 共享 `AiConfigModel` 与 `GET /api/config/question` 缓存注入各会话
- **信号门控**：`analysisResultViewed` 仅同 `caseId` 立即向外 emit，否则 pending 至切回该方案；`analysisDataRequested` / `deriveCaseFromBaseRequested` / `aiDataSelectRequested` 仅来自**当前可见**会话；AI 框选回写经 `mActiveAiSelectSessionKey` 定位发起会话
- 双屏对比模式：**不**把工作台 rebind 到左侧对比方案（保持当前标准方案会话）

### 流程与 SSE（CaseWorkSessionPage）

- 算法步骤：`GrpcStreamClient` → `GET /api/grpc/stream`（SSE 超时机制见 [`api.md`](api.md) 算法管理「客户端 HTTP 超时」；与 REST 分层不同）
- 工作台初始化时通过 `GET /api/case/detail/:id` 读取 `{motor|slow|bus}_alg1/alg2`。两个状态均为 `1` 时，基础方案恢复 `network` / `traffic` 为已完成，普通方案恢复 `suggestion` / `review` 为已完成，并按已完成状态展示报告入口；数据支撑集检查未通过时仍不能运行。已完成算法步骤仍可再次手动运行。
- 流步骤（`network` / `traffic` / `review`）失败后保持 `Failed`，「运行」仍可点击重试；下一步仅在上一步成功后解锁（如 `network`→`traffic`）
- 普通方案改造流程三步：**方案建议** → **修改方案记录** → **方案评审**（`editLog` 无运行按钮，可多次查询修改记录）
- **方案评审**可在完成后再次运行（仅运行中禁用按钮）；完成后步骤内展示【路网流量分布】【交通饱和度】（与基础方案机动车路网现状分析相同的 RdYlGn 等距/饱和度固定区间，`traffic_flow` / `saturation`）
- 方案建议（子按钮）：`GrpcAiStreamClient` → `GET /api/grpc/aistream`（带 `prompt`）；主流程「方案建议」运行已改用 `GET /api/grpc/runalg?type2=change`（长耗时同步 REST，适用 api.md 长接口 30s 超时策略）
- **修改方案记录**：`HttpApis::caseEditLog` → `GET /api/case/edit_log`（见 [`api.md`](api.md)）；独立步骤卡片，下方 TextEdit 展示回信列表

### 共享智能对话

- 基础方案宿主步骤：**交通问题诊断**（`traffic`）
- 普通方案宿主：**方案评审**（`review`）
- `network` / `suggestion` / `editLog` 的日志与 AI 写入同一 `FlowStep::chatTimeline`（普通方案宿主为 `review`）

### 显示逻辑

- 默认隐藏（`logVisible=false`）；标题行 **AI 智能助手** 切换显示；运行/方案建议时若未显示则自动展开
- 运行结束**不**自动重新弹出

### AIChatWidget

- `setUseBackendAiApi(true)` → `promptSubmitted` → `HttpApis::grpcAiApi`（长耗时同步 REST，适用 api.md 长接口 30s 超时策略）
- 展示 `GrpcAiApiResultJson::content`；`download_url` → **「查看详情」** 链接（`FileDownloader`）
- 消息区最小高度 **240px**；timeline 含 User/Assistant/Log/Error；`refreshWorkPage` 后 `restoreChatTimeline`
- **AiWordSelectBar**：L1/L2 Chip（`AiConfigModel` + `GET /api/config/aiconfig`）；「数据选择」为 checkable：checked 激活地图框选（`aiDataSelectRequested(module, stepKey, activate)` → `CasePanel::onAiDataSelectRequested`），框选完成或再次点击 unchecked；L1 含「框选」时门控可用性且离开「框选」清空 `aiSelectedLinkIds`，不含「框选」时默认可用；发送时 L1 → `analysis_object`、L2 → `engineer_notes`、常见问题 → `business_goal`（`GET /api/config/question`）
- 等待 AI 时禁用输入与选词

### 其它步骤

- **交通问题诊断**完成后不展示步骤内「分析结果:」区块
- **成本效益分析**：`caseCostView` + `openHtmlReportWithFallback`（`content` 为 HTML URL）
- **报告**：`grpcCaseExport`、`caseReportView`、`caseReportDownload`、`exportPtCaseReport`（已移除 `/api/case/export_report`）；机动车分析报告 / 改造方案报告查看与基础数据分析一致（`content` 为 HTML URL，`openHtmlReportWithFallback`）
- **基础数据分析**（侧栏工具）：`caseBaseDataView`（HTML URL，WebView2 或下载后系统浏览器）
- **OD 数据生成**：`grpcRunAlg`，`type=tool`，`type2=trip_model_all`，`case_id=0`；外层 `code=200` 后以 `data.code==1` 为业务成功，否则用 `data.message` 警告，不加载图层

### 基础方案路网分析（地图 / 数据）

| 能力 | 说明 |
|------|------|
| 默认 type 过滤 | 勾选路段 WFS 层时 `"type" < 10`（客户端过滤），详见 [`layer-filter.md`](layer-filter.md) |
| 分析上图 | `onAnalysisResultViewed` → `applyAnalysisResultOnMap`：勾选对应 `road_way`/`bus_way`/`slow_road_way`，清除 type 过滤，应用流量/饱和度分级样式（三模块与普通方案评审均为机动车参数：流量 RdYlGn 等距 5 级、饱和度固定区间、线宽 0.6pt）。基础方案**慢行**路网现状分析完成后展示【机动车流量】【机动车饱和度】【慢行流量】【慢行饱和度】：前两个上图 `road_way`，后两个上图 `slow_road_way`；机动车/公交模块仍为【路网流量分布】【交通饱和度】 |
| 自动保存画布 | 程序启动时在 app 目录（`depends` 模式下向上修正后的基准目录）创建 `images/`；基础方案完成「路网现状分析」后，点击流量/饱和度结果（`traffic_flow` / `saturation`，含慢行模块的机动车与慢行四按钮）并成功应用样式时，自动导出 PNG 至 `images/`（当前可视范围、96 DPI、含天地图）；文件名 `地图画布_{名称}_项目{id}_方案{id}_{yyyyMMdd_HHmmss}.png`；显示 Loading，成功写日志，失败弹警告；复用 `CaseMapToolController::exportMapWithOptions`（Auto 模式，无参数弹窗） |
| 分析数据 | `onAnalysisDataRequested` → `showAnalysisDataDialog`：`DataBrowserDialog::loadVectorLayer` 读取对应模块 WFS 路段层（`road_way`/`bus_way`/`slow_road_way`）属性；弹窗标题用结果按钮名称；**不**改地图样式；字段见 [`layer-style-factory.md`](layer-style-factory.md) §6.1 |
| 按属性过滤 | 地图工具栏 → `TNLayerAttributeFilterDialog`（WFS 客户端 / PostGIS subset） |

### FileDownloader

- `dialogParent` 挂主窗口；AI 报告下载防重复（`m_httpBusy`）

## 可选编译项

`ENABLE_ZOOM_TO_LAYER=ON` 时，方案树与基础数据图层右键增加「缩放至图层」（`TNMapWidget::zoomToLayer`）。

## HTTP 封装索引

`httpapidefines.h` / `httpapis.cpp`：`getAiConfig`、`getCommonQuestions`、`caseBaseDataView`、`caseCostView`、`caseComparisonView`、`grpcAiApi`、`uploadCSV`、`exportProjectTable`、`copyProjectCase` 等。
