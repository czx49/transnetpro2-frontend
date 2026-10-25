# 架构与项目结构

## 概述

TransNetPro2 Phase2 是交通网络分析与规划桌面应用（Qt5/C++ + QGIS + GDAL），二期聚焦首页与方案模块 UI。

## 技术栈

- **Qt 5.15** — Widgets, Network, Xml, Svg, Concurrent, Sql
- **QGIS** — 地图与空间操作
- **GDAL 3.12+**
- **C++17** / **CMake 3.16+**
- **Log4Qt**

## 目录结构（概要）

```
Phase2/
├── CLAUDE.md              # AI 入口（精简）
├── README.md
├── build/                 # CMake 构建输出
├── out/                   # bin/lib
└── src/
    ├── main.cpp
    ├── cmake/             # gdal, qgis, webview2, variants…
    ├── publicctrls/       # TableModelBase, TreeModelBase, phase2comboboxutil
    ├── homectrls/         # 首页卡片、项目列表
    ├── casectrls/         # 方案树、详情、WFS、导入导出辅助
    ├── uis/               # MainWindow, CasePanel, CaseWorkPanel（壳+会话缓存）, AI 对话…
    ├── icons/
    ├── httpapis/          # REST 封装
    ├── jsonreader/
    ├── qrequest/
    ├── themechange/
    ├── tnmapwidget/
    ├── launcher/          # tnp2_launcher + tnp2.env 启动器
    └── …
```

## 应用程序入口

### 发布运行（tnp2_launcher）

1. 构建产物位于 `out/bin/`：`tnp2_launcher.exe`、`tnp2.env`、`tnp2_app.exe`
2. 启动器读取 `tnp2.env`，设置环境变量后启动主程序（无控制台窗口）
3. 包布局（Layout A）：`bin/` 放 exe、depends、QGIS `plugins/`（providers）与 `resources/` 等；包根放 Qt `plugins/`、`share/`、`gdalplugins/`

### 主程序（main.cpp）

初始化顺序：

1. 高 DPI（Windows Per-Monitor V2）
2. `QgsApplication`
3. 环境变量（PROJ_LIB、PATH、QGIS 插件）
4. Log4Qt（`log4qt.properties`）
5. 翻译（`translations/*.zh_CN.qm`）
6. `ThemeChange::loadTheme`
7. 坐标系 EPSG:4326
8. `MainWindow` 最大化显示

## 主窗口

- `MainWindow` 继承 `QWidget`：顶部导航 + `QTabBar` + `QStackedWidget`
- 窗口/品牌标题：**城市道路交通网络预测与分析平台**；品牌区显示 `:/icons/icon.ico`
- 首页为固定起始页签；项目页动态开闭；同项目复用已有页签

## 数据模型基类

**TableModelBase**（`QAbstractTableModel` 模板）

- 子类实现 `headers()`、`colValue()`
- 数据：`datas()` / `setDatas()` / 增删改
- 示例：`ProjectItemModel`、`BaseDataTableModel`

**TreeModelBase**（`QAbstractItemModel`）

- 节点 `TreeItemBase` + `RoleToData`
- 支持 `Qt::CheckState`
- 示例：`CaseTreeModel`

## 首页模块（概要）

- `FeatureCardWidget` — 5 类业务入口卡片
- `ProjectListWidget` — 项目卡片、搜索、右键
- `NewProjectDialog` / `ProjectItemModel`

UI 细节见 [`ui-conventions.md`](ui-conventions.md)。

## 编码规范

完整规范见 [`CodingStandards.md`](CodingStandards.md)。硬性要点：

- 源文件 UTF-8 无 BOM，换行 **CRLF**
- 含中文等非 ASCII 的字面量使用 **`u8` 前缀**（`tr()` 除外）
- 成员 `m` 前缀、4 空格缩进、行宽 ≤ 120、虚函数重写加 `override`
