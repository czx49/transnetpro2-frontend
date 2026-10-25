# 项目文档维护约定

本文档定义 **CLAUDE.md** 与 **docs/** 的分工，供 AI 助手与开发者共同遵守。

## 原则

| 位置 | 用途 | 篇幅 |
|------|------|------|
| [`CLAUDE.md`](../CLAUDE.md) | AI 入口：项目是什么、如何构建、文档索引、**不可违反的简短约定** | 保持精简（建议 &lt; 120 行） |
| [`docs/`](README.md) | 架构、模块行为、UI 规范、API、设计稿等**可查阅的详细说明** | 按需增长 |
| [`README.md`](../README.md) | 面向人的项目说明与功能概览 | 可较长，但避免与 docs 重复维护同一段落 |

## 更新规则

1. **新增或变更功能说明**：优先写入 `docs/` 下对应专题文档；仅在 `CLAUDE.md` 增加一行索引或一句摘要（若确属全局硬性约定）。
2. **禁止**把大段架构、模块流程、控件规范直接堆进 `CLAUDE.md`。
3. **同步修改**：若删除/重命名 docs 文件，必须更新 `docs/README.md` 与 `CLAUDE.md` 中的索引链接。
4. **API 文档**：线上同步来源为 `http://gisapi.jiheshop.com/api/config/api`；本地以 [`api.md`](api.md) + [`swagger-interfaces-current.json`](swagger-interfaces-current.json) 为准；与代码冲突时以 `src/httpapis/` 实现为准。
5. **不再维护** 根目录 `AGENTS.md`；内容已并入 `docs/` 与 `CLAUDE.md`。

## AI 使用方式

- 进入仓库后先读 **`CLAUDE.md`**。
- 涉及构建 → [`build-and-env.md`](build-and-env.md)
- 涉及方案/WFS/工作台 → [`case-module.md`](case-module.md)
- 涉及地图叠置/样式 → [`map-widget.md`](map-widget.md) + [`layer-style-factory.md`](layer-style-factory.md) + [`field-symbolization.md`](field-symbolization.md)
- 涉及图层过滤/定义查询 → [`layer-filter.md`](layer-filter.md)
- 涉及弹窗/输入框/图标 → [`ui-conventions.md`](ui-conventions.md)
- 涉及接口字段 → [`api.md`](api.md)
- 涉及原型/摹客 → [`design-prototype.md`](design-prototype.md)
- 涉及 C++ 风格 → [`CodingStandards.md`](CodingStandards.md)
