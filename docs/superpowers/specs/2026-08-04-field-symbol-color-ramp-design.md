# 字段分级符号化：基准色改为色带

**日期：** 2026-08-04  
**状态：** 已实现（方案 A）  
**范围：** `CaseSymbolStyleDialog` 字段分级渲染的色彩来源

## 1. 目标

将符号化设置中分级色的生成来源由「基准色 + HSL 偏移」改为「预置色带采样」，并支持反转色带；保存/加载/清除/自动恢复链路保持完整且兼容旧数据。

## 2. 已确认决策

| 项 | 决策 |
|----|------|
| 基准色控件 | 完全移除，改为色带选择 |
| 色带清单 | 8 套：`RdYlGn` / `Reds` / `Spectral` / `RdYlBu` / `Blues` / `YlOrRd` / `YlGnBu` / `PuBuGn` |
| 反转 | 支持「反转色带」勾选 |
| 存储 | 方案 A：JSON 元数据 + 烘焙颜色 |
| 单级手动改色 | 保留；改色带/反转/分级数时整表重算 |

## 3. 存储（方案 A）

- 表结构 `SymbolColors` **不变**；规则仍写入 `color_value` JSON。
- `FieldColorRule` 增加：
  - `colorRampName`（默认 `RdYlGn`）
  - `rampInverted`（默认 `false`）
- 继续持久化 `colors[]`（各级 `#rrggbb` + min/max），作为渲染与自动恢复的唯一颜色来源。
- `LayerStyleRenderer` / `TNMapWidget::updateLayerSymbolColor` **不依赖**色带元数据。
- 旧 JSON 缺字段：按默认值解析；加载后 UI 显示默认色带，不强制重算已烘焙色。

## 4. 行为

| 操作 | 是否重算分级色 |
|------|----------------|
| 改色带 / 反转 / 分级数 | 是，立即预览 |
| 改透明度 / 线宽 | 否 |
| 点击分级列表改单色 | 否 |
| 应用 / 保存 | 不强制重算（规则无效时再生成） |

采样：`t = i / (n - 1)`（`n==1` 时 `t=0`），与 `LayerStyleFactory` 一致；反转时对 ramp 调用 `invert()` 后再采样。

## 5. 主要改动文件

- `src/tnmapwidget/symbolrendererconfig.h/.cpp` — 扩展色带 + 名称列表 API
- `src/tnmapwidget/layerstylepersistence.h/.cpp` — JSON 字段读写 / `reset`
- `src/casectrls/casesymbolstyledialog.h/.cpp` — UI 与生成逻辑
- `docs/field-symbolization.md` — 文档同步

## 6. 非目标

- 不引入 QGIS 原生色带选择器
- 不改 SQLite 表结构
- 不把色带挂到 `QgsGraduatedSymbolRenderer`（仍用离散烘焙色）
- 不做 Bool/String 分类渲染扩展
