# 首页与项目方案页真实数据接入设计

## 背景与目标

基于现有 API 能力，完成以下目标：

1. 首页“我的项目”从 `/api/project/page` 获取真实项目列表，不再使用本地模拟数据。
2. “新建项目”通过 `/api/project/add` 创建，成功后提供“立即打开/稍后”交互选择。
3. 打开项目后先请求 `/api/project/detail` 获取项目基础信息，再请求方案列表接口补齐方案数据。
4. 将 `project/detail.data.menu` 临时归类为“基础方案”数据，并按“道路 -> 公交 -> 其他”顺序展示。
5. 左侧基础数据区与右侧项目方案内容区均展示基础方案数据。
6. 通过 `UIGlobalState` 统一管理项目数据、详情、方案及界面消息通知。

## 范围与非目标

### 范围内

- `HomePanel`、`MainWindow`、`CasePanel` 的数据流改造。
- `UIGlobalState` 扩展为项目域状态中心。
- 项目与方案数据映射、加载状态、错误状态、重试机制。

### 范围外

- 方案新增/编辑/删除的完整后端联动改造（本次仅保证列表展示链路）。
- 基础方案长期数据模型定稿（本次为临时归类规则）。
- 新增复杂缓存淘汰策略或离线持久化。

## 设计原则

1. **单一数据源**：项目域状态统一由 `UIGlobalState` 管理。
2. **界面只表达意图**：页面层发起动作与渲染状态，不做跨接口拼装。
3. **部分可用优先**：`detail` 成功、`case list` 失败时仍允许进入项目页。
4. **可重试**：方案列表失败后在项目页就地重试。
5. **可扩展**：后续可平滑接入项目编辑、删除、方案管理等能力。

## 方案选型

采用“`UIGlobalState` 作为项目域单一数据源（SSOT）”：

- `HomePanel`：加载与创建项目入口。
- `MainWindow`：仅负责标签页与页面切换容器。
- `CasePanel`：按状态渲染项目详情、基础方案、方案列表。
- `UIGlobalState`：集中进行 API 调用、状态管理、事件通知、数据映射。

## 关键流程

### 1) 首页加载项目列表

1. 页面初始化触发 `UIGlobalState::loadProjectPage(page=1, limit=10, name="")`。
2. 调用 `/api/project/page`。
3. 成功后更新项目列表状态与数据；失败则更新错误态并发出通知事件。
4. “我的项目”弹窗需保持现有界面交互不变：
   - 顶部按项目名称搜索框继续可用；
   - “新建项目”入口继续可用并复用同一创建链路。

### 2) 新建项目

1. 在首页弹窗输入项目名称后触发 `UIGlobalState::createProject(name, desc)`。
2. 调用 `/api/project/add`。
3. 成功后刷新项目列表，并触发“创建成功”事件。
4. UI 弹出“立即打开 / 稍后”二选一：
   - 立即打开：触发 `openProject(newProjectId)`。
   - 稍后：停留首页，仅刷新展示。
5. “我的项目”弹窗中的“新建项目”入口与首页其它新建入口必须行为一致，统一走同一状态动作与成功后分支。

### 2.1) 我的项目弹窗搜索适配

1. 搜索框按“项目名称”实时过滤当前项目列表展示结果。
2. 搜索行为仅影响展示，不修改 `UIGlobalState` 的原始项目列表快照。
3. 清空搜索词后立即恢复完整项目列表。
4. 搜索结果中的双击打开、右键菜单打开、以及新建入口行为与非搜索态保持一致。

### 3) 打开项目（主链路）

1. 触发 `UIGlobalState::openProject(projectId)`。
2. 先调用 `/api/project/detail`。
3. 成功后解析 `detail` 与 `menu`；若失败则整体打开失败。
4. 再调用 `/api/case/list?project_id=...` 获取方案列表。
5. 结果分支：
   - 全成功：`Ready`。
   - `detail` 成功 + `case list` 失败：`PartialReady`（允许进入项目页）。
   - `detail` 失败：`Error`（不进入项目页）。

### 4) 方案列表重试

1. `PartialReady` 时在方案区展示“加载失败 + 重试按钮”。
2. 点击重试触发 `reloadCurrentProjectCases()`。
3. 成功转 `Ready`，失败保持 `PartialReady` 并更新错误信息。

## 数据模型设计

## 1) 项目列表项（UI）

- 来源：`/api/project/page -> PagedDataJson.data[]`
- 映射字段：
  - `id <- ProjectItemJson.id`
  - `name <- ProjectItemJson.name`
  - `description <- ProjectItemJson.desc`
  - `caseCount <- ProjectItemJson.num`（缺省 0）
  - `status <- ProjectItemJson.status`（枚举映射为文案，未知回退“进行中”）
  - `createTime <- ProjectItemJson.created_at`（解析失败回退当前时间）

## 2) 项目详情（当前项目）

- 来源：`/api/project/detail -> ProjectDetailJson`
- 重点字段：`info / algorithm_params / db_conn_str / menu`

## 3) 方案列表（右侧项目方案）

- 来源：`/api/case/list`
- 映射到 `CaseItem`：
  - `id / name / projectId / createTime / status / typeName`
  - 统计字段来自 `case_count` 聚合，空值按 0

## 4) 基础方案（临时规则）

- 来源：`ProjectDetailJson.menu`
- 归类方式：
  - 道路：`road*` 或语义对应“道路”
  - 公交：`bus*` 或语义对应“公交”
  - 其他：剩余全部
- 固定展示顺序：**道路 -> 公交 -> 其他**
- 同时供左侧基础数据区与右侧项目方案区使用同一份归类数据。

## 状态机设计

- `ProjectListState`：`Idle | Loading | Ready | Error`
- `ProjectOpenState`：`Idle | LoadingDetail | LoadingCases | Ready | PartialReady | Error`

语义约束：

- `PartialReady` 仅表示 `detail` 可用、`case list` 不可用。
- `Error`（打开项目）用于 `detail` 失败或关键数据不可用场景。

## 事件通知设计（UIGlobalState）

新增 UI 事件（示例）：

- `ProjectListLoadingStarted`
- `ProjectListUpdated`
- `ProjectListLoadFailed`
- `ProjectCreated`
- `ProjectCreateFailed`
- `ProjectOpenStateChanged`
- `ProjectCasesLoadFailed`
- `ProjectCasesReloaded`

事件参数使用 `QVariantList` 传递最小必要信息（如 `projectId`、错误消息、状态枚举值）。

## 文件级改动清单

### `src/uis/uiglobalstate.h/.cpp`

- 扩展项目域状态、动作方法、映射工具与事件定义。
- 增加项目列表、项目详情、方案列表、基础方案缓存及当前项目标识。

### `src/uis/homepanel.h/.cpp`

- 移除本地模拟数据注入路径（`seedProjects` 退出主链路）。
- 改为监听 `UIGlobalState` 项目列表状态并同步 `ProjectItemModel`。
- 新建项目改为调用 `UIGlobalState::createProject`。
- 创建成功后弹出“立即打开/稍后”交互。
- 保持并适配“我的项目”弹窗：
  - 按项目名称搜索基于真实项目列表过滤；
  - “新建项目”入口复用统一创建链路与成功后交互。

### `src/uis/mainwindow.h/.cpp`

- 删除 `buildDemoCases` 依赖路径。
- 项目打开改为由 `UIGlobalState::openProject` 驱动状态。
- 仅在可打开状态下创建或复用 `CasePanel` 标签页。

### `src/uis/casepanel.h/.cpp`

- 新增 `setProjectId(int)` 绑定项目上下文。
- 监听当前项目状态渲染：
  - `Ready`：展示详情 + 基础方案 + 方案列表。
  - `PartialReady`：展示详情 + 基础方案 + 方案失败提示与重试按钮。
- 左右区域共享同一份基础方案数据源。

## 交互细节确认结论

1. 新建项目成功后：弹“立即打开 / 稍后”。
2. 打开项目时若 `detail` 成功但方案列表失败：允许进入项目页，显示可重试提示。
3. 方案列表主接口：`/api/case/list`。
4. `detail.menu` 临时归入基础方案并按“道路 -> 公交 -> 其他”展示。
5. 基础方案在左侧基础数据区与右侧项目方案区同时展示。
6. “我的项目”界面中的按名称搜索与新建入口必须按设计保留并完成真实数据适配。

## 风险与缓解

1. **接口字段变化风险**：新增映射层与回退策略，避免 UI 直接依赖原始 JSON 细节。
2. **状态不同步风险**：统一通过 `UIGlobalState` 发事件与更新状态，禁止页面跨层互相写状态。
3. **部分可用分支复杂度**：显式状态机与清晰渲染分支，降低条件分散。

## 验收标准

1. 首页项目数据来自 `/api/project/page`，不再出现固定模拟项目。
2. 新建项目调用 `/api/project/add`，成功后弹出“立即打开/稍后”。
3. 打开项目后按顺序调用 `detail` 与 `case list`，并在项目页展示真实数据。
4. `detail.menu` 归类为基础方案，顺序固定为道路、公交、其他。
5. 方案列表失败时页面仍可用，支持重试并可恢复。
6. 页面消息通知由 `UIGlobalState` 统一驱动。
7. “我的项目”弹窗中：
   - 按项目名称搜索可实时过滤真实数据；
   - “新建项目”入口可正常创建并触发“立即打开/稍后”。

## 后续计划入口

本设计确认后，下一步将进入实现计划编写，按模块拆分为：

1. 状态中心改造（`UIGlobalState`）
2. 首页接入与新建交互
3. 项目打开链路与标签页切换
4. 项目页展示与失败重试
5. 联调与验收

