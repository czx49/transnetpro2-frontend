# TNMapWidget 地图控件

方案与基础数据共用的 QGIS 地图容器（`src/tnmapwidget/`）。

## 容器样式

- 继承 `QFrame`，**NoFrame**，避免与外层圆角卡片叠加双线描边
- 画布对齐 QGIS Desktop 主画布默认：`setCachingEnabled(true)`、`setParallelRenderingEnabled(true)`、`setPreviewJobsEnabled(true)`，减轻缩放/平移重绘闪白

## 底图

- `ensureBasemapLayersFromSession` — 从 `AppSession` 基础配置 `mapurl2` / `mapurl3` 加载
- `mBaseMapLayers` 顺序：`[底图标注, 底图]`
- 三亚范围只用于空白画布的初始视野；画布已有有效范围时不再改回三亚

## 图层叠置（updateCanvasLayers）

与 `QgsMapCanvas::setLayers` 一致：

- 列表**从前到后**绘制，**后绘制在上**；**最后一项在最底层**
- 底图 **append 在列表末尾**（最下）
- 业务层按 `LEVEL_PROPERTY`（`level`）**升序**（**level 越小越靠上**）
- 同 level 时，`mLayers` 中下标越小（较新 `insert(0)` 的层）越靠上

## API 要点

- `setCurrentLayer` — 更新当前层并 `updateCanvasLayers()`（勿单独拼装 `setLayers` 以免丢层）
- `MapStatus`、`currentMapStatus()`、`mapStatusChanged`、`currentLayerChanged`
- `setBottomLeftOverlayWidget` — 左下角图例等
- `removeLayersWithCustomProperty` — 按自定义属性批量移除

## 图层样式

WFS 业务层加载后延迟一帧调用 **`LayerStyleFactory::applyDefaultStyle(vl, layerVal, typeName)`**，避免同步栈崩溃。规则见 [`layer-style-factory.md`](layer-style-factory.md)。

图层 `addLayer` 末尾会调用 **`updateLayerSymbolColor`**：若 `setStyleContext` 已设置且本地库存在该图层的字段样式，则覆盖为分级渲染。详见 [`field-symbolization.md`](field-symbolization.md)。

- **`setStyleContext(projectId, caseId)`** — 基础数据 `caseId=0`，方案图为真实方案 id

## 图层过滤与移除

- **定义查询**：`TNLayerFilter::applyDefinitionQuery` — WFS 客户端 RuleBased 过滤，PostGIS `subsetString`。详见 [`layer-filter.md`](layer-filter.md)。
- **移除消毒**：`removeLayer` 前 `prepareVectorLayerForRemoval` — 停止渲染并 `restoreBaseRendererAfterClientFilter`，避免 RuleBased 析构崩溃。
- **交互回退**：`finalizeMapLayerRemoval` 前若添加/编辑/删除工具以该层为工作目标，先 `releaseMapToolsForRemovedLayer`（对齐 `deactivateEditTools`）并 `editInteractionCleared`，工具栏同步回「浏览」，避免悬空添加工具或静默改绑到其他层。
- **URI 同步**：WFS URI 去除服务端 filter 参数后 `syncLayerUriKey` 更新 `mUriLayers` 索引。

## 缩放至图层

编译宏 `TNP_ENABLE_ZOOM_TO_LAYER`（CMake `ENABLE_ZOOM_TO_LAYER=ON`）启用时：

- `TNMapWidget::zoomToLayer(QgsMapLayer*)`
- 方案/基础数据图层树右键菜单提供「缩放至图层」

## 相关代码

- `casewfsmapcontroller.cpp` — 业务 WFS 加载、路段 type 过滤调度
- `casewaylayerhelper.cpp` — 路段层默认定义查询
- `tnlayerfilterapply.cpp` / `tnlayerclientfilter.cpp` — 客户端过滤与 subset 分发
- `casepanel.cpp` — 基础数据 vs 方案详情地图路由、分析上图
