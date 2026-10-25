# 原型与设计稿

## 桌面系统在线原型

- 地址：`http://gis.cdrmt.com/#/start`
- **勿用 HTTPS**（证书不受信任）
- **首页**：顶栏 + 欢迎区 + 5 业务卡片 + 「快速开始 / 项目管理 / 帮助文档」
- **示例项目**：「某大学附近设施建设专案」→ `http://gis.cdrmt.com/#/project/project-1`
- **项目页**：左「项目方案 / 基础数据」+ 侧栏树；`QSplitter` 可调宽；右侧方案卡片列表 ↔ 方案详情（地图/双屏）与基础数据地图互斥

对照表见 [`prototype_uidesign_mapping.md`](prototype_uidesign_mapping.md)（如有更新请维护该文件）。

## Mockplus 设计稿

- 地址：`https://app.mockplus.cn/app/I6Uzgp_Ds/develop/design/sy2i6KBQy`
- 标题：城市交通GIS项目 - 摹客
- 需登录；优先复用持久会话目录：`docs/mockplus_browser_profile/`
- 备用（已 `.gitignore`）：`docs/mockplus_cookies.json`、`docs/mockplus_storage_state.json`
- **勿提交**凭证文件；**勿在回复中展示** Cookie
- Playwright：优先 `launchPersistentContext('docs/mockplus_browser_profile', …)`；单独导入 Cookie 可能触发「已在其它设备登录」限制
