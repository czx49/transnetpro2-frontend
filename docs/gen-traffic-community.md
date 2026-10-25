# 生成交通小区（功能实现）

基础数据侧栏工具：从 `road_way` 与人口栅格本地生成交通小区（TAZ），经 `POST /api/project/gen_community` 整表写入 `road_community`。

## 入口与模块

| 组件 | 文件 |
|------|------|
| 侧栏入口 | [`CasePanel::runBaseDataCommunityGeneration`](../src/uis/casepanel.cpp) |
| 配置弹窗 | [`CaseCommunityGenerationDialog`](../src/casectrls/casecommunitygenerationdialog.cpp) |
| 本地算法 | [`CaseCommunityGenHelper`](../src/casectrls/casecommunitygenhelper.cpp) |
| HTTP | [`HttpApis::projectGenCommunity`](../src/httpapis/httpapis.cpp) |

工具节点来自项目 detail 的 `tools[]`（`buildBaseToolNodes` 遍历 `j_tools()`）；服务端配置 `type=openarea` 时点击走 `runBaseDataCommunityGeneration`。

## 图层加载策略

- **运行前**：仅勾选加载 `road_way`（WFS 至 `m_baseMapWidget`）
- **不加载** `road_community`；弹窗不再展示「路段图层」和「目标表」
- **成功后**：刷新 detail/WFS 缓存，勾选加载或 `reload()` `road_community`

## 处理流程

1. **clip_mask**：`road_way` 转米制 CRS → 线合并 → buffer(半宽, ROUND) → 去内环（填洞；受 CMake 选项 `ENABLE_COMMUNITY_GEN_FILL_HOLES` 控制，默认 `ON`）
2. **fishnet**：缓冲面 envelope 按格网边长生成矩形格网
3. **pop_grid**：对格网 band1 人口栅格 zonal sum → `pop_sum`
4. **grid_clip**：用 clip_mask 裁剪格网
5. **regionGrow**：等距水滴扩张——预计算邻接拓扑与格心，按相邻格到种子质心的平方距离加随机扰动入优先队列；人口超 `目标×系数` 时当前格回队列作新种子；小路网为后续小区预留网格
6. **merge**：每组 union → TAZ；`area_id` 从 1 编号；几何转 EPSG:4326 GeoJSON
7. **submit**：`gen_community`（不含 `traffic_generation_volume`）

## CRS

- 计算：按 `road_way` 范围中心经度选 CGCS2000 3° 带（`EPSG 4544 + zone - 25` 或 PROJ tmerc 回退）
- 提交：EPSG:4326，GeoJSON `[lon, lat]`

## 与「生成人口数据」对比

| | 生成人口数据 | 生成交通小区 |
|--|-------------|-------------|
| 输入图层 | 已有 `road_community` | `road_way` |
| 运行前加载 | `road_community` | 仅 `road_way` |
| 本地计算 | 分区统计 | 缓冲+格网+聚类 |
| API | `popdata` | `gen_community` |

## 调试 SHP

CMake 选项 `ENABLE_COMMUNITY_GEN_DEBUG_SHP=ON` 时，中间结果写入 `{appDir}/data/`：

| 文件 | 导出时机 | 属性字段 |
|------|----------|----------|
| `community_gen_clip_mask.shp` | 缓冲掩膜 | 仅几何 |
| `community_gen_fishnet.shp` | 人口分区统计后 | `grid_oid`、`pop_sum` |
| `community_gen_grid_clip.shp` | 格网裁剪后 | `grid_oid`、`pop_sum` |
| `community_gen_taz.shp` | 合并交通小区后 | `area_id`、`pop_sum`、`num_grids`、`seed_oid` |

详见 [`build-and-env.md`](build-and-env.md)。

## 相关文档

- 服务端对接：[`gen-traffic-community-api.md`](gen-traffic-community-api.md)
- 用户说明：[`gen-traffic-community-user-guide.md`](gen-traffic-community-user-guide.md)
