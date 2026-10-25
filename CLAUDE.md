# CLAUDE.md — AI 助手入口

TransNetPro2 Phase2：Qt5/C++ + QGIS + GDAL 交通网络桌面应用（首页 + 方案模块）。

当前仅推进联网版，离线计划已停止。开发入口：`scripts/build_online.ps1` / `scripts/launch_online.ps1`；操作与协作步骤见 [`docs/online-development.md`](docs/online-development.md)。

> **文档分层**：本文件只保留必要摘要。**详细说明在 [`docs/`](docs/README.md)**；更新项目文档时须保持本文件精简，详见 [`docs/documentation-policy.md`](docs/documentation-policy.md)。

## 快速构建

```powershell
cd Phase2
cmake -S .\src -B .\build -A x64 `
    -DGDAL_DIR="D:/work/workspace/developenv/gdal-dev/lib/cmake/gdal" `
    -DQGIS_DIR="D:/work/workspace/developenv/qgis-ltr-dev" `
    -DCMAKE_PREFIX_PATH="D:/work/workspace/developenv/Qt5" `
    -DWEBVIEW2_SDK_DIR="D:/work/workspace/developenv/WebView2"
cmake --build .\build --config Debug
```

可选：`-DENABLE_ZOOM_TO_LAYER=ON`、`-DTHEMES_DEBUG_MODE=ON`、`-DENABLE_AUTO_WFS_CAPS_REFRESH=ON`。完整选项与环境变量 → [`docs/build-and-env.md`](docs/build-and-env.md)

## 文档索引（按需阅读）

| 主题 | 文档 |
|------|------|
| 文档维护规则 | [`docs/documentation-policy.md`](docs/documentation-policy.md) |
| 全部文档目录 | [`docs/README.md`](docs/README.md) |
| 架构 / 目录 / 入口 | [`docs/architecture.md`](docs/architecture.md) |
| 方案 / WFS / 工作台 / AI | [`docs/case-module.md`](docs/case-module.md) |
| 生成交通小区 | [`docs/gen-traffic-community.md`](docs/gen-traffic-community.md) |
| 地图叠置 / TNMapWidget | [`docs/map-widget.md`](docs/map-widget.md) |
| 图层样式工厂 | [`docs/layer-style-factory.md`](docs/layer-style-factory.md) |
| 字段分级符号化 | [`docs/field-symbolization.md`](docs/field-symbolization.md) |
| 图层过滤 / 定义查询 | [`docs/layer-filter.md`](docs/layer-filter.md) |
| 弹窗 / 输入框 / 图标 / 主题 | [`docs/ui-conventions.md`](docs/ui-conventions.md) |
| API 与 Swagger | [`docs/api.md`](docs/api.md) · [`docs/swagger-interfaces-current.json`](docs/swagger-interfaces-current.json) |
| 原型 / Mockplus | [`docs/design-prototype.md`](docs/design-prototype.md) |
| C++ 编码规范 | [`docs/CodingStandards.md`](docs/CodingStandards.md) |

## 全局硬性约定

1. **字符串**：含中文等非 ASCII 的字面量用 `u8"…"`（`tr()` 除外）；文件 UTF-8、换行 CRLF。
2. **弹窗**：必须继承 `FramelessDialog` + `Phase2Ui` 深色控件套件（`dialogShellStyleSheet` / primary/ghost 等），见 [`docs/ui-conventions.md`](docs/ui-conventions.md)。
3. **提示弹窗**：`TnUi::showInformation` / `showWarning` / `showCritical` / `confirmYesNo`，禁止 `QMessageBox::*`。
4. **ComboBox**：`Phase2Ui::setupThemedCombo(..., InputFieldTheme)`，勿只写弹层样式。
5. **API 与代码冲突**：以 `src/httpapis/httpapidefines.h`、`httpapis.cpp` 为准。
6. **`case_id`**：方案侧栏导入/导出传方案 id；基础数据侧栏传 `0`。
7. **Shell 读文件**：`Get-Content -Encoding UTF8`。
8. **Mockplus 凭证**：勿提交、勿在回复中展示 Cookie。

## 一句话架构

`MainWindow`（页签）→ `CasePanel`（方案树 / 基础数据 / 详情地图）+ `CaseWorkPanel`（流程与智能对话）；WFS 经 `CaseWfsMapController`；HTTP 经 `HttpApis` + `QRequest2`。
