# 生成交通小区 — 服务端接口对接

## 接口

`POST /api/project/gen_community`（需 Bearer Token）

常量：[`API_PROJECT_GEN_COMMUNITY`](../src/httpapis/httpapidefines.h)  
客户端：[`HttpApis::projectGenCommunity`](../src/httpapis/httpapis.cpp)

## 请求体

```json
{
  "project_id": 29,
  "data": [
    {
      "area_id": 1,
      "pop_sum": 98500.2,
      "num_grids": 128,
      "seed_oid": 4521,
      "geometry": {
        "type": "Polygon",
        "coordinates": [[[110.4087, 29.6598], [110.4100, 29.6598], [110.4100, 29.6610], [110.4087, 29.6610], [110.4087, 29.6598]]]
      }
    }
  ]
}
```

### 顶层字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `project_id` | number | 是 | 当前项目 ID |
| `data` | array | 是 | 交通小区数组；**replace 语义**（服务端先清空项目 `road_community` 再插入） |

### `data[]` 每项字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `area_id` | number | 是 | 小区编号，从 1 连续递增 |
| `pop_sum` | number | 是 | 该 TAZ 内格网人口合计 |
| `num_grids` | number | 是 | 合并的格网数量 |
| `seed_oid` | number | 是 | 区域生长种子格 OID（本地格网自增 id） |
| `geometry` | object | 是 | **GeoJSON** 面几何（`Polygon` 或 `MultiPolygon`），坐标 `[lon, lat]`，EPSG:4326 |

**不包含** `traffic_generation_volume`（客户端组装时不得写入该键）。

## 响应示例

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "success": true,
    "inserted": 42,
    "message": "小区生成成功"
  }
}
```

解析类型：[`ProjectGenCommunityResultJson`](../src/jsonreader/httpapijson.h)（`success` / `inserted` / `message`）。

## 与 Swagger 差异

[`swagger-interfaces-current.json`](swagger-interfaces-current.json) 中该接口仍描述：

- `geometry` 为 WKT 字符串
- 必填 `traffic_generation_volume`

Phase2 客户端以本文为准：**GeoJSON 对象 + 扩展字段**，且**省略** `traffic_generation_volume`。联调时以服务端实际解析为准。

## 成功后客户端行为

1. 弹窗 `accept()`，提示成功信息  
2. `refreshBaseDataLayerTreeInPlace`  
3. `CaseWfsMap::refreshWfsCapabilitiesCache`  
4. 勾选加载或 `reload()` 菜单中 `road_community` WFS 图层  

**不调用** `update_centroid`（首版）。
