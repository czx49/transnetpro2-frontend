# TransNetPro2 Phase2

## 项目概述

TransNetPro2 Phase2 是 TransNetPro 项目的二期工程，专注于实现首页模块和方案模块的界面功能。

**当前开发方向：联网版。离线版计划已停止，当前源码通过 HTTP API、WFS 和后端分析服务工作。**

前端优化需要完整保留后端对接与真实数据链路。仓库同时维护接口客户端、响应模型、API 契约以及开发包中已有的算法源码和联调示例，见 [后端联调与算法资产](backend/README.md)。

在本机双击根目录的 **`启动软件.cmd`**。首次构建或修改 C++/界面资源后，先运行 **`编译联网版.cmd`**，成功后再启动。
启动后使用软件后端分配的账号登录；GitHub 账号仅用于代码协作。

完整操作步骤、服务器配置、常见问题及 Git 上传命令见 [联网开发与协作指南](docs/online-development.md)。

## 开发环境配置

- **开发工具目录**: `D:\work\workspace\developenv`
- **QGIS开发包**: `D:\work\workspace\developenv\qgis-ltr-dev`
- **GDAL开发包**: `D:\work\workspace\developenv\gdal-dev`
- **Qt5开发包**: `D:\work\workspace\developenv\Qt5`
- **编译输出目录**: `Phase2/build`

## 构建命令

### CMake 配置（Windows x64）

```powershell
cd Phase2
cmake -S .\src -B .\build -A x64 `
    -DGDAL_DIR="D:/work/workspace/developenv/gdal-dev/lib/cmake/gdal" `
    -DQGIS_DIR="D:/work/workspace/developenv/qgis-ltr-dev" `
    -DCMAKE_PREFIX_PATH="D:/work/workspace/developenv/Qt5"
```

### 构建

```powershell
# Debug 构建
cmake --build .\build --config Debug

# Release 构建
cmake --build .\build --config Release
```

### CMake 可选选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `THEMES_DEBUG_MODE` | `OFF` | 从本地 QSS 调试主题 |
| `NO_TRANSLATION` | — | 禁用翻译 |
| `ENABLE_ZOOM_TO_LAYER` | `OFF` | 图层树右键「缩放至图层」；开启后编译宏 `TNP_ENABLE_ZOOM_TO_LAYER` |
| `ENABLE_AUTO_WFS_CAPS_REFRESH` | `OFF` | 图层变更后自动强制刷新 WFS GetCapabilities；开启后编译宏 `TNP_ENABLE_AUTO_WFS_CAPS_REFRESH` |

启用「缩放至图层」时在配置阶段追加：

```powershell
-DENABLE_ZOOM_TO_LAYER=ON
```

图层需已在地图中加载（侧栏勾选可见）后，右键菜单才会将视图缩放到该图层范围；行为对齐 QGIS「缩放至图层」。

启用图层变更后自动刷新 WFS Capabilities：

```powershell
-DENABLE_AUTO_WFS_CAPS_REFRESH=ON
```

默认关闭；开启后导入、质心/期望线等图层列表实质变更会异步强制刷 Caps。详见 [`docs/case-module.md`](docs/case-module.md)、[`docs/build-and-env.md`](docs/build-and-env.md)。

## 目录结构

```
Phase2/
├── CMakeLists.txt              # 主CMake配置
├── README.md                   # 项目说明
├── build/                      # CMake构建目录
├── out/                        # 输出目录
│   └── bin/                    # 可执行文件和依赖
└── src/                        # 源代码目录
    ├── CMakeLists.txt          # 源码CMake配置
    ├── main.cpp               # 程序入口
    ├── cmake/                 # CMake配置模块
    │   ├── gdal.cmake
    │   ├── qgis.cmake
    │   ├── webview2.cmake
    │   ├── msvcenvs.cmake
    │   ├── variants.cmake
    │   └── sourcegroup.cmake
    │
    ├── log4qt/               # 日志模块
    ├── logprinterex/         # 日志打印扩展
    ├── qrequest/             # 网络请求工具
    ├── httpapis/             # HTTP API客户端
    ├── jsonreader/           # JSON解析工具
    ├── singleton/            # 单例模板
    ├── themechange/          # 主题切换模块
    ├── utils/                # 工具函数
    ├── tnmapwidget/          # 地图控件
    ├── publicctrls/          # 公共控件目录
    │   ├── tablemodelbase.h      # 表格模型基类模板
    │   ├── treemodelbase.h       # 树模型基类
    │   ├── treemodelbase.cpp
    │   ├── tableview.h           # 自定义表格视图
    │   └── tableview.cpp
    │
    ├── homectrls/            # 首页模块控件
    │   ├── featurecardwidget.h   # 功能卡片控件
    │   ├── featurecardwidget.cpp
    │   ├── projectlistwidget.h   # 项目列表控件
    │   ├── projectlistwidget.cpp
    │   ├── newprojectdialog.h    # 新建项目对话框
    │   ├── newprojectdialog.cpp
    │   ├── projectitemmodel.h    # 项目数据模型
    │   ├── projectitemmodel.cpp
    │   └── CMakeLists.txt
    │
    ├── casectrls/            # 方案模块控件
    │   ├── basedatatableview.h/cpp
    │   ├── basedatamodel.h/cpp
    │   ├── casetreeview.h/cpp
    │   ├── casetreemodel.h/cpp
    │   ├── casedetailpanel.h/cpp   # 方案详情（标准视图 + 双屏对比 + 悬浮工具栏）
    │   ├── casemaptoolbarwidget.h/cpp
    │   ├── casemapattributewidget.h/cpp
    │   ├── casemaputils.h/cpp
    │   ├── caseiconutils.h/cpp
    │   ├── casewfsmapcontroller.h/cpp   # WFS 2.0 图层加载
    │   ├── casedrcsvhelper.h/cpp        # CSV 导入 table 推导
    │   ├── casemenuhelper.h/cpp         # 图层组 menu/list JSON 解析
    │   ├── caseitem.h / casetype.h
    │   ├── newcasedialog.h/cpp
    │   └── CMakeLists.txt
    │
    ├── uis/                  # 主界面模块
    │   ├── mainwindow.h/cpp
    │   ├── appwindow.h/cpp
    │   ├── homepanel.h/cpp
    │   ├── casepanel.h/cpp       # 方案工作台（列表/详情/基础数据）
    │   ├── caseworkpanel.h/cpp   # 方案详情右侧业务工作台（分析模块与算法流程）
    │   ├── aichatwidget.h/cpp    # 流程步骤智能对话（aiapi 同步对话 + 运行日志 timeline）
    │   ├── aiwordselectbar.h/cpp # 智能对话 L1/L2 选词条
    │   ├── aiconfigmodel.h/cpp   # aiconfig 树形选词模型
    │   ├── uiglobalstate.h/cpp
    │   ├── loginpanel.h/cpp
    │   ├── webview2dialog.h/cpp
    │   └── CMakeLists.txt
    │
    └── icons/                # 图标资源目录
        ├── icons.qrc             # 图标资源文件
        └── *.png                # 所有图标文件（74个）
```

## 已完成的工作

### 第一阶段：基础设施搭建

#### 1. 目录结构创建
- 创建了Phase2完整目录结构
- 包含所有核心模块目录

#### 2. 核心模块迁移（9个模块）
- log4qt - Log4Qt日志库
- logprinterex - 日志打印扩展
- qrequest - QRequest2网络请求库
- httpapis - HTTP API客户端
- jsonreader - JSON解析工具
- singleton - 单例模板
- themechange - 主题切换模块
- utils - 工具函数
- tnmapwidget - 地图控件模块

#### 3. 公共控件迁移
- tablemodelbase.h - 表格模型基类模板
- treemodelbase.h/cpp - 树模型基类
- tableview.h/cpp - 自定义表格视图

#### 4. CMake配置
- 根目录 CMakeLists.txt
- src/CMakeLists.txt（包含Qt5、GDAL、QGIS配置）
- uis/CMakeLists.txt（主界面模块配置）
- cmake配置文件（gdal.cmake, qgis.cmake等）

#### 5. 主程序入口
- main.cpp - 程序入口，包含高DPI支持、QGIS初始化、Log4Qt日志初始化
- mainwindow.h/cpp - 主窗口，支持首页和方案面板切换

#### 6. 项目文档
- README.md - 项目说明文档

### 第二阶段：图标资源管理

#### 1. 图标收集（74个图标文件）
- 首页图标：14个
- 我的项目图标：4个
- 新建项目图标：2个
- 方案图标：14个
- 导航展开图标：14个
- 包含@2x高分辨率版本

#### 2. 图标重命名（英文化）
- 功能卡片图标：urban_planning、traffic_planning、traffic_control、traffic_policy、public_transport
- 项目操作图标：add_project、delete、search、view、rename、copy
- 导航和布局图标：navigation、dropdown、more、close、checked
- 视图模式图标：compare_view
- 文件和文件夹图标：folder_open、document、database、chart
- 联系方式图标：email、phone、location、user
- 功能图标：teaching_mode、settings

#### 3. 图标资源文件
- 创建icons.qrc，统一管理所有图标资源
- 添加到CMakeLists.txt，编译时自动生成资源文件
- 使用路径：`:/icons/图标名.png`

### 第三阶段：首页模块实现

#### 1. 数据模型（ProjectItem、ProjectItemModel）
- ProjectItem：项目数据结构，包含id、name、description、createTime、caseCount、status、path字段
- ProjectItemModel：继承自TableModelBase，支持增删改查、搜索功能

#### 2. 功能卡片控件（FeatureCardWidget）
- 支持5种功能卡片类型：城市规划、交通规划、交通管控、交通政策、公共交通
- 支持鼠标悬停效果和点击事件
- 支持选中状态切换

#### 3. 我的项目界面（ProjectListWidget）
- 自定义委托实现卡片样式
- 支持项目搜索功能
- 支持项目选中状态切换
- 支持操作菜单（打开、编辑、删除）
- 支持双击打开项目

#### 4. 新建项目对话框（NewProjectDialog）
- 项目名称输入（必填）
- 项目描述输入（可选）
- 项目路径选择（可选）
- 表单验证和创建/取消操作

#### 5. 首页主界面（HomePanel）
- 展示5个功能卡片
- 展示快速开始模块（显示最近打开的项目）
- 展示管理面板（新建项目、打开项目、教学案例）
- 展示帮助文档模块（方案配置、数据库建立、基本原理、图形编辑）
- 展示联系我们模块（联系人信息）

### 第四阶段：方案模块实现

#### 1. 数据模型
- CaseItem：方案数据结构，包含id、name、typeId、projectId、createTime、status、linkCount、nodeCount、zoneCount、odPairCount、remark、parameters字段
- CaseDetailItem：方案详情数据结构，包含caseId、caseName、projectId、projectName、creator、createTime、updateTime、roadTypes、busTypes、otherTypes、algResults、layers字段
- BaseDataItem：基础数据结构，包含id、name、type、source、status、createTime、attributes、checked字段
- CaseTreeModel：继承自TreeModelBase，管理方案的树形结构，支持展开/收起、复选框
- BaseDataTableModel：继承自TableModelBase，管理基础数据列表，支持复选框

#### 2. 基础数据界面（BaseDataTableView）
- 复选框树：**所有图层 → 图层组 → 图层**，以及独立的 **工具** 分支
- 数据来自 `GET /api/project/detail` 的 `menu[]`，经 `CaseMenuHelper::buildBaseDataLayerGroups` 解析
- **工具** 白名单（`CasePanel::buildBaseToolNodes`）仅展示：**OSM数据下载**、**OD数据生成**、**基础数据分析**（按名称/URL 匹配，其余接口工具不显示）
- 图层组：三态勾选、单击行展开/收起（分支箭头为 Qt 默认样式）
- 图层叶子勾选通过 `layerStateChangedWithData` 联动 **`m_baseMapWidget`** 上的 WFS 矢量层
- **图层右键**：「导入」（`POST /api/files/drcsv`）、「导出 → 导出 Excel / 导出 TXT」（`GET /api/project/export`）；`case_id` 在基础数据侧栏传 `0`
- 显示工具列表（基础/现状数据分析、获取 OD、OSM 下载等）

#### 3. 项目方案界面（CasePanel、CaseTreeView）
- **CaseTreeView**：三级树 **方案 → 图层组 → 图层**；方案节点右键管理方案；**图层叶子**右键「导入」「导出 → Excel/TXT」（`case_id` 为所属方案 id）
- 方案图层来自 `/api/case/list` 每项的 `list[]`（与 `menu[]` 同结构：`children[]` 内 `name`、`value`→WFS typename、`level`、`val`）；`CasePanel::refreshCasesFromGlobalState` 经 `parseLayerGroupsFromCaseJson` 解析，避免仅走 `j_list()` 导致子图层丢失
- 图层组：三态勾选联动 WFS；单击行展开/收起；`modelReset` 后默认全部收起
- **CasePanel**：左侧「项目方案 | 基础数据」与右侧 **QSplitter**；右侧在卡片列表与 **CaseDetailPanel** 间切换
- **WFS 勾选显隐**：`CaseWfsMap`（QGIS WFS provider）；详情标准/双屏地图与基础数据地图隔离；进入详情时按树勾选同步图层；返回列表不清 WFS
- **CSV 导入 / 表导出**：`CaseDrcsvHelper` 推导 `table`；`CasePanel::requestDrcsvLayerImport(caseId, …)` → `HttpApis::uploadCSV`；`CasePanel::requestProjectTableExport(caseId, …)` → `HttpApis::exportProjectTable`，成功后 `FileDownloader` 下载
- 方案卡片、新建/对比；右侧 **CaseWorkPanel** 业务工作台（见下）

#### 3.0 业务工作台与智能对话（CaseWorkPanel / AIChatWidget）

| 组件 | 说明 |
|------|------|
| `CaseWorkPanel` | 嵌入方案详情右侧，可折叠；含机动车/慢行/公交模块选择、数据支撑集 gRPC 检查、基础/改造方案流程、报告与最终输出 |
| 应用标题 | 主窗口标题与导航栏品牌区：**城市道路交通网络预测与分析平台**（菜单与标题间显示 `icon.ico`） |
| 共享智能对话 | 基础方案宿主为 **交通问题诊断**（`traffic`），普通方案为 **方案评审**（`review`）；`network`/`suggestion` 日志与 AI 消息写入同一 `chatTimeline` |
| `AIChatWidget` | `setUseBackendAiApi(true)` 时发送走 `GET /api/grpc/aiapi`；展示 `content` + 可点击 **「查看详情」**（`download_url`）；消息区最小高度 **240px**；`refreshWorkPage` 后 `restoreChatTimeline` 恢复 |
| `AiWordSelectBar` | L1/L2 Chip 选词（`GET /api/config/aiconfig` 树形配置，L2 随 L1 级联）；「数据选择」占位且默认禁用 |
| 时间线 | `FlowStep::chatTimeline` 统一存储用户/助手/错误/运行日志（`AIChatMessage`）；AI 回调先写 timeline 再刷新 UI |
| 报告接口 | 基础方案：`/api/grpc/case_export` → `/api/case/report_view` 或 `/api/case/report_download`；普通方案：`/api/case/ptcase_export` |
| 方案建议 | `GrpcAiStreamClient` → `/api/grpc/aistream`（必填 `prompt`） |
| 成本效益 | 评审完成后 → `caseCostView` → `openHtmlReportWithFallback` |
| 显示控制 | 默认不显示；步骤行 **AI 智能助手** 按钮切换显示/隐藏；**运行** / 方案建议未显示时自动展开 |
| 交通诊断 UI | `traffic` 步骤完成后**不展示**步骤卡片内「分析结果:」区块 |
| 下载 | `FileDownloader` 支持 `dialogParent`，保存对话框挂主窗口 |
| 图标 | `workspace_ai_assistant`、`workspace_send` 等，见 `docs/icon_mapping.md` |

#### 3.1 WFS 与侧栏导入（概要）

| 能力 | 说明 |
|------|------|
| 加载方式 | `QgsVectorLayer` + provider `WFS`；`typename` = 接口 `list[].value` |
| 叠置顺序 | 业务层 `level`（`LEVEL_PROPERTY`）越小越在上；同 level 后添加的在上；底图在 `setLayers` 列表最末（最下） |
| 标记移除 | `customProperty("tnp_panel_wfs")`；`removePanelWfsLayers` |
| 导入接口 | `POST /api/files/drcsv`：`project_id`、`case_id`（方案 id / 基础数据 `0`）、`table`（小写表名）、`type`（`csv`/`txt`/`xlsx`） |
| 导出接口 | `GET /api/project/export`：同上 `case_id` 规则，`type` 为 `xlsx`/`txt`；本地文档在官方参数基础上补全 `case_id` |
| 图层样式 | WFS 矢量层加载后，`LayerStyleFactory` 按 `val` 或 `typename` 后缀识别图层类型并应用默认渲染（如 `other_od` 的 `demand` 五级分级）；详见 [`docs/layer-style-factory.md`](docs/layer-style-factory.md) |

详见 [`docs/case-module.md`](docs/case-module.md) 与 [`docs/map-widget.md`](docs/map-widget.md)；接口字段见 [`docs/api.md`](docs/api.md)（同步来源：`http://gisapi.jiheshop.com/api/config/api`，2026-06-09）；Apifox 导入见 [`docs/swagger-interfaces-current.json`](docs/swagger-interfaces-current.json)。AI 助手入口见 [`CLAUDE.md`](CLAUDE.md)。

#### 4. 方案详情（CaseDetailPanel，标准视图 + 双屏对比）
- **同一控件**内通过 `CaseMapToolbarWidget` 在标准视图与双屏对比之间切换；双屏为左右 `TNMapWidget`，共用悬浮工具栏与底栏逻辑
- 嵌入 CasePanel 时调用 `setBuiltInSchemeSidebarVisible(false)`，仅保留外层 CasePanel 的方案树，避免双方案侧栏
- 标题行：**方案标题居左**，「返回方案列表」居右（嵌入模式下显示），发出 `schemeListReturnRequested` 切回卡片列表
- **渲染 / 数据** 切换：地图渲染视图与属性表视图；底栏同一行左侧为分段切换，右侧为坐标系、坐标、比例尺（双屏对比底栏含导出）
- **数据视图下隐藏**悬浮的 `CaseMapToolbarWidget`，仅渲染视图显示，避免遮挡属性表
- 图例浮动在地图左下角；样式对齐设计稿（浅底地图块、深色底栏、卡片投影等）

#### 5. 地图控件（TNMapWidget）
- 继承 `QFrame`，使用 **NoFrame**，便于嵌入圆角地图卡片
- 底图由 `AppSession` 基础配置 `mapurl2`/`mapurl3` 加载（`[底图标注, 底图]`）
- **`updateCanvasLayers` 叠置规则**（与 QGIS `QgsMapCanvas::setLayers` 一致）：
  - 列表从前到后绘制，**后绘制在上**；**最后一项在最底层**
  - 底图 append 在列表末尾；业务层按 `level` 升序（level 越小越靠上），同 level 时较新加入的层更靠上
- `MapStatus`、底栏坐标/比例尺、`currentLayerChanged`、可选 `zoomToLayer`（`ENABLE_ZOOM_TO_LAYER`）
- **图层样式工厂**：WFS 业务层加载后由 `LayerStyleFactory` 覆盖几何默认符号；首期支持 `other_od`（`demand` 五级分级）。详见 [`docs/layer-style-factory.md`](docs/layer-style-factory.md)

## 后续工作（概要）

- 与后端接口、算法工具的深化对接与联调
- 集成测试与场景化验收（含大数据量与权限等）
- 持续对照原型与设计稿迭代 UI/UX

（历史 README 中重复的「待实现」条目已与当前代码状态不一致，故收敛为上述概要；具体任务以迭代计划为准。）

## 项目文档

| 文件 | 说明 |
|------|------|
| [`CLAUDE.md`](CLAUDE.md) | AI 助手入口（精简；详细内容见 `docs/`） |
| [`docs/README.md`](docs/README.md) | 文档索引与分类 |
| [`docs/documentation-policy.md`](docs/documentation-policy.md) | 文档分层与更新规则 |

## API 文档

| 文件 | 说明 |
|------|------|
| [`docs/layer-style-factory.md`](docs/layer-style-factory.md) | 图层样式工厂：WFS 加载后按 `val`/`typename` 自动渲染；`other_od` 的 `demand` 五级分级（对齐 Phase1 OD 分布） |
| [`docs/api.md`](docs/api.md) | 完整接口说明（2026-06-09 同步；含 `aiconfig` 树形、`aiapi` content/download_url、`editlog` 等；`/api/project/export` 本地补全 `case_id`） |
| [`docs/swagger-interfaces-current.json`](docs/swagger-interfaces-current.json) | Apifox / OpenAPI 导入文件（v1.0.4） |

**`case_id` 传参约定（导入/导出）**：处于某方案上下文（方案侧栏图层）时传对应方案 id；处于项目基础数据侧栏时传 `0`。

## 技术栈

- **Qt 5.15** - UI 框架
- **QGIS** - 地理信息系统
- **GDAL 3.12+** - 地理空间数据抽象库
- **C++17** - 编程语言标准
- **CMake 3.16+** - 构建系统

## 编码规范

项目严格遵循 `CodingStandards.md` 中定义的 C/C++ 编码规范：
- 文件编码：UTF-8 无签名
- 命名约定：驼峰命名法
- 成员变量：小驼峰 + m前缀
- 行宽：不超过 120 字符
- Doxygen 格式注释

## GitHub 协作

本仓库维护二期前端源码、界面资源和开发文档。Qt、QGIS、GDAL、WebView2 开发包需按本地环境单独配置。

编译产物、本地连接配置、AI 密钥和 Mockplus 登录缓存不纳入版本控制。离线启动入口与实现已移除；当前以 `scripts/build_online.ps1` 和 `scripts/launch_online.ps1` 为开发入口。

日常提交与推送：

```powershell
git status
git add <本次修改的文件>
git commit -m "说明本次修改"
git push
```
