# 构建与环境

## 当前本机开发入口

当前仅维护联网版。在仓库根目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_online.ps1`，然后双击 `启动软件.cmd`。
脚本自动查找 Visual Studio 2022 自带的 CMake，默认使用同级开发包 `../tnpsource_developmentKit/developenv`；其它机器通过 `TNP_DEPS_ROOT` 或 `-DependencyRoot` 指定开发包目录。
联网开发的完整流程及服务器配置见 [online-development.md](online-development.md)。下述手动构建和发布包说明适用于自行配置环境的场景。

## CMake 配置（Windows x64）

```powershell
cd Phase2
cmake -S .\src -B .\build -A x64 `
    -DGDAL_DIR="D:/work/workspace/developenv/gdal-dev/lib/cmake/gdal" `
    -DQGIS_DIR="D:/work/workspace/developenv/qgis-ltr-dev" `
    -DCMAKE_PREFIX_PATH="D:/work/workspace/developenv/Qt5" `
    -DWEBVIEW2_SDK_DIR="D:/work/workspace/developenv/WebView2"
```

## 构建

```powershell
cmake --build .\build --config Debug
cmake --build .\build --config Release
```

## 可选 CMake 选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `THEMES_DEBUG_MODE` | `OFF` | 从本地 QSS 调试主题 |
| `NO_TRANSLATION` | — | 禁用翻译 |
| `ENABLE_ZOOM_TO_LAYER` | `OFF` | 图层树右键「缩放至图层」；开启后为 `tnp2_app` 定义 `TNP_ENABLE_ZOOM_TO_LAYER` |
| `ENABLE_COMMUNITY_GEN_DEBUG_SHP` | `OFF` | 生成交通小区时输出中间 shp 至 `{appDir}/data/`；开启后定义 `TNP_COMMUNITY_GEN_DEBUG_SHP` |
| `ENABLE_COMMUNITY_GEN_FILL_HOLES` | `ON` | 生成交通小区时对缓冲掩膜（clip_mask）填洞（移除内环）；开启时定义 `TNP_COMMUNITY_GEN_FILL_HOLES`，置 `OFF` 则保留空洞 |
| `ENABLE_AUTO_WFS_CAPS_REFRESH` | `OFF` | 图层列表/业务变更后自动强制刷新 WFS GetCapabilities；开启后定义 `TNP_ENABLE_AUTO_WFS_CAPS_REFRESH` |

启用缩放至图层示例：

```powershell
cmake -S .\src -B .\build -A x64 `
    -DGDAL_DIR="D:/work/workspace/developenv/gdal-dev/lib/cmake/gdal" `
    -DQGIS_DIR="D:/work/workspace/developenv/qgis-ltr-dev" `
    -DCMAKE_PREFIX_PATH="D:/work/workspace/developenv/Qt5" `
    -DWEBVIEW2_SDK_DIR="D:/work/workspace/developenv/WebView2" `
    -DENABLE_ZOOM_TO_LAYER=ON
```

启用生成交通小区中间 shp 调试：

```powershell
cmake -S .\src -B .\build -A x64 `
    -DGDAL_DIR="D:/work/workspace/developenv/gdal-dev/lib/cmake/gdal" `
    -DQGIS_DIR="D:/work/workspace/developenv/qgis-ltr-dev" `
    -DCMAKE_PREFIX_PATH="D:/work/workspace/developenv/Qt5" `
    -DWEBVIEW2_SDK_DIR="D:/work/workspace/developenv/WebView2" `
    -DENABLE_COMMUNITY_GEN_DEBUG_SHP=ON
```

启用图层变更后自动刷新 WFS Capabilities：

```powershell
cmake -S .\src -B .\build -A x64 `
    -DGDAL_DIR="D:/work/workspace/developenv/gdal-dev/lib/cmake/gdal" `
    -DQGIS_DIR="D:/work/workspace/developenv/qgis-ltr-dev" `
    -DCMAKE_PREFIX_PATH="D:/work/workspace/developenv/Qt5" `
    -DWEBVIEW2_SDK_DIR="D:/work/workspace/developenv/WebView2" `
    -DENABLE_AUTO_WFS_CAPS_REFRESH=ON
```

## 运行时环境变量

**本机开发**：双击仓库根目录的 `启动软件.cmd`。**完整发布包**：双击 `out/bin/tnp2_launcher.exe`（或为其创建快捷方式）。发布包启动器读取同目录 `tnp2.env`，在启动 `tnp2_app.exe` 前注入 PATH、GDAL、QGIS 等环境变量。

`WorkSpace/start/*.bat|vbs` 脚本已废弃，仅作参考；请改用启动器 + env 文件。

### tnp2.env 占位符

| 占位符 | 含义 |
|--------|------|
| `%LAUNCHER_DIR%` | `tnp2_launcher.exe` 所在目录（通常即 `out/bin`） |
| `%PACKAGE_ROOT%` | `%LAUNCHER_DIR%` 的上一级（通常即 `out/`） |
| `%PATH%` | 启动器进程当前 PATH |

### tnp2.env 主要键

| 键 | 说明 |
|----|------|
| `TARGET_EXE` | 目标程序文件名（如 `tnp2_app.exe`）；Debug 仅存在 `tnp2_appd.exe` 时自动回退 |
| `WORKING_DIR` | 工作目录，默认 `%LAUNCHER_DIR%` |
| `PATH` | DLL 搜索路径（`bin`、`depends`、`WebView2`、包根 `gdalplugins`） |
| `PROJ_LIB` / `GDAL_DATA` / `GDAL_DRIVER_PATH` | GDAL/PROJ 数据与驱动（包根 `share/`、`gdalplugins/`） |
| `QGIS_PREFIX_PATH` | `%LAUNCHER_DIR%`（`bin/`；QGIS resources/svg/i18n 与 exe 同级） |
| `QGIS_PLUGINPATH` | `%LAUNCHER_DIR%\plugins`（QGIS providers，与 exe 同级） |
| `QT_PLUGIN_PATH` | `%PACKAGE_ROOT%\plugins`（Qt platforms/imageformats 等；另见 `bin/qt.conf`） |

**发布包布局（Layout A）**：

```text
packageRoot/
├── bin/                 # 启动器、主程序、depends、QGIS providers/resources、data、WebView2、knowledge
├── plugins/             # 仅 Qt 插件
├── gdalplugins/
└── share/proj, share/gdal
```

模板见 [`src/launcher/tnp2.env.in`](../src/launcher/tnp2.env.in)。

**IDE 直启**：Visual Studio / Qt Creator 调试 `tnp2_app` 时，由 [`msvcenvs.cmake`](../src/cmake/msvcenvs.cmake) 注入调试 PATH；主程序 [`main.cpp`](../src/main.cpp) 内 `setupEnvironment()` 作为兜底。

程序启动时（主程序内）还会设置：

- `PROJ_LIB` → `appDir/../share/proj`
- QGIS：`setPrefixPath(appDir, false)` + `setPluginPath(appDir/plugins)` + `setPkgDataPath(appDir)`（禁止 `useDefaultPaths=true`，以免 Windows 裁掉 `/bin`）
- `PATH` → 追加 `appDir/WebView2`
- Qt 插件：`addLibraryPath(appDir/../plugins)`

## 工具约定

- PowerShell `Get-Content` 读取文本时，未指定编码则**必须**使用 UTF-8：
  `Get-Content -Path docs\CodingStandards.md -Encoding UTF8`

## 开发包路径（参考）

| 组件 | 路径 |
|------|------|
| QGIS | `D:\work\workspace\developenv\qgis-ltr-dev` |
| GDAL | `D:\work\workspace\developenv\gdal-dev` |
| Qt5 | `D:\work\workspace\developenv\Qt5` |
| WebView2 SDK | `D:\work\workspace\developenv\WebView2` |
