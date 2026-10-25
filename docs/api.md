# 城市综合交通网络预测与管理 API 文档

> Urban Integrated Transportation Network Prediction & Management System
>
> 文档同步来源：`http://gisapi.jiheshop.com/api/config/api`（2026-07-05）

---

## 统一响应格式

**接口说明：** 所有 API 响应均遵循以下统一格式

**URL:** `http://gisapi.jiheshop.com`

```json
{
  "code": 200,
  "data": { ... },
  "message": "success"
}
```

---

## 认证说明

### JWT Token 认证

除登录接口外，所有接口需要在请求头中携带 JWT Token：

```
Authorization: Bearer <token>
```

Token 通过 `POST /api/user/login` 接口获取。

---

## Apifox 导入与鉴权

OpenAPI/Swagger 文件：[`swagger-interfaces-current.json`](swagger-interfaces-current.json)（当前 version 1.0.7）。

**重要：** Apifox 将 OpenAPI 的 `security` 视为契约声明，**不会**在调用登录后自动把 `data.token` 写入后续请求。重新导入 swagger 后，项目级 Auth、环境变量与登录后置脚本可能被覆盖，需手动恢复。

**重新导入后必做 3 步：**

1. **环境变量**：当前环境中保留变量 `token`（初值可空）。
2. **登录后置操作**：`POST /api/user/login` → 后置操作 → 提取变量，JSONPath `$.data.token` → 环境变量 `token`。
3. **项目 Auth**：项目根目录（或父目录）绑定 `BearerAuth`；Bearer Token 类型填 `{{token}}`；若显示为 API Key（Header `Authorization`），值填 **`Bearer {{token}}`**。

**导入选项建议：** 开启「导入 Security Scheme」、勾选「将全局鉴权应用于项目根目录」；冲突处理选「智能合并」；资源清理选「不删除」。

**验证：** 先调登录（确认环境 `token` 有值），再调 `GET /api/project/page`，Request Headers 应含 `Authorization: Bearer ...`。

完整检查清单见 swagger 文件 `info.description` 中【Apifox 重新导入检查清单】段落。

---

## 基础信息

**接口说明：** 基础信息相关的所有请求，包括首页内容、联系我们等

### GET `/api/config/base` `公开`

获取基础信息（首页内容、联系我们、地图 URL 等）  url=空的 是显示文字 不可点击

**响应示例：**

```json
{
	"code": 200,
	"message": "操作成功",
	"data": {
		"site_name": "城市综合交通网络预测与管理",
		"mapurl": "http://t1.tianditu.gov.cn/img_w/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=img&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles&TILEMATRIX={z}&TILEROW={y}&TILECOL={x}&tk=27dcd4fd9571575b64f3fba58cbf96ef",
		"mapurl2": "https://t2.tianditu.gov.cn/vec_w/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=vec&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles&TILECOL={x}&TILEROW={y}&TILEMATRIX={z}&tk=27dcd4fd9571575b64f3fba58cbf96ef",
		"mapurl3": "https://t2.tianditu.gov.cn/cva_w/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=cva&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles&TILECOL={x}&TILEROW={y}&TILEMATRIX={z}&tk=27dcd4fd9571575b64f3fba58cbf96ef",
		"maxkb": "11",
		"mapselurl": "http://gisapi.jiheshop.com/pymap/map",
		"tit1": "城市综合交通系统集成及分析平台软件",
		"tit2": "提供面向不同业务功能的\"一体式\"城市综合交通集成分析平台",
		"titlist": [
			{
				"id": 1,
				"tit": "城市",
				"title": "城市开发"
			},
			{
				"id": 2,
				"tit": "交通",
				"title": "交通规划"
			},
			{
				"id": 3,
				"tit": "交通",
				"title": "交通管控"
			},
			{
				"id": 4,
				"tit": "交通",
				"title": "交通政策"
			},
			{
				"id": 5,
				"tit": "公共",
				"title": "公共交通"
			}
		],
		"contact": "张明、李华、王强",
		"email1": "contact@gis-traffic.com",
		"email2": "contact@gis-traffic.com",
		"phone": "400-800-800",
		"address": "中国·成都",
		"study_url": "http://gisapi.jiheshop.com/web/news?bid=3",
		"manual_url": "http://gisapi.jiheshop.com/web/news?bid=1",
		"help_list": [
			{
				"id": 1,
				"title": "方案配置",
				"img": "http://gisapi.jiheshop.com/uploads/icon1.png",
				"url": "http://gisapi.jiheshop.com/web/news/1"
			},
			{
				"id": 2,
				"title": "数据库建立",
				"img": "http://gisapi.jiheshop.com/uploads/icon2.png",
				"url": "http://gisapi.jiheshop.com/web/news/2"
			},
			{
				"id": 3,
				"title": "基本原理",
				"img": "http://gisapi.jiheshop.com/uploads/icon3.png",
				"url": "http://gisapi.jiheshop.com/web/news/3"
			},
			{
				"id": 4,
				"title": "图形编辑",
				"img": "http://gisapi.jiheshop.com/uploads/icon4.png",
				"url": "http://gisapi.jiheshop.com/web/news/4"
			}
		],
		"ai_url": "http://8.137.55.118:8790"
	}
}
```

### GET `/api/config/getversion` `公开`

最新版本

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "id": 1,
    "version": "1.0.0",
    "url": "http://gisapi.jiheshop.com/uploads/",
    "md5": "1",
    "size": "1",
    "desc": "V1.0.0",
    "created_at": "2026-04-24T08:40:59.313Z"
  }
}
```

### GET `/api/config/aiconfig` `公开`

AI 多级选词（树形 `data[]`，节点含 `id`、`pid`、`level`、`title`、`children`）。

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": [
    {
      "id": 1,
      "title": "机动车分析",
      "pid": 0,
      "level": 1,
      "children": [
        {
          "id": 4,
          "title": "机动车二级",
          "pid": 1,
          "level": 2,
          "children": [
            {
              "id": 5,
              "title": "机动车02-01",
              "pid": 4,
              "level": 3,
              "children": []
            }
          ]
        }
      ]
    },
    {
      "id": 2,
      "title": "慢行交通分析",
      "pid": 0,
      "level": 1,
      "children": []
    },
    {
      "id": 3,
      "title": "公交运行分析",
      "pid": 0,
      "level": 1,
      "children": []
    }
  ]
}
```

**Phase2 客户端：** `HttpApis::getAiConfig` → `AiConfigModel`（解析 `data[]` 树形 `children`，按分析模块 id 过滤 L1/L2）；`AIChatWidget` 内 `AiWordSelectBar` 提供 **L1/L2** 两行 Chip 选词（L2 随 L1 级联），会话级保持直至用户改选。「数据选择」为 **checkable** 按钮：checked 时激活地图框选工具，框选完成或再次点击后 unchecked；若 L1 含「框选」则仅 L1 为「框选」时可用且离开「框选」清空 `ids`，不含「框选」时默认可用；手动取消无「未选中路段」提示。

### GET `/api/config/question` `公开`

AI 多级选词下的常见问题。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `isbase` | number | 是 | `1` 基础方案，`0` 普通方案 |
| `type` | string | 是 | 机动车 `motor`，慢行 `slow`，公交 `bus` |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": [
    { "name": "1111" },
    { "name": "2222" }
  ]
}
```

**Phase2 客户端：** `HttpApis::getCommonQuestions`；`CaseWorkPanel` 按当前方案类型 `isbase` 与当前分析模块 `type` 查询并缓存，`AIChatWidget` 在 L1/L2 选词条与用户输入框之间显示「常见问题」下拉。选中后不填充输入框，发送时作为 `/api/grpc/aiapi` 的 `business_goal` 参数。

### GET `/api/config/regions` `公开`

省市列表。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `province` | number | 是 | `0` 返回全部省份；传省 `id` 时返回该省下城市 |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": [
    {
      "id": 510000,
      "name": "四川"
    }
  ]
}
```

**Phase2 客户端：** `HttpApis::getRegions`；`NewProjectDialog` 打开时以 `province=0` 加载省份下拉，选中省后以省 `id` 加载市下拉；创建项目时将选中省/市的 `id`/`name` 传给 `POST /api/project/add`。

### GET `/api/config/field_chinese` `公开`

图层字段中文翻译。返回英文字段名到中文展示名的映射，供属性表、要素信息、图例等 UI 展示列头/字段名时使用；**不影响**字段实际值、WFS 提交或 SQL 表达式中的原始字段名。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `province` | number | 否 | 服务端文档标注可选筛选用：`0` 全部，`id` 为各省。Phase2 客户端**不传**此参数，直接拉取全量映射 |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "lonlat": "经纬度",
    "node_id": "节点ID",
    "node_name": "节点名称",
    "node_type": "节点类型",
    "incoming_link_count": "进入节点路段数量",
    "outgoing_link_count": "离开节点路段数量",
    "node_capacity": "节点通行能力",
    "is_centroid": "是否小区质心",
    "geometry": "地理信息",
    "link_id": "路段ID",
    "name": "路名",
    "init_node": "起点节点ID",
    "term_node": "终点节点ID",
    "route_ref": "路线编号",
    "direction": "方向",
    "capacity": "通行能力",
    "b": "BPR离散度参数",
    "power": "BPR幂次参数",
    "toll": "道路收费",
    "type": "路段类型",
    "fft": "自由流时间",
    "speedlimit": "限速",
    "length": "长度",
    "centroid_matched_node": "质心匹配节点",
    "adjust_num": "调整系数",
    "link_osmid": "路段对应osmid",
    "volume_edit": "预设流量",
    "saturation_edit": "预设饱和度",
    "link_flow_diff": "流量差值",
    "saturation_diff": "饱和度差值",
    "lane_num": "车道数量",
    "volume": "分配后路段流量",
    "v_c": "饱和度",
    "bottleneck": "是否瓶颈路段",
    "area_id": "小区ID",
    "cars": "车辆",
    "eind": "产业就业",
    "eret": "零售就业",
    "eoth": "其他就业",
    "etot": "总就业",
    "du": "住户",
    "hinc": "收入",
    "area": "面积",
    "longitude": "经度",
    "latitude": "纬度",
    "connection_to_road_network": "与道路网络连接信息",
    "traffic_generation_volume": "交通生成量",
    "traffic_attraction_volume": "交通吸引量",
    "id": "编号",
    "f_id": "起点ID",
    "t_id": "终点ID",
    "demand": "交通需求",
    "m_special": "是否为特殊od",
    "demand_diff": "需求差值",
    "channel_id": "卡口唯一编号",
    "gps_x": "经度",
    "gps_y": "纬度",
    "plate_num": "车辆唯一编号牌",
    "cap_time": "数据记录时刻",
    "vehicle_speed": "速度",
    "city": "城市",
    "popu": "数量",
    "line_id": "线路编号",
    "line_name": "线路名称",
    "od_station": "起点与终点",
    "route": "线路途径道路",
    "stations": "线路途径站点",
    "departure_time": "发车时间",
    "departure_station": "首班发车站点",
    "departure_dir": "发车时单向或双向",
    "departure_plan": "排班计划",
    "price": "价格",
    "trip_mile": "单趟里程",
    "line_type": "线路类型",
    "remarks": "补充内容",
    "cust_line_id": "线路编号",
    "cust_line_name": "线路名称",
    "cust_od_station": "起点与终点",
    "cust_o_station": "起点",
    "cust_o_departure_time": "起点发车时间",
    "cust_d_station": "终点站",
    "cust_d_departure_time": "终点发车时间",
    "cust_stations": "途经的站点",
    "cust_price": "价格",
    "cust_workdays": "服务的时间范围",
    "cust_bus_shuttle_route": "接送线路",
    "cust_service": "服务",
    "sch_line_id": "线路编号",
    "sch_od_station": "起止点",
    "sch_departure_time": "发车时间",
    "sch_plan": "运力配置",
    "sch_route": "线路走向",
    "sch_stations": "停靠站点",
    "sch_price": "价格",
    "sch_service": "服务",
    "sch_trip_mile": "单趟里程",
    "sch_remarks": "备注",
    "vehicle_id": "车辆ID",
    "vehicle_lng": "经度",
    "vehicle_lat": "纬度",
    "time": "时间",
    "pop": "人口数量",
    "flow": "观测流量值",
    "zone_id": "分区ID",
    "production": "生成量",
    "attraction": "吸引量",
    "created_at": "创建时间",
    "stop_id": "站点唯一编号",
    "stop_name": "站点名称",
    "x": "经度",
    "y": "纬度",
    "route_id": "线路唯一编号",
    "route_short_name": "线路简称",
    "route_long_name": "线路全称",
    "route_type": "线路类型",
    "shape_id": "线路形状ID",
    "frequency": "发车频率",
    "stop_sequence": "停站序号",
    "from_stop": "起点站",
    "to_stop": "终点站",
    "origin_stop": "起点站",
    "destination_stop": "终点站",
    "lane_width": "机动车道宽度",
    "separation_type": "分离类型",
    "has_parking": "是否存在路侧停车",
    "bike_lane_width": "非机动车道宽度",
    "buffer_width": "缓冲区宽度",
    "req_bike_width": "慢行交通需求宽度",
    "lat_safety_clear": "侧向安全净距",
    "enc_ratio": "慢行交通侵占比例",
    "aff_lane_num": "受到影响的机动车道数量"
  }
}
```

**Phase2 客户端：** `HttpApis::getFieldChinese`（`PropertyJson`）；`AppSession::fetchFieldChinese` 在 `AppWindow` 启动时拉取并缓存为 `mFieldChineseMap`；`AppSession::fieldChineseDisplayName` 供各 UI 查表。图层加载时经 `TNMapWidget::addLayer` 批量 `setFieldAlias`；PostGIS `pg_description` 已有别名优先于 API 映射。登出不清空该缓存。

---

## 表数据查询

**接口说明：** 表数据

### GET `/api/project/select_data_all` `公开`

获取所有数据

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `tablefullname` | string | 是 | 表名 |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {}
}
```

### GET `/api/project/select_data_page` `公开`

获取所有数据分页

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `tablefullname` | string | 是 | 表名 |
| `page` | number | 是 | 页码 |
| `limit` | number | 是 | 每页数量 |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {}
}
```

---

## 用户管理

### POST `/api/user/login` `公开`

用户登录（测试账号：a123，密码：123456）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `username` | string | 是 | 用户名 |
| `password` | string | 是 | 密码 |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "token": "eyJhbGciOiJi5jbi9jdmFfdy93bXRzI3GYWHl1ohKTucBjQIVa84Y_So-YuL8W4mek19k",
    "user": {
      "user_id": 1,
      "username": "a123",
      "real_name": "测试用户1",
      "mobile": "15800000001",
      "status": 1,
      "project_num": 0,
      "url": "11",
      "mapurl": "http://t1.tianditu.gov.cn/img_",
      "mapurl_vector": "https://t2.tianditu.gov.cn/4fd9571575b64f3fba58cbf96ef",
      "mapurl_note": "https://t2.tianditu.gov.cn/c=w&F"
    }
  }
}
```

### POST `/api/user/change-password` `需认证`

修改用户密码

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `oldPassword` | string | 是 | 旧密码 |
| `newPassword` | string | 是 | 新密码 |
| `confirmNewPassword` | string | 是 | 确认新密码 |

---

## 项目管理

### GET `/api/project/page` `需认证`

获取项目分页列表（支持分页）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `page` | number | 否 | 页码，默认 1 |
| `limit` | number | 否 | 每页数量，默认 10 |
| `name` | string | 否 | 项目名称筛选 |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "data": [
      {
        "id": 12,
        "name": "1212",
        "desc": "简介10151",
        "users": [1],
        "num": 0,
        "status": 1,
        "created_at": "2026-04-15T12:04:39.000Z",
        "updated_at": "2026-04-15 20:04:39"
      }
    ],
    "total": 2,
    "page": 1,
    "limit": 20,
    "totalPages": 1
  }
}
```

### POST `/api/project/add` `需认证`

创建项目

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `name` | string | 是 | 项目名称 |
| `desc` | string | 否 | 项目描述 |
| `province_id` | number | 是 | 省 ID |
| `province` | string | 是 | 省名称 |
| `city_id` | number | 是 | 市 ID |
| `city` | string | 是 | 市名称 |

**Phase2 客户端：** `HttpApis::createProject`；`NewProjectDialog` 分别加载省（`province=0`）与市（传省 `id`），创建时传 `province_id`/`province`/`city_id`/`city`。

### GET `/api/project/detail` `需认证`

获取项目详情

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `id` | number | 是 | 项目 ID |

**说明：** `menu[]` 为图层分组结构。每项为图层组（`name`/`value` 为组名与组标识，如 `road`、`bus`、`slow`、`other`），具体可勾选图层在 `children[]` 中（`value` 为 WFS 表全名，`val` 为表类型键，`level` 为渲染层级）。Phase2 客户端基础数据树展示为：**所有图层 → 图层组 → 图层**。

**响应示例：**

```json
{
	"code": 200,
	"message": "操作成功",
	"data": {
		"info": {
			"id": 22,
			"name": "5-9加了慢行",
			"desc": "简介全部表",
			"users": [1],
			"num": 2,
			"status": 1,
			"sort": 100,
			"created_at": "2026-05-09T03:20:36.000Z",
			"updated_at": "2026-05-12T07:28:03.470Z",
			"is_delete": 0
		},
		"dbfix": "user_project",
		"dbuserfix": "project22_user1",
		"algorithm_params": {
			"conv": "1e-6",
			"max_iter": 100,
			"cost_scalar": 60
		},
		"menu": [
			{
				"name": "道路基础数据",
				"value": "road",
				"children": [
					{
						"name": "点图层",
						"value": "project22_user1_road_point",
						"val": "road_point",
						"level": 1
					},
					{
						"name": "路段图层",
						"value": "project22_user1_road_way",
						"val": "road_way",
						"level": 2
					},
					{
						"name": "小区图层",
						"value": "project22_user1_road_community",
						"val": "road_community",
						"level": 3
					}
				]
			},
			{
				"name": "公交基础数据",
				"value": "bus",
				"children": [
					{
						"name": "点图层",
						"value": "project22_user1_bus_point",
						"val": "bus_point",
						"level": 1
					},
					{
						"name": "路段图层",
						"value": "project22_user1_bus_way",
						"val": "bus_way",
						"level": 2
					},
					{
						"name": "小区图层",
						"value": "project22_user1_bus_community",
						"val": "bus_community",
						"level": 3
					},
					{
						"name": "公交站点",
						"value": "project22_user1_pt_stop",
						"val": "pt_stop",
						"level": 4
					},
					{
						"name": "公交线路",
						"value": "project22_user1_pt_route",
						"val": "pt_route",
						"level": 4
					},
					{
						"name": "公交线路停站序列",
						"value": "project22_user1_pt_shape",
						"val": "pt_shape",
						"level": 4
					},
					{
						"name": "公交站间运行弧",
						"value": "project22_user1_pt_transit",
						"val": "pt_transit",
						"level": 4
					},
					{
						"name": "步行换乘弧",
						"value": "project22_user1_pt_walk",
						"val": "pt_walk",
						"level": 4
					},
					{
						"name": "公交出行需求",
						"value": "project22_user1_pt_trip",
						"val": "pt_trip",
						"level": 4
					}
				]
			},
			{
				"name": "慢行数据",
				"value": "slow",
				"children": [
					{
						"name": "点图层(慢行)",
						"value": "project22_user1_slow_road_point",
						"val": "slow_road_point",
						"level": 1
					},
					{
						"name": "路段图层(慢行)",
						"value": "project22_user1_slow_road_way",
						"val": "slow_road_way",
						"level": 2
					},
					{
						"name": "小区图层(慢行)",
						"value": "project22_user1_slow_road_community",
						"val": "slow_road_community",
						"level": 3
					},
					{
						"name": "慢行OD数据",
						"value": "project22_user1_slow_other_od",
						"val": "slow_other_od",
						"level": 4
					}
				]
			},
			{
				"name": "其他数据",
				"value": "other",
				"children": [
					{
						"name": "OD数据",
						"value": "project22_user1_other_od",
						"val": "other_od"
					},
					{
						"name": "卡口数据",
						"value": "project22_user1_other_toll_gate",
						"val": "other_toll_gate"
					},
					{
						"name": "手机信令数据",
						"value": "project22_user1_other_mobile_cdr",
						"val": "other_mobile_cdr"
					},
					{
						"name": "公交常规线路",
						"value": "project22_user1_other_bus_route",
						"val": "other_bus_route"
					},
					{
						"name": "公交定制线路",
						"value": "project22_user1_other_bus_customized",
						"val": "other_bus_customized"
					},
					{
						"name": "公交校车线路",
						"value": "project22_user1_other_bus_school",
						"val": "other_bus_school"
					},
					{
						"name": "出租车数据",
						"value": "project22_user1_other_taxi_trip",
						"val": "other_taxi_trip"
					},
					{
						"name": "人口数据",
						"value": "project22_user1_other_population",
						"val": "other_population"
					},
					{
						"name": "路段观测流量数据",
						"value": "project22_user1_other_observation",
						"val": "other_observation"
					},
					{
						"name": "TAZ基础属性",
						"value": "project22_user1_other_taz_socioeconomic",
						"val": "other_taz_socioeconomic"
					},
					{
						"name": "出行生成吸引表",
						"value": "project22_user1_other_trip_production_attraction",
						"val": "other_trip_production_attraction"
					},
					{
						"name": "历史OD表",
						"value": "project22_user1_other_trip_distribution_hist",
						"val": "other_trip_distribution_hist"
					}
				]
			}
		],
		"tools": [
			{
				"name": "基础/现状数据分析",
				"type": "open",
				"url": "analysis"
			},
			{
				"name": "获取OD数据/四阶段法",
				"type": "url",
				"url": "http://www.baidu.com"
			},
			{
				"name": "OSM数据下载",
				"type": "url",
				"url": "http://gismap3.jiheshop.com/urban-traffic/map?project_id=22&user_id=1"
			},
			{
				"name": "公交可达性",
				"type": "url",
				"url": "http://www.baidu.com"
			},
			{
				"name": "公交站点覆盖率",
				"type": "url",
				"url": "http://www.baidu.com"
			},
			{
				"name": "轨道站点覆盖率",
				"type": "url",
				"url": "http://www.baidu.com"
			},
			{
				"name": "轨道运营信息",
				"type": "url",
				"url": "http://www.baidu.com"
			}
		]
	}
}
```

### POST `/api/project/edit` `需认证`

编辑项目

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `id` | number | 是 | 项目 ID |
| `name` | string | 是 | 项目名称 |
| `desc` | string | 否 | 项目描述 |
| `province_id` | number | 是 | 省 ID |
| `province` | string | 是 | 省名称 |
| `city_id` | number | 是 | 市 ID |
| `city` | string | 是 | 市名称 |

**Phase2 客户端：** `HttpApis::editProject` 当前仅传 `id`/`name`/`desc`，尚未传省市区；全仓库暂无调用方。

### POST `/api/project/del` `需认证`

删除项目（移到回收站）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `id` | number | 是 | 项目 ID |

### GET `/api/project/export` `需认证`

导出项目表数据（TXT 或 XLSX 格式，保存在 MinIO）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `table` | string | 是 | 表名 |
| `type` | string | 是 | 导出类型（txt/xlsx） |
| `case_id` | number | 是 | 方案 ID；处于某方案时填对应方案 id，处于项目基础数据时填 `0`（线上文档未列此项，客户端按与 `/api/files/drcsv` 相同规则传参） |

**Phase2 客户端（方案树 / 基础数据图层右键「导出」）**

- 调用：`HttpApis::exportProjectTable` → 本接口
- `table`：由图层叶子节点 `val` 直接作为表名（见 `CaseDrcsvHelper::tableNameFromLayerVal`，须在已知表名白名单内）
- `case_id`：方案侧栏图层导出传所属方案 id；基础数据侧栏导出传 `0`
- 导出成功后通过 `FileDownloader` 下载 `data.downloadUrl` 指向的文件

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "fileName": "project12_user1_road_way_export_2026-04-16T08-01-16-336Z_67j9.xlsx",
    "downloadUrl": "http://192.168.3.208:9000/exports/project12_user1_road_way_export_2026-04-16T08-01-16-336Z_67j9.xlsx",
    "bucket": "exports",
    "fileSize": 132765,
    "recordCount": 978,
    "exportType": "XLSX",
    "tableName": "project12_user1_road_way"
  }
}
```

### POST `/api/project/clrtable` `需认证`

清空项目表数据

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `table` | string | 是 | 表名 |

### GET `/api/project/exportcase` `需认证`

导出方案

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |

### GET `/api/project/update_centroid` `需认证`

生成质心

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `tablefullname` | string | 是 | 表全名（如：project64_user1_case64_road_community） |

### POST `/api/project/clip` `需认证`

路段裁剪 by 小区

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `way_table` | string | 是 | 表 |
| `community_table` | string | 是 | 表 |
| `clip_name` | string | 是 | 裁剪图层 |

### POST `/api/project/clippart` `需认证`

自定义裁剪路段

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `way_table` | string | 是 | 表 |
| `clip_name` | string | 是 | 裁剪图层 |
| `wkt` | string | 是 | wkt |

### GET `/api/project/clipdel` `需认证`

删除裁剪图层

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `tablefullname` | string | 是 | 删除的表 |

### POST `/api/project/popdata` `需认证`

生成人口数据（客户端本地分区统计后提交）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `data` | array | 是 | 人口统计数组，每项含 `area_id`、`pop_sum` |

**请求示例：**

```json
{
  "project_id": 29,
  "data": [
    { "area_id": 1, "pop_sum": 12492.1104 },
    { "area_id": 2, "pop_sum": 5154.9682 }
  ]
}
```

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {}
}
```

**Phase2 客户端：** `HttpApis::projectPopData`；基础数据工具「生成人口数据」在本地按 `area_id` 汇总后调用本接口，成功后刷新项目 detail 缓存并重载 WFS 小区图层。

### POST `/api/project/gen_community` `需认证`

批量生成小区（replace 整表 `road_community`）。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `data` | array | 是 | 小区数组，每项见下表 |

**`data[]` 每项（Phase2 客户端）：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `area_id` | number | 小区编号（从 1 递增） |
| `pop_sum` | number | 人口合计 |
| `num_grids` | number | 合并格网数 |
| `seed_oid` | number | 区域生长种子格 OID |
| `geometry` | object | GeoJSON Polygon/MultiPolygon，`[lon,lat]` |

**请求示例（Phase2）：**

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

**响应示例：**

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

**Phase2 客户端：** `HttpApis::projectGenCommunity`；基础数据工具「生成交通小区」本地格网聚类后调用本接口；不含 `traffic_generation_volume`。对接细节见 [`gen-traffic-community-api.md`](gen-traffic-community-api.md)。

> Swagger 仍可能描述 WKT + `traffic_generation_volume`；以 Phase2 客户端与 [`gen-traffic-community-api.md`](gen-traffic-community-api.md) 为准。

### GET `/api/project/check_obs` `需认证`

校验当前项目是否已有路段观测流量数据。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "obs": 1
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `obs` | number | `1` 有路段观测流量数据，`0` 无数据 |

**Phase2 客户端：** `HttpApis::checkProjectObs`；基础数据工具「OD反推」弹窗打开时自动调用；`obs!=1` 时禁用「运行」并提示缺少观测流量数据。

### GET `/api/project/base_analysis` `需认证` `已废弃`

基础/现状数据分析，返回 HTML 代码。**线上已废弃**，请改用 `GET /api/case/basedata_view`。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |

**Phase2 客户端：** 常量 `API_PROJECT_BASE_ANALYSIS` 已定义但 UI 未调用；侧栏工具「基础数据分析」使用 `/api/case/basedata_view`。

---

## 方案管理

- **方案类型：** 基础方案 / 普通方案
- **业务类型：** 机动车分析 / 慢行交通分析 / 公交运行分析
- **状态：** 进行中 / 已完成

### GET `/api/case/listAll` `需认证`

获取项目方案列表（当前登录用户，查询所有数据） 可不用

### GET `/api/case/list` `需认证`

获取项目方案列表（支持筛选和分页）  isbase = 1 就是 基础方案一个一个项目只能有一个

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 |
| `page` | number | 否 | 页码，默认 1 |
| `limit` | number | 否 | 每页数量，默认 10 |

**说明：** 每项方案的 `list[]` 与 `project/detail` 的 `menu[]` 结构一致（图层组 + `children[]` 图层）。`case_count` 除道路/公交/其他统计外，还包括 `pt_*`（公交站点/线路等）、`slow_*`（慢行路网与 OD）等字段。Phase2 方案树展示为：**方案 → 图层组 → 图层**；图层组支持三态勾选并联动子图层 WFS 加载。

**响应示例：**

```json
{
	"code": 200,
	"message": "操作成功",
	"data": {
		"data": [
			{
				"id": 9,
				"project_id": 22,
				"user_id": 1,
				"case_type_id": 1,
				"name": "基础方案",
				"road_types": ["point", "way", "community"],
				"bus_types": ["point", "way"],
				"other_types": ["od", "toll_gate"],
				"status": 1,
				"created_at": "2026-05-09 13:49:06",
				"isbase": 1,
				"case_count": {
					"other_od": 256,
					"other_toll_gate": 0,
					"other_mobile_cdr": 0,
					"other_bus_route": 4,
					"other_bus_customized": 0,
					"other_bus_school": 0,
					"other_taxi_trip": 0,
					"other_population": 0,
					"other_observation": 9,
					"other_taz_socioeconomic": 16,
					"other_trip_production_attraction": 16,
					"other_trip_distribution_hist": 0,
					"road_point": 572,
					"road_way": 1002,
					"road_community": 48,
					"bus_point": 0,
					"bus_way": 0,
					"bus_community": 48,
					"pt_stop": 858,
					"pt_route": 132,
					"pt_shape": 4356,
					"pt_transit": 4224,
					"pt_walk": 631,
					"pt_trip": 5332,
					"slow_road_point": 572,
					"slow_road_way": 1002,
					"slow_road_community": 48,
					"slow_other_od": 100
				},
				"list": [
					{
						"name": "道路基础数据",
						"value": "road",
						"children": [
							{
								"name": "点图层(道路)",
								"value": "project22_user1_case9_road_point",
								"val": "road_point",
								"level": 1
							},
							{
								"name": "路段图层(道路)",
								"value": "project22_user1_case9_road_way",
								"val": "road_way",
								"level": 2
							},
							{
								"name": "小区图层(道路)",
								"value": "project22_user1_case9_road_community",
								"val": "road_community",
								"level": 3
							}
						]
					},
					{
						"name": "公交基础数据",
						"value": "bus",
						"children": [
							{
								"name": "公交站点",
								"value": "project22_user1_case9_pt_stop",
								"val": "pt_stop",
								"level": 4
							}
						]
					},
					{
						"name": "慢行基础数据",
						"value": "slow",
						"children": [
							{
								"name": "慢行OD数据",
								"value": "project22_user1_case9_slow_other_od",
								"val": "slow_other_od",
								"level": 2
							}
						]
					},
					{
						"name": "其他数据",
						"value": "other",
						"children": [
							{
								"name": "OD数据",
								"value": "project22_user1_case9_other_od",
								"val": "other_od",
								"level": 2
							}
						]
					},
					{
						"name": "辅助图层",
						"value": "clip",
						"children": []
					}
				]
			}
		],
		"total": 1,
		"page": 1,
		"limit": 21,
		"totalPages": 1
	}
}
```

### GET `/api/case/detail/:id` `需认证`

获取项目方案详情（包含路段总数、小区总数、节点总数等）

**响应 `data` 算法运行状态字段**

| 字段 | 类型 | 说明 |
|--------|------|------|
| `motor_alg1` / `motor_alg2` | number | 机动车模式第 1/2 个算法：`1` 运行成功，`0` 运行失败 |
| `slow_alg1` / `slow_alg2` | number | 慢行模式第 1/2 个算法：`1` 运行成功，`0` 运行失败 |
| `bus_alg1` / `bus_alg2` | number | 公交模式第 1/2 个算法：`1` 运行成功，`0` 运行失败 |

**Phase2 客户端：** 工作台会话初始化时读取当前模式对应的两个状态。两者均为 `1` 时，将对应算法步骤恢复为已完成，并显示与手动连续运行成功后一致的报告入口；已完成算法步骤仍可再次手动运行。

### POST `/api/case/add` `需认证`

创建项目方案

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `name` | string | 是 | 方案名称 |
| `project_id` | number | 是 | 项目 |
| `case_type_id` | number | 是 | 方案类型 ID |
| `road_types` | string[] | 是 | 公交基础数据 |
| `bus_types` | string[] | 是 | 道路基础数据 |
| `other_types` | string[] | 是 | 其它基础数据 |

### POST `/api/case/edit` `需认证`

编辑项目方案

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `name` | string | 是 | 方案名称 |
| `id` | number | 是 | 方案 ID |
| `case_type_id` | number | 是 | 方案类型 ID |
| `road_types` | string[] | 是 | 公交基础数据 |
| `bus_types` | string[] | 是 | 道路基础数据 |
| `other_types` | string[] | 是 | 其它基础数据 |

### POST `/api/case/edit-name` `需认证`

修改项目方案名称

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `id` | number | 是 | 方案 ID |
| `name` | string | 是 | 名称 |

### POST `/api/case/edit-status` `需认证`

修改项目方案状态

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `id` | number | 是 | 方案 ID |
| `status` | number | 是 | 状态 |

### DELETE `/api/case/del/:id` `需认证`

删除项目方案

### POST `/api/case/copy` `需认证`

复制项目方案

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `case_id` | number | 是 | 方案 ID |
| `name` | string | 是 | 新方案名称 |

### POST `/api/case/copyproject` `需认证`

更新项目基础方案

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `case_id` | number | 是 | 方案 ID（基础方案 ID） |

**Phase2 客户端（基础方案右键「覆盖」）**

- 调用：`HttpApis::copyProjectCase` → 本接口
- 参数：`case_id` 为当前项目基础方案（`isbase = 1`）的 ID
- 成功后刷新当前工程方案列表（`UIGlobalState::reloadCurrentProjectCases`）

### POST `/api/case/edit-result` `需认证`

修改项目方案结果

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `id` | number | 是 | 方案 ID |
| `result` | string | 是 | 结果 |
| `url` | string | 是 | 地址 |

### POST `/api/case/fieldbatch` `需认证`

方案道路图层字段数据批量处理

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 |
| `case_id` | number | 是 | 方案 ID |
| `tablefullname` | string | 是 | 表 |
| `field` | string | 是 | 字段 |
| `type` | number | 是 | 类型 1(加)、2(减)、3(乘) |
| `num` | number | 是 | 数值 |

### GET `/api/case/create_expect_line` `需认证`

生成期望线

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 |
| `case_id` | number | 是 | 方案 ID |
| `tablefullname` | string | 是 | 表 |

### GET `/api/case/get_trace` `需认证`

道路溯源

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 |
| `case_id` | number | 是 | 方案 ID |
| `tablefullname` | string | 是 | 表 |
| `link_id` | number | 是 | 路段 ID |

### GET `/api/case/export_simulate` `需认证`

仿真流量数据 下载

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**Phase2 客户端：** 工作台仿真流量下载 → `HttpApis::exportCaseSimulate`；`type` 取自 `CaseWorkPanel::grpcTransportType()`。

### GET `/api/case/basedata_view` `需认证`

基础现状数据分析（HTML 报告）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |

**响应：** `data.content` 为 HTML 文件 URL。

**Phase2 客户端：** 基础数据侧栏工具「基础数据分析」→ `HttpApis::caseBaseDataView`；客户端将 `data.content` 按 URL 处理，优先用 WebView2 浏览器弹窗打开，若当前构建或环境无法打开，则下载 HTML 到本地后调用系统默认浏览器打开。

### GET `/api/case/cost_view` `需认证`

成本效益分析（HTML 报告）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**响应：** `data.content` 为 HTML 文件 URL。

**Phase2 客户端：** 工作台「方案评审」完成后「成本效益分析」→ `HttpApis::caseCostView`；`type` 取自 `CaseWorkPanel::grpcTransportType()`；客户端将 `data.content` 按 URL 处理，优先用 WebView2 浏览器弹窗打开，若当前构建或环境无法打开，则下载 HTML 到本地后调用系统默认浏览器打开。

### GET `/api/case/casecomparison_view` `需认证`

方案对比（HTML 报告）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_ids` | string | 是 | 方案 ID 列表，逗号分隔，如 `1,2,3` |
| `check_extent` | string | 是 | 对比地理范围，GeoJSON **Geometry** 字符串。客户端传 **Polygon**（矩形闭合环），坐标系与方案地图 canvas CRS 一致（通常 EPSG:4326）。示例：`{"type":"Polygon","coordinates":[[[lon1,lat1],[lon2,lat1],[lon2,lat2],[lon1,lat2],[lon1,lat1]]]}` |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**Phase2 客户端：** 方案侧栏「方案对比」→ 先在基础方案标准视图地图框选四至 → `CaseComparisonDialog` 勾选方案 → `HttpApis::caseComparisonView`（携带 `check_extent`、`type`）；`type` 取自 `CaseWorkPanel::grpcTransportType()`，未选模块默认 `motor`；`data.content` 按 URL 处理，优先 WebView2，否则下载后用系统浏览器打开。

### GET `/api/case/export_osm` `需认证`

OSM地图数据 下载

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**Phase2 客户端：** 工作台 OSM 地图数据下载 → `HttpApis::exportCaseOsm`；`type` 取自 `CaseWorkPanel::grpcTransportType()`。

### GET `/api/grpc/case_export` `需认证`

生成分析报告（流程第一步）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

### GET `/api/case/ptcase_export` `需认证`

普通方案 生成改造方案报告

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**响应：** `data.content` 为 HTML 文件 URL。

也可能返回下载字段（与 `report_download` 类似）：`download_url` / `file_name`。客户端优先按 URL 打开 `content`，否则走下载。

**Phase2 客户端：** 工作台「生成改造方案报告」→ `HttpApis::exportPtCaseReport` → `openHtmlReportWithFallback`（WebView2 或下载后系统浏览器），无 `content` 时走文件下载。

### GET `/api/case/editlog` `需认证`

修改方案 log 分页显示。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `page` | number | 是 | 页码 |
| `limit` | number | 是 | 每页数量 |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**Phase2 客户端：** 普通方案工作台独立步骤「修改方案记录」→ `HttpApis::caseEditLog`，默认 `page=1`、`limit=100`；`type` 取自 `CaseWorkPanel::grpcTransportType()`；结果以 `created_at` + `demo` 摘要列表（每行一条）显示在该步骤下方 TextEdit 中（可多次查询刷新）。

### GET `/api/case/report_view` `需认证`

分析报告 查看

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**响应：** `data.content` 为 HTML 文件 URL。

**Phase2 客户端：** 基础方案工作台分析报告「查看」→ `HttpApis::caseReportView`；`type` 取自 `CaseWorkPanel::grpcTransportType()` → `openHtmlReportWithFallback`（与 `basedata_view` 一致：WebView2 或下载后系统浏览器）。

### GET `/api/case/report_download` `需认证`

分析报告 下载

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 分析模块：`motor`（机动车）、`slow`（慢行）、`bus`（公交） |

**Phase2 客户端：** 基础方案工作台分析报告「下载」→ `HttpApis::caseReportDownload`；`type` 取自 `CaseWorkPanel::grpcTransportType()`。

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "file_name": "report_p22_c9_u1_1779777877328.zip",
    "file_path": "/www/wwwroot/app-gis/servernews/temp/report_p22_c9_u1_1779777877328.zip",
    "download_url": "http://gisapi.jiheshop.com/temp/report_p22_c9_u1_1779777877328.zip"
  }
}
```

---

## 文件管理

- **支持的上传格式：** CSV, XLSX, TXT, OSM, Shapefile(.shp/.dbf/.shx)
- **存储方式：** MinIO 对象存储

### POST `/api/files/drcsv` `需认证`

导入 CSV/TXT/XLSX 文件到 MinIO 并导入数据库（multipart 上传文件，查询参数见下表）。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `table` | string | 是 | 表名（小写，如 `road_way`、`bus_point`、`road_community` 等） |
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID；处于某方案时填对应方案 id，处于项目基础数据时填 `0` |
| `type` | string | 是 | 文件类型，**小写**：`csv`、`txt`、`xlsx`（`.xls` 客户端按 `xlsx` 提交） |

**Phase2 客户端（方案树 / 基础数据图层右键「导入」）**

- 图层树为三级：**方案或「所有图层」→ 图层组 → 图层**；仅图层叶子节点可右键导入
- 调用：`HttpApis::uploadCSV` → 本接口
- `table`：由图层叶子节点 `val` 直接作为表名（见 `CaseDrcsvHelper::tableNameFromLayerVal`，须在已知表名白名单内）
- `case_id`：方案侧栏图层导入传所属方案 id；基础数据侧栏导入传 `0`
- 导入成功后刷新当前工程方案列表（`UIGlobalState::reloadCurrentProjectCases`）

**Phase2 图层树交互（基础数据 / 方案侧栏，与导入无关）**

| 项 | 说明 |
|----|------|
| 解析 | `CaseMenuHelper::parseLayerGroupsFromJsonArray` / `parseLayerGroupsFromCaseJson`；从 `QJsonObject` 的 `children[]` 读取子图层，不依赖可能为空的 `MenuJson::j_children()` |
| 勾选 | 图层组三态（无/部分/全选），联动组内叶子 WFS 显隐 |
| 展开 | `setExpandsOnDoubleClick(false)`；单击图层组**行正文**切换展开/收起；点击复选框仅改勾选；点击分支箭头区由客户端显式 toggle（自定义 branch 图标后不再走 Qt 默认） |
| 分支箭头 | 四套 PNG（`arrow_right` / `arrow_down` 及 hover 变体）经 `Phase2Ui::darkSidebarTreeBranchStyleRules` 应用于 `::branch`；hover 仅在箭头区域生效 |
| 默认展开/收起 | 方案树 `modelReset` 后 `collapseAll()` 全部收起；基础数据仅「所有图层」「工具」根节点默认展开，图层组不强制展开 |
| 图层样式 | WFS 加载后由 `LayerStyleFactory` 按 `val` / `typename` 应用专用渲染；基础方案路段层默认 type 过滤见 [`docs/layer-filter.md`](layer-filter.md)；详见 [`docs/layer-style-factory.md`](layer-style-factory.md) |

---

## 文章管理 看基础配置

### GET `/api/news`

获取文章列表（支持按 bid 筛选）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `bid` | number | 否 | 文章分类 ID  1:使用手册 2:帮助文档 3:教学案例  |

### GET `/api/news/:id`

获取文章详情

---

## 算法管理

**Phase2 工作台传参约定：**

- 凡调用本节的 `check` / `stream` / `runalg` / `aistream` / `aiapi`，Query 均带 `type`：`motor`（机动车）、`slow`（慢行）、`bus`（公交），对应当前分析模块。
- `stream`、`runalg` 另带 `type2`：`network`（路网现状）、`traffic`（交通问题诊断）、`change`（方案改造）、`review`（方案评审）。
- 普通方案「方案建议」运行按钮使用 `runalg`，`type2=change`；客户端将同步返回内容显示在只读文本框中。
- 方案侧工作台相关接口（`grpc/case_export`、`ptcase_export`、`report_view`、`report_download`、`cost_view`、`editlog`、`export_simulate`、`export_osm`）Query 均带 `type`（`motor` / `slow` / `bus`），对应当前分析模块；客户端统一经 `CaseWorkPanel::grpcTransportType()` 传入。

**Phase2 客户端 HTTP 超时：**

- 普通 REST（`HttpApis` → `QRequest2`）：默认传输超时 **15s**（`kDefaultTransferTimeoutMs`）。
- 长耗时同步接口（`grpc/check`、`grpc/runalg`、`grpc/aiapi`、`grpc/case_export`、项目/方案导出、裁剪、`postFile` 上传等）：传输超时 **30s**（`kLongRunningTransferTimeoutMs`）。
- 传输超时失败时，回调错误文案为 **`响应超时`**（`QRequest2` 将 `OperationCanceledError` / `TimeoutError` 映射为中文提示）。
- 流式 SSE（`grpc/stream`、`grpc/aistream`）：不经 `QRequest2`；`GrpcStreamClient` / `GrpcAiStreamClient` 显式 `setTransferTimeout(0)`，并依赖应用层 idle（15min）/ deadline（1h）定时器。

### GET `/api/grpc/check`

数据检测

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 机动车 motor, 慢行 slow, 公交 bus |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "way": 1,
    "point": 1,
    "community": 1,
    "od": 1
  }
}
```

### GET `/api/grpc/runalg`

运行算法 直接返回结果

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 机动车 `motor`，慢行 `slow`，公交 `bus`，工具 `tool` |
| `type2` | string | 是 | 路网现状分析 `network`，交通问题诊断 `traffic`，方案改造 `change`，方案评审 `review`；工具四阶段 `trip_model_all`，工具 OD 反推 `estimation`（OD 反推改用 `/api/grpc/stream`，参数走弹窗 `json`） |
| `json` | string | 否 | 可选算法参数；OD 数据生成工具传 `{"od_mode":"motor"}` 或 `{"od_mode":"slow"}`；普通方案「方案建议」可不传 |

**Phase2 客户端：** 普通方案「方案建议」点击运行 → `HttpApis::grpcRunAlg`，Query 带 `type2=change`；OD 数据生成工具先经 `CaseOdTypeDialog` 选择 OD 类型（机动车/慢行，取消后终止），再调用 `type=tool`、`type2=trip_model_all`、`case_id=0`，并携带 `json`。成功后在步骤下方只读 `QTextBrowser` 中显示返回内容。客户端展示字段优先级：`data.content`、嵌套 `result.content`、`result`、`message`、原始 JSON；若响应含非空 `content_url`（候选路径同 `content`：`data.content_url`、嵌套 `result.content_url`、根级），在正文末尾追加「查看详情」软链接，点击后经 `openWebViewUrlOrWarn` 用 WebView2 弹窗打开该 URL；失败时同样在文本框中显示错误并弹出提示（无详情链接）。

### GET `/api/grpc/stream`

运行算法 流式输出 sse

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `isbase` | number | 是 | 1 是基础方案 0普通方案 |
| `type` | string | 是 | 机动车 motor, 慢行 slow, 公交 bus, 工具 tool |
| `type2` | string | 是 | 路网现状分析 network, 交通问题诊断 traffic ，微观gis内traffic_micro   , 方案改造 change, 方案评审 review, OD 反推 estimation |
| `ids` | string | 否 | 在gis框选 或单选的id 多个用, 只有评审需要 |
| `token` | string | 是 | 登录的token |
| `json` | string | 否 | 路网现状分析 / OD 反推 参数弹窗 |

**Phase2 客户端：** 基础数据「工具节点」下「OD 反推」工具 → `GrpcStreamClient`，Query 带 `type=tool`、`type2=estimation`、`case_id=0`、`isbase=1`，`json` 为参数弹窗扁平 JSON（同路网现状分析）；弹窗（`OdEstimationDialog`）以 TabWidget 承载「参数配置」与「运行输出」两页，运行后切到第二页只读 `QTextEdit` 显示 SSE 文本。



---

### GET `/api/grpc/aistream`

普通方案建议 流式输出 sse

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 机动车 motor, 慢行 slow, 公交 bus |
| `type2` | string | 是 | 路段优化建议 way, 信号配时调整 mobile, 路网拓展方案 network |
| `token` | string | 是 | 登录的 token |
| `prompt` | string | 是 | 输入内容（方案建议场景可用步骤标题或用户描述） |

**Phase2 客户端：** 当前普通方案「方案建议」已改用 `/api/grpc/runalg?type2=change` 同步返回并展示；`GrpcAiStreamClient` 类仍保留，但不再由方案建议三个子按钮触发。

**响应示例：**

```json
{
	"data": {
		"code": 0,
		"message": "Running",
		"data": {
			"percent": 9,
			"done": 0,
			"title": "标题1"
		},
		"result": null
	}
}
```

### GET `/api/grpc/aiprovider` `需认证`

获取可用 AI 模型列表。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": [
    {
      "id": "deepseek",
      "label": "DeepSeek"
    }
  ]
}
```

**Phase2 客户端：** 尚未实现；可选配合 `/api/grpc/aiapi` 的 `llm_provider` 参数使用。

### GET `/api/grpc/aiapi` `需认证`

AI 多级提示（**同步 GET**，非 SSE）。客户端等待单次 JSON 响应，将 `data.content`（或兼容字段 `message`）展示在智能对话窗；若存在 `download_url` 则渲染为 **「查看详情」** 下载链接。

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `project_id` | number | 是 | 项目 ID |
| `case_id` | number | 是 | 方案 ID |
| `type` | string | 是 | 机动车 motor, 慢行 slow, 公交 bus |
| `type2` | string | 是 | 路段优化建议 way, 信号配时调整 mobile, 路网拓展方案 network |
| `token` | string | 是 | 登录的 token（Query；同时建议 Header `Authorization: Bearer`） |
| `prompt` | string | 是 | 用户输入 |
| `analysis_object` | string | 是 | 分析对象（Phase2 客户端：L1 Chip 当前选中项，`AiConfigModel::optionLabel(L1, …)`） |
| `engineer_notes` | string | 是 | 特殊问题类型（Phase2 客户端：L2 Chip 当前选中项，`AiConfigModel::optionLabel(L2, …)`） |
| `business_goal` | string | 是 | 业务类型（Phase2 客户端：常见问题下拉当前选中值） |
| `planning_targets` | string | 是 | 附加 C：规划预期与目标 |
| `constraints` | string | 是 | 附加 A：改造类型约束等 |
| `ids` | string | 否 | 框选的路段 id，如 `1,2,3`（Phase2：已框选路段时传入；L1 含「框选」选项时仅 L1 为「框选」且已框选时传入） |
| `llm_provider` | string | 否 | AI 模型标识，如 `deepseek`（可通过 `/api/grpc/aiprovider` 获取可选值） |

**响应示例：**

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "content": "（展示给用户的回复正文，可为 Markdown 或 JSON 文本）",
    "download_url": "http://gisapi.jiheshop.com/api/download/ai_report_1780458547964.md"
  }
}
```

**Phase2 客户端：**

- `HttpApis::grpcAiApi`；`CaseWorkPanel` 在共享宿主步骤（基础方案 `traffic` / 普通方案 `review`）挂载 `AIChatWidget`，调用 `setUseBackendAiApi(true)`。
- 发送时发出 `promptSubmitted(prompt, l1, l2, commonQuestion)`，由 `onAiChatPromptSubmitted` 调用本接口；`type2` 取自步骤 `FlowStep::type2`（如 `network`、`traffic`、`review`）。
- 等待期间禁用输入、L1/L2 选词与常见问题下拉；先 `beginAssistantReply()` 显示「正在思考…」，返回后 `finishAssistantReply` 将 **`content`** 填入「智能助手」气泡；**`download_url`** 经 `AIChatMessage::downloadUrl` 结构化传递，不拼入正文 Markdown。
- 选词映射：`analysis_object` = L1 Chip（`optionLabel(L1, …)`）；`engineer_notes` = L2 Chip（`optionLabel(L2, …)`）；`business_goal` = 常见问题下拉；`planning_targets`、`constraints` 传空字符串；`ids` = 已框选路段 id（无则省略；L1 含「框选」时离开「框选」会清空本地已选）。
- 回复与运行日志统一写入 `FlowStep::chatTimeline`；AI 回调先更新 timeline 再刷新 UI；`refreshWorkPage` 重建控件后 `restoreChatTimeline` 恢复。
- AI 回复含 `download_url` 时，客户端渲染「查看详情」链接，链接文本颜色与 AI 回复正文一致。

---

## SSE 事件订阅说明

连接方式：使用 Server-Sent Events (SSE) 接收实时通知

名称格式：`p{project_id}_u{user_id}_c{case_id}_motor_network`

参数加 token：`?token=xxx`

**响应示例：**

```json
// 开始
{
  "data": {
    "code": 0,
    "message": "Starting",
    "data": {
      "percent": 0,
      "done": 0,
      "title": "正在加载路网数据..."
    },
    "result": null
  }
}

// 进行中
{
  "data": {
    "code": 0,
    "message": "Running",
    "data": {
      "percent": 10,
      "done": 0,
      "title": "路网加载完成，正在迭代求解..."
    },
    "result": null
  }
}

// 完成
{
  "data": {
    "code": 1,
    "message": "Success",
    "data": {
      "percent": 100,
      "done": 1,
      "title": "交通分配完成，共迭代 1 次"
    },
    "result": {
      "code": 1,
      "message": "Success",
      "data": {
        "tables": {
          "items": {
            "od_table": "user_project.12_1_2_od_demand",
            "network_table": "user_project.12_1_2_network_topology_information_table"
          }
        },
        "counts": { "items": { "iter": "1" } },
        "metrics": null
      },
      "summary": {
        "logs": [],
        "attributes": { "od_mode": "road_node_fallback" },
        "counts": {},
        "metrics": {},
        "stage": "base_motor_network"
      }
    }
  }
}
```

---

## WFS 地理要素服务

**接口说明：** WFS (Web Feature Service) 提供空间图层数据查询，响应格式为 `application/geo+json`。所有 WFS 接口无需认证（`@Public`）。

**数据来源：** `user_project` schema 下的所有含 geometry 列的表。

### GET `/wfs?request=GetCapabilities` `公开`

获取可用图层列表

**请求示例：**

```
GET /wfs?request=GetCapabilities
```

**响应示例：**

```json
{
  "service": "WFS",
  "version": "2.0.0",
  "layers": [
    "project12_user1_case2_road_way",
    "project20_user1_road_way",
    "project20_user1_bus_point",
    "project5_user1_road_community"
  ]
}
```

### GET `/wfs?request=DescribeFeatureType` `公开`

获取图层字段描述

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `request` | string | 是 | 固定 `DescribeFeatureType` |
| `typeName` | string | 是 | 图层表名 |

**请求示例：**

```
GET /wfs?request=DescribeFeatureType&typeName=project12_user1_case2_road_way
```

**响应示例：**

```json
{
  "typeName": "project12_user1_case2_road_way",
  "properties": [
    { "name": "link_id", "type": "integer" },
    { "name": "init_node", "type": "integer" },
    { "name": "term_node", "type": "integer" },
    { "name": "capacity", "type": "numeric" },
    { "name": "length", "type": "numeric" },
    { "name": "geometry", "type": "geometry" }
  ]
}
```

### GET `/wfs?request=GetFeature` `公开`

获取图层要素数据（GeoJSON FeatureCollection）

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `request` | string | 是 | 固定 `GetFeature` |
| `typeName` | string | 是 | 图层表名（完整名，如 `project12_user1_case2_road_way`） |
| `bbox` | string | 否 | 空间过滤，格式 `south,west,north,east` |
| `maxFeatures` | number | 否 | 最大返回数，默认 1000 |
| `startIndex` | number | 否 | 偏移量（分页） |
| `cql_filter` | string | 否 | 属性过滤，格式 `key=value` |
| `projectNo` | string | 否 | 项目编号前缀，配合短表名使用 |

**请求示例 1 - 直接使用完整表名：**

```
GET /wfs?request=GetFeature&typeName=project12_user1_case2_road_way
```



效果等同于查询 `user_project.project12_user1_case2_road_way`

**请求示例 3 - bbox 空间过滤：**

```
GET /wfs?request=GetFeature&typeName=road_way&projectNo=project20_user1&bbox=18.2,109.4,18.4,109.6
```

**请求示例 4 - 分页查询：**

```
GET /wfs/feature?typeName=project12_user1_case2_road_way&maxFeatures=50&startIndex=100
```

**请求示例 5 - 属性过滤：**

```
GET /wfs?request=GetFeature&typeName=road_way&projectNo=project20_user1&cql_filter=link_id=305
```

**响应示例（Content-Type: `application/geo+json`）：**

```json
{
  "type": "FeatureCollection",
  "numberMatched": 1520,
  "numberReturned": 50,
  "features": [
    {
      "type": "Feature",
      "id": 1,
      "geometry": {
        "type": "LineString",
        "coordinates": [[109.45, 18.25], [109.46, 18.26]]
      },
      "properties": {
        "link_id": 101,
        "init_node": 1,
        "term_node": 2,
        "capacity": 1800,
        "length": 320.5
      }
    }
  ]
}
```

### GET `/wfs/capabilities` `公开`

快捷路由，等同于 `?request=GetCapabilities`

**服务端优化建议（配合 Phase2 QGIS 客户端，降低间歇性 typename 校验失败）：**

| 项 | 建议 |
|----|------|
| Capabilities 体量 | 当前全局文档含全部项目 FeatureType（约 1MB+）；建议支持按 `projectNo` / `caseId` 过滤，仅返回当前项目相关图层 |
| HTTP 缓存 | `GetCapabilities` 响应建议 `Cache-Control: no-cache` 或 `max-age=0, must-revalidate`；质心/导入/克隆方案后确保 WFS 已注册 typename **再** 返回 API menu |
| 发布时序 | 避免 API `menu`/`list` 已含新 `value`，但 Capabilities 尚未包含对应 `<Name>` 的窗口期 |
| 可观测性 | 记录 Capabilities 生成时间与 FeatureType 数量，便于对照客户端缓存版本 |

### GET `/wfs/feature` `公开`

快捷路由，等同于 `?request=GetFeature`，参数同上（`typeName` 必填）

**前端调用示例：**

```js
// 获取图层 GeoJSON 数据
const res = await fetch('/wfs/feature?typeName=road_way&projectNo=project20_user1&bbox=18.2,109.4,18.4,109.6');
const geojson = await res.json();
// 可直接用于 Leaflet / Mapbox 渲染
map.addSource('roads', { type: 'geojson', data: geojson });
```

### WFS-T 事务操作

---

#### 插入要素

**POST /wfs/feature**

请求参数 (Body JSON):

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| typeName | string | 是 | 图层/表名 全名 |
| feature | object | 是 | GeoJSON Feature (含 geometry 和 properties) |
| projectNo | string | 否 | 项目编号前缀 不填 |

请求示例:
```json
{
  "typeName": "road_point",
  "projectNo": "project20",
  "feature": {
    "geometry": {
      "type": "Point",
      "coordinates": [109.5, 18.25]
    },
    "properties": {
      "name": "新增站点",
      "type": "bus_stop",
      "capacity": 100
    }
  }
}
```

响应示例:
```json
{
  "success": true,
  "insertedId": 123,
  "message": "Feature inserted successfully"
}
```

---

#### 更新要素

**PUT /wfs/feature**

请求参数 (Body JSON):

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| typeName | string | 是 | 图层/表名 全名 |
| featureId | string/number | 是 | 要素 ID；**推荐为图层主键值**（如 `link_id`、`area_id`），客户端解析到主键时应与动态主键字段一致；无法解析时回退 QGIS 内部 fid |
| feature | object | 是 | 要更新的内容 (含 geometry 和/或 properties) |
| *主键字段名* | string/number | 否 | 与图层真实主键同名的额外字段（如 `link_id`、`area_id`），值为该要素主键；服务端优先用此字段定位记录 |
| projectNo | string | 否 | 项目编号前缀 不填 |

请求示例:
```json
{
  "typeName": "project22_user1_case9_road_way",
  "featureId": 305,
  "link_id": 305,
  "projectNo": "project20",
  "feature": {
    "properties": {
      "name": "修改后的路段名",
      "capacity": 200
    }
  }
}
```

响应示例:
```json
{
  "success": true,
  "message": "Feature updated successfully"
}
```

---

#### 删除要素

**DELETE /wfs/feature**

请求参数 (Body JSON):

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| typeName | string | 是 | 图层/表名 全名 |
| featureId | string/number | 是 | 要素 ID；**推荐为图层主键值**（如 `link_id`、`area_id`），客户端解析到主键时应与动态主键字段一致；无法解析时回退 QGIS 内部 fid |
| *主键字段名* | string/number | 否 | 与图层真实主键同名的额外字段（如 `link_id`、`area_id`），值为该要素主键；服务端优先用此字段定位记录 |
| projectNo | string | 否 | 项目编号前缀 不填 |

请求示例:
```json
{
  "typeName": "project22_user1_case9_road_way",
  "featureId": 305,
  "link_id": 305,
  "projectNo": "project20"
}
```

响应示例:
```json
{
  "success": true,
  "message": "Feature deleted successfully"
}
```

---

#### WFS-T Transaction (XML 格式 - QGIS 兼容)

**POST /wfs** Content-Type: application/xml

兼容 QGIS 等 GIS 客户端发送的标准 WFS-T Transaction XML 请求，支持 Insert、Update、Delete 操作。

Insert 示例:
```xml
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs" service="WFS" version="1.1.0">
  <wfs:Insert>
    <road_point>
      <geometry>
        <gml:Point xmlns:gml="http://www.opengis.net/gml">
          <gml:coordinates>109.5,18.25</gml:coordinates>
        </gml:Point>
      </geometry>
      <name>新站点</name>
    </road_point>
  </wfs:Insert>
</wfs:Transaction>
```

Update 示例:
```xml
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs" service="WFS" version="1.1.0">
  <wfs:Update typeName="road_point">
    <wfs:Property>
      <wfs:Name>name</wfs:Name>
      <wfs:Value>修改后名称</wfs:Value>
    </wfs:Property>
    <wfs:Filter>
      <FeatureId fid="123"/>
    </wfs:Filter>
  </wfs:Update>
</wfs:Transaction>
```

Delete 示例:
```xml
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs" service="WFS" version="1.1.0">
  <wfs:Delete typeName="road_point">
    <wfs:Filter>
      <FeatureId fid="123"/>
    </wfs:Filter>
  </wfs:Delete>
</wfs:Transaction>
```

Transaction 响应:
```json
{
  "success": true,
  "transactionResponse": {
    "totalInserted": 1,
    "totalUpdated": 0,
    "totalDeleted": 0,
    "insertedIds": [124],
    "errors": []
  }
}
```

---

#### 批量 JSON Transaction

**POST /wfs** Content-Type: application/json

updates / deletes 单条元素：`featureId` 推荐为图层主键值，与动态主键字段（如 `link_id`、`area_id`）保持一致；服务端优先用主键字段定位记录。

公交路段（`*_bus_way`）新增 insert 项须与 `typeName` 同级附带 `busType`，取值：`new`（新建）/ `expand`（扩建）/ `reroute`（改线）。

请求示例:
```json
{
  "inserts": [
    {
      "typeName": "road_point",
      "feature": {
        "geometry": { "type": "Point", "coordinates": [109.5, 18.2] },
        "properties": { "name": "点A" }
      }
    },
    {
      "typeName": "project22_user1_bus_way",
      "busType": "new",
      "feature": {
        "geometry": { "type": "LineString", "coordinates": [[109.5, 18.2], [109.51, 18.21]] },
        "properties": {}
      }
    }
  ],
  "updates": [
    {
      "typeName": "project22_user1_case9_road_way",
      "featureId": 305,
      "link_id": 305,
      "feature": { "properties": { "name": "改名" } }
    }
  ],
  "deletes": [
    { "typeName": "project22_user1_case9_road_way", "featureId": 99, "link_id": 99 }
  ]
}
```

---

## 交互流程

基础方案与普通方案，工作台内容不一样，切换方案时，跟随切换；但是都分类型：机动车 motor，慢行 slow，公交 bus，在工作台交互调用接口时，都要传入这个类型（含报告查看/下载、成本效益、修改记录、仿真/OSM 导出等方案侧接口）。

### 基础方案

**说明：** 流程

1. `/api/case/list`（一个项目只有一个基础方案，其他为普通方案，通过 `isbase` 判断）
2. `/api/grpc/check` 数据支撑集检查（1 为已加载，0 为未加载）
3. `/api/grpc/stream` 运行路网现状分析时，要先弹出交通分配算法参数弹窗，将参数填入 `json` 字段
   - 3.1 路网流量分布
   - 3.2 OD 分布
   - 3.3 交通饱和度
4. `/api/grpc/stream` 交通问题诊断（无弹窗参数）
   - 4.1 路网流量分布
   - 4.2 预设流量与现状差值
   - 4.3 预设饱和度与现状差值
   - 4.4 交通饱和度
   - 4.5 OD 分布
   - 4.6 饱和 OD 差值
5. `/api/grpc/case_export` 生成分析报告，随后 `/api/case/report_view` 查看或 `/api/case/report_download` 下载
6. `/api/case/add` 改造方案（添加普通方案）
7. `/api/case/copyproject` 基础方案右键「覆盖」项目基础数据

渲染和显示见 https://docs.qq.com/doc/DTUtxZmZyQU1ocVFB

### 项目工具

- `/api/case/add` 改造方案 / 添加方案
- **基础数据分析**（侧栏工具）：`GET /api/case/basedata_view`（`project_id`）；响应 `data.content` 为 HTML 文件 URL，客户端优先 WebView2 弹窗打开，失败则下载后用本地默认浏览器打开
- **OD数据生成**（侧栏工具）：先弹出 `CaseOdTypeDialog` 选择机动车（`motor`）或慢行（`slow`），取消不请求；`GET /api/grpc/runalg`，`type=tool`，`type2=trip_model_all`，`case_id=0`，`json={"od_mode":"motor"|"slow"}`；外层 `code=200` 后以 `data.code==1` 为业务成功，否则用 `data.message` 警告且不加载图层；成功后勾选并加载 `other_od` 图层并应用 OD 渲染样式
- **OD反推**（侧栏工具，「生成人口数据」之后固定节点）：`GET /api/grpc/stream`，`type=tool`，`type2=estimation`，`case_id=0`，`isbase=1`，`json` 为 OD 反推参数弹窗；弹窗 `OdEstimationDialog` 第一页配置参数、第二页显示 SSE 输出
- **基础方案路段层**：客户端 WFS 加载时不传 `cql_filter`；默认 `"type" < 10` 由客户端过滤实现；工作台路网流量/饱和度分析上图时解除 type 限制并应用分级样式（见 [`layer-filter.md`](layer-filter.md)、[`layer-style-factory.md`](layer-style-factory.md)）
- OSM 数据下载：URL 地址
- 公交站点覆盖率：暂无效果
- 轨道站点覆盖率：暂无效果
- 轨道运营：暂无效果
- 公交运营信息：暂无效果
- 公交可达性：暂无效果

### 普通方案

**说明：** 流程

1. 同基础方案步骤 1–2
2. `/api/grpc/runalg` 方案建议（Query 含 `type2=change`），返回内容显示在只读文本框中
3. `/api/grpc/stream` 方案评审
   - 3.1 评审结果
4. `/api/case/cost_view` 成本效益分析（HTML URL，`openHtmlReportWithFallback`）
5. 智能对话输入：`/api/grpc/aiapi`（同步 GET，`AIChatWidget` + `setUseBackendAiApi`）
6. `/api/case/ptcase_export` 生成改造方案报告

### 右键

1. `/api/case/ptcase_export` 生成改造方案报告
2. 计算诊断指标（框选后多图层）：需提供各方法所需图层，返回框选 id，分机动车、慢行、公交

---

## 通用说明

### 通用响应格式

```json
{
  "code": 200,
  "message": "成功",
  "data": {}
}
```

### 常见错误码说明

| 错误码 | 说明 |
|--------|------|
| 200 | 成功 |
| 400 | 请求参数错误 |
| 401 | 未授权 |
| 404 | 资源不存在 |
| 500 | 服务器错误 |
