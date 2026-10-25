# UI 与资源约定

## 主题

```cpp
ThemeChange::init();
ThemeChange::loadTheme("default");
```

- 生产：打包 QSS 资源
- 调试：`THEMES_DEBUG_MODE=ON` 从本地 QSS 加载

## 图标

- 目录：`src/icons/`，资源 `src/icons.qrc`，前缀 `/icons`
- 路径示例：`:/icons/icons/urban_planning@2x.png`
- 有 `@2x` 时**优先**使用 `@2x`
- **例外**：方案模块基础数据浮动工具条用普通 `.png`，不用 `@2x`
- 应用图标：`src/icon.ico` → `:/icons/icon.ico`
- 缺失图标：按 [`icon_mapping.md`](icon_mapping.md) 别名兜底，勿散落硬编码分支

## 首页 UI

- `FeatureCardWidget` 图标按卡片区域**自适应缩放**，可整块拉伸填充
- 单元素图标按控件尺寸计算，避免写死 `16×16`、`84×84` 等

## 弹窗（FramelessDialog）

- 新弹窗**必须**继承 `FramelessDialog`，禁止直接继承 `QDialog`
- 不重复实现无边框标题栏、拖拽、最小化/最大化/关闭
- 业务内容加到 `contentLayout()`，不替换根布局
- 标题栏图标：窗体 icon → `QApplication::windowIcon()` → `:/icons/icon.ico`

### 深色弹窗样式（新设计）

工具：[`phase2dialogstyle.h`](../src/publicctrls/phase2dialogstyle.h)

| API | 用途 |
|-----|------|
| `dialogShellStyleSheet()` | 顶层透明 + `#framelessDialogShell`；壳上 Label/CheckBox/Radio/GroupBox 白字 |
| `dialogPrimaryButtonStyleSheet()` | 主按钮 `#2188ff` 胶囊 |
| `dialogGhostButtonStyleSheet()` | 取消/关闭幽灵描边 |
| `dialogSecondaryButtonStyleSheet()` | 多操作次要白底按钮 |
| `dialogTabStyleSheet(objectName)` | 深色壳页签 |
| `dialogProgressBarStyleSheet()` | 加载进度条 |
| `dialogLightContentPanelStyleSheet(objectName)` | 白底内容板（表格/HTML/输出）深字 |

```cpp
setStyleSheet(Phase2Ui::dialogShellStyleSheet());
cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
okBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
```

- 壳上主文字 `#ffffff`、次要 `#cbd5e1`；白底输入/表格保持深字
- 圆角靠内层 `#framelessDialogShell`；基类 `WA_TranslucentBackground` 仅让圆角外透明
- 外壳圆角仅 **16px**；`contentLayout` 贴边容器（含 `QTabWidget::pane`）禁止再设 `border-radius` / 闭合描边，避免双层边角
- 白底内容板（`dialogLightContentPanelStyleSheet`）与弹窗内表格等数据区使用直角；圆角仅用于外壳与约定按钮/输入主题
- **禁止**给顶层 `FramelessDialog` 填不透明背景
- **禁止**再写 `#f2f7ff` / `#dbe8f9` / `#1f2430` 等浅色时代壳/标签色
- **禁止**平行发明第三套按钮圆角/颜色；同窗输入须同一 `InputFieldTheme`
- 例外（不自绘）：`QFileDialog`、`QColorDialog`、`QgsSymbolSelectorDialog`

## 提示 / 消息弹窗

一律用 [`TnUi`](../src/publicctrls/messagedialog.h)（`FramelessDialog` 深色封装）：

- `showInformation` / `showWarning` / `showCritical`
- `confirmYesNo`（是/否，返回 bool）

- **禁止** `QMessageBox::information` / `warning` / `critical` / `question`

## 输入控件（Phase2Ui）

工具：`src/publicctrls/phase2comboboxutil.h`

| 控件 | 用法 |
|------|------|
| **QComboBox** | `Phase2Ui::setupThemedCombo(combo, InputFieldTheme)` — 含闭合态 QSS + `setView` 弹层 |
| **QLineEdit / QSpinBox / QDoubleSpinBox / QDateTimeEdit** | `fieldStyleSheet(theme)` 或 `applyFieldStyle(widget, theme)` |

**禁止**：仅 `attachStyledComboPopup` 且闭合态靠父 QSS；手写 `setView(new QListView)` 并复制弹层 QSS。

### InputFieldTheme

| 枚举 | 用途 | 闭合高度 |
|------|------|----------|
| `LightDialogRounded` | 新建/重命名方案、新建项目 | 30px，8px 圆角 |
| `LightCompact` | 属性过滤、裁剪、算法参数 | 30px，4px 圆角 |
| `LightCompactSoft` | 选择集、符号化 | 30px，8px 圆角 |
| `DarkToolbar` | 地图工具栏双屏方案下拉 | **40px**（例外） |

同对话框内 LineEdit 与 ComboBox 须**同主题、同工厂**（`lightCompactFieldBase` / `lightCompactComboClosed`）。

未设样式的 QGIS 内嵌表单可不接入；一旦加闭合态样式须与 Combo 同一 `InputFieldTheme`。
