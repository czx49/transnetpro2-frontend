# 图标命名映射表

本文档记录设计图原始图标名与代码内资源名的对应关系。由于 `src/icons.qrc` 使用的是 `<qresource prefix="/icons">` 且文件位于 `icons/` 子目录，代码中统一优先使用 `:/icons/icons/英文名@2x.png`；若某个图标没有 `@2x` 资源，再退回普通版本。

## 功能卡片图标

| 中文名称 | 英文名称 | 说明 |
|---------|---------|------|
| 组 16 | `urban_planning` | 城市规划 |
| 组 16_1 | `traffic_planning` | 交通规划 |
| 组 16_2 | `traffic_control` | 交通管控 |
| 组 16_3 | `traffic_policy` | 交通政策 |
| 组 16_4 | `public_transport` | 公共交通 |

## 项目与方案操作图标

| 中文名称 | 英文名称 | 说明 |
|---------|---------|------|
| 新建项目 | `add_project` | 新建项目 |
| 新建项目_1 | `add_project_1` | 新建项目变体 |
| 删除 | `delete` | 删除 |
| 删除_1 | `delete_1` | 删除变体 1 |
| 删除_2 | `delete_2` | 删除变体 2 |
| 删除_3 | `delete_3` | 删除变体 3 |
| 搜索 (3) | `search` | 搜索 |
| 查看 | `view` | 查看 |
| 查看_1 | `view_1` | 查看变体 1 |
| 查看_2 | `view_2` | 查看变体 2 |
| 重命名 | `rename` | 重命名 |
| 复制 | `copy` | 克隆/复制 |

## 导航与布局图标

| 中文名称 | 英文名称 | 说明 |
|---------|---------|------|
| 导航-02 | `navigation` | 导航/定位 |
| 下拉 拷贝 3 | `dropdown` | 下拉箭头 |
| 更多 | `more` | 更多菜单 |
| 关闭 (2) | `close` | 关闭 |
| 关闭 (2) 拷贝 | `close_1` | 关闭变体 |
| 选中 (2) | `checked` | 选中状态 |
| 设计方案对比 | `compare_view` | 双屏对比视图 |

## 文件与内容图标

| 中文名称 | 英文名称 | 说明 |
|---------|---------|------|
| open-folder | `folder_open` | 打开项目 |
| open-folder_1 | `folder_open_1` | 文件夹变体 |
| 文档 | `document` | 文档 |
| 数据库 (1) | `database` | 数据库 |
| 图形 | `chart` | 图表/分析 |

## 联系方式与通用图标

| 中文名称 | 英文名称 | 说明 |
|---------|---------|------|
| 符号-邮箱 | `email` | 邮箱 |
| 符号-邮箱 拷贝 | `email_1` | 邮箱变体 1 |
| 符号-邮箱 拷贝_1 | `email_2` | 邮箱变体 2 |
| 符号-电话 | `phone` | 电话 |
| 符号-位置 | `location` | 地址 |
| 用户 (2) | `user` | 用户 |
| 教学模式 | `teaching_mode` | 教学案例 |
| 设置 (1) | `settings` | 设置 |

## 业务工作台图标（WorkSpace）

| 中文名称 | 英文名称 | 说明 |
|---------|---------|------|
| 机动车 | `workspace_motor` | 机动车分析模块（默认） |
| 机动车checked | `workspace_motor_checked` | 机动车分析模块（选中） |
| 公交 | `workspace_bus` | 公交运行分析模块（默认） |
| 公交checked | `workspace_bus_checked` | 公交运行分析模块（选中） |
| 交通 | `workspace_traffic` | 慢行交通分析模块（默认） |
| 交通checked | `workspace_traffic_checked` | 慢行交通分析模块（选中） |
| 数据支撑 | `workspace_data_support` | 数据支撑集检查标题 |
| 流程 | `workspace_flow` | 算法/改造方案流程标题 |
| 运行 | `workspace_run` | 流程步骤运行按钮 |
| ai助手 | `workspace_ai_assistant` | 流程步骤 AI 智能助手按钮（显示/隐藏智能对话窗体） |
| 发送 | `workspace_send` | 智能对话窗体发送按钮 |

代码中通过 `CaseIconUtils::workbenchDualStateIcon()` / `workbenchSingleStateIcon()` 加载，资源路径为 `:/icons/icons/workspace_{stem}.png`。

## 树分支箭头（深色侧栏）

| 中文名称 | 资源路径 | 说明 |
|---------|---------|------|
| 收起 / 普通 | `:/icons/arrow_right.png` | 图层树分支未展开 |
| 收起 / hover | `:/icons/arrow_right_hover.png` | 鼠标悬浮分支箭头 |
| 展开 / 普通 | `:/icons/arrow_down.png` | 图层树分支已展开 |
| 展开 / hover | `:/icons/arrow_down_hover.png` | 鼠标悬浮分支箭头 |

由 `Phase2Ui::darkSidebarTreeBranchStyleRules` 应用于 `CaseTreeView` / `BaseDataTableView`。

## 当前缺失图标与临时映射

以下图标在本地图标资源中未找到独立文件，当前已在 `src/icons.qrc` 中通过 `icons/...` alias 临时映射到相近图标，保证编译与界面完整性。后续如补齐正式资源，可直接替换 alias。

| 代码名 | 当前临时映射 | 备注 |
|-------|-------------|------|
| `zoom_in` | `navigation` | 缺少独立放大图标 |
| `zoom_out` | `dropdown` | 缺少独立缩小图标 |
| `edit` | `view_1` | 缺少独立编辑图标 |
| `measure` | `chart` | 缺少独立测量图标 |
| `statistics` | `chart` | 缺少独立统计图标 |
| `connect` | `compare_view` | 缺少独立连接图标 |
| `share` | `copy` | 缺少独立分享图标 |
| `print` | `document` | 缺少独立打印图标 |
| `waveform` | `chart` | 缺少独立波形图标 |

## 使用说明

```cpp
QPushButton* button = new QPushButton(u8"新建项目");
button->setIcon(QIcon(QStringLiteral(":/icons/icons/add_project@2x.png")));
```

Qt 会自动优先加载对应的 `@2x` 资源。
