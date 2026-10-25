# Phase2 文档索引

> 详细说明均在此目录；[`CLAUDE.md`](../CLAUDE.md) 仅保留 AI 入口与简短约定。维护规则见 [`documentation-policy.md`](documentation-policy.md)。

## 开发与构建

| 文档 | 内容 |
|------|------|
| [`build-and-env.md`](build-and-env.md) | CMake 配置/构建、可选编译项、运行时环境变量、Shell 工具约定 |
| [`online-development.md`](online-development.md) | 联网版启动、后端配置、修改代码、编译与 GitHub 上传 |
| [`package-layout.md`](package-layout.md) | 发布包 Layout A 内容清单（补齐/剔除/可选瘦身） |
| [`CodingStandards.md`](CodingStandards.md) | C/C++ 编码规范（完整版） |
| [`architecture.md`](architecture.md) | 技术栈、目录结构、入口初始化、主窗口与数据模型基类 |

## 业务与 UI

| 文档 | 内容 |
|------|------|
| [`case-module.md`](case-module.md) | 方案模块：图层树、WFS、导入导出、工作台、智能对话 |
| [`gen-traffic-community.md`](gen-traffic-community.md) | 基础数据「生成交通小区」功能实现 |
| [`map-widget.md`](map-widget.md) | TNMapWidget 底图叠置、状态信号、缩放至图层 |
| [`layer-style-factory.md`](layer-style-factory.md) | 图层样式工厂、`other_od` 与路段分析渲染 |
| [`field-symbolization.md`](field-symbolization.md) | 字段分级符号化弹窗、SQLite 持久化与自动恢复 |
| [`layer-filter.md`](layer-filter.md) | WFS 客户端过滤、路段 type 过滤、按属性过滤 |
| [`ui-conventions.md`](ui-conventions.md) | 首页 UI、图标、Frameless 深色弹窗套件、TnUi 提示/确认、主题与输入控件 |
| [`icon_mapping.md`](icon_mapping.md) | 图标资源路径与别名映射 |
| [`design-prototype.md`](design-prototype.md) | 在线原型与 Mockplus 设计稿访问 |
| [`prototype_uidesign_mapping.md`](prototype_uidesign_mapping.md) | 原型与 UI 对照（如有迭代请在此维护） |

## API

| 文档 | 内容 |
|------|------|
| [后端联调与算法资产](../backend/README.md) | 业务服务边界、已有算法源码、历史接口契约与数据验收 |
| [`api.md`](api.md) | 完整 REST/WFS 接口说明（含 Phase2 客户端约定） |
| [`swagger-interfaces-current.json`](swagger-interfaces-current.json) | Apifox / OpenAPI 2.0 导入 |

**`case_id` 约定**：方案侧栏图层导入/导出传方案 id；基础数据侧栏传 `0`。

## 其它

| 文档 | 内容 |
|------|------|
| [`documentation-policy.md`](documentation-policy.md) | 文档分层与更新规则 |
