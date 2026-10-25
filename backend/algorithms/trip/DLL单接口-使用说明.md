# DLL 单接口使用说明（一次调用先生成再分布）

本文聚焦 DLL 方案的“单接口模式”，即一次调用先执行“出行生成（Trip Generation）”，再执行“出行分布（Trip Distribution）”，并返回一次性 JSON 结果。包含：总接口说明、Qt 调用 DLL 注意事项、生成/分布的输入输出与参数、统一返回 JSON 结构，以及简要构建与测试指引。

---

## 1. 总接口概览

- 导出函数（cdecl）

  ```cpp
  // dll: dist_dll/trip_model.dll
  extern "C" __declspec(dllexport)
  int run_trip_model_all(void (*on_progress)(int,double),
                         const char* cfg_json,
                         const char* taz,
                         const char* hist_od,
                         /*out*/ char** out_json);

  extern "C" __declspec(dllexport)
  void free_json(char* p);
  ```

- 参数
  - `on_progress(it,pct)`：进度回调（可空）。单接口内部会把生成阶段映射到 0..50，分布阶段映射到 50..100。
  - `cfg_json`：配置 JSON 文本（UTF-8）。至少包含：
    - `db_conn_str`：`host,port,dbname,user,password` 或 `connection_string`。并建议提供：`role`（schema）、`scenario_prefix`（表名前缀）。
    - `algorithm_params`：两阶段算法参数（见第 3 节）。
  - `taz`：TAZ 输入表名。可传完整限定名 `schema.table`，也可依赖数据库 `search_path`。
  - `hist_od`：历史 OD 表名（可选）。传空字符串或 NULL 时，不使用历史OD标定，直接采用 `algorithm_params.param_lambda`；不会自动尝试任何默认历史OD表。
- 返回值
  - `rc=0` 表示调用成功（不等于算法一定成功，需看返回 JSON 的 `status`）。
  - JSON 通过 `out_json` 返回，使用 `free_json(out_json)` 释放。
- 表名解析（若未显式传入相应表名，DLL 会根据 `role` 与 `scenario_prefix` 自动拼装；当 `prefix` 以 `_` 结尾时不会重复添加下划线）
  - TAZ 默认：`<role>.<prefix>{_}other_taz_socioeconomic`
  - 生成/吸引表：`<role>.<prefix>{_}other_trip_production_attraction`
  - 分布（OD）表：`<role>.<prefix>{_}other_od`
  - 历史 OD（用于 λ 标定，见 3.2）：约定命名 `<role>.<prefix>trip_distribution_hist`。仅当调用方显式传入非空 `hist_od` 时才触发历史OD流程；若传入表不可读，内部会自动按“给定表→默认 hist → other_od 复制”的顺序尝试。

---

## 2. Qt 调用 DLL 注意事项

- 加载与函数指针（cdecl）

  ```cpp
  #include <QLibrary>
  typedef void (*ProgressCB)(int,double);
  typedef int  (*RunAll)(ProgressCB, const char*, const char*, const char*, char**);
  typedef void (*FreeJson)(char*);

  QLibrary lib("dist_dll/trip_model");
  RunAll  runAll  = (RunAll) lib.resolve("run_trip_model_all");
  FreeJson freeJs = (FreeJson)lib.resolve("free_json");

  QByteArray cfg = QFile::readAll("tools/cfg.json");
  QByteArray taz = QByteArray("user_project.project59_taz_socioeconomic");
  QByteArray histOd = QByteArray(""); // 可选，空字符串/NULL 则跳过历史OD标定（使用默认λ）

  char* out = nullptr;
  auto cb = [](int it, double pct){ /* 更新 UI 请用 QueuedConnection 切主线程 */ };
  int rc = runAll(cb, cfg.constData(), taz.constData(), histOd.constData(), &out);
  QByteArray resultJson = out ? QByteArray(out) : QByteArray();
  if (out) freeJs(out);
  ```

- 内存与线程
  - `out_json` 字符串由 DLL 分配，必须用 `free_json` 释放。
  - 回调在调用线程触发；更新 UI 请用信号或 `Qt::QueuedConnection` 切回主线程。
- Python 运行时与路径
  - DLL 会尝试把 `<project_root>` 和 `dist_dll` 加入 `sys.path`。
  - 若存在嵌入式 Python 于 `dist_dll/pyhome`，DLL 会通过 `Py_SetPythonHome` 指向此目录。
  - 若目录结构有变，可在进程启动前设置环境：`PYTHONPATH=<project_root>`；确保 `PATH` 能找到 `pythonXY.dll`。
- 编码
  - DLL/脚本返回 JSON 为 UTF-8。Windows 控制台建议 `chcp 65001` 或在 Qt 侧用 `QString::fromUtf8`。

---

## 3. 生成/分布：输入、输出与参数

### 3.1 出行生成（Trip Generation）

#### 输入表：TAZ 社会经济属性表
| 字段名 | 类型 | 必需 | 描述 |
|--------|------|------|------|
| id | INTEGER | 是 | 分区唯一标识（主键） |
| pop | NUMERIC | 可选 | 居住人口 |
| cars | NUMERIC | 可选 | 车辆保有量 |
| eind | NUMERIC | 可选 | 产业就业岗位数 |
| eret | NUMERIC | 可选 | 零售就业岗位数 |
| eoth | NUMERIC | 可选 | 其他就业岗位数 |
| du | NUMERIC | 可选 | 住户数/住宅单元 |
| hinc | NUMERIC | 可选 | 家庭收入 |
| area | NUMERIC | 可选 | 分区面积 |
| etot | NUMERIC | 可选 | 总就业岗位数 |
| longitude | NUMERIC | 推荐 | 经度（WGS84，用于分布阶段） |
| latitude | NUMERIC | 推荐 | 纬度（WGS84，用于分布阶段） |
|geometry	|geometry(Point,4326)	|PostGIS 点几何（WGS84）	|用于网络/空间分析|

#### 字段选择规则
- 程序自动检测 TAZ 表中存在的字段，从内置支持字段中选择实际可用的进行计算
- 要求：至少 1 个生产字段、至少 1 个吸引字段；否则返回失败 JSON
- 支持字段：`pop,cars,eind,eret,eoth,du,hinc,area,etot`

#### 系数配置（algorithm_params）
- **内置默认系数**：
  - 生产系数：`pop:2.0, cars:0.7, eind:0.1, eret:0.2, eoth:0.15, du:0.1, hinc:0.05/10000, area:0.0, etot:0.0`
  - 吸引系数：`pop:1.0, cars:0.5, eind:1.5, eret:2.0, eoth:1.2, du:0.8, hinc:0.03/10000, area:0.0, etot:0.0`
- **覆盖方式**（任选其一或混用）：
  - 字典方式：`"production_rates": {"pop": 1.8}, "attraction_rates": {"eind": 1.6}`
  - 扁平键方式：`"p_pop_rate": 1.8, "a_eind_rate": 1.6`

#### 输出表：出行生成吸引表
| 字段名 | 类型 | 描述 |
|--------|------|------|
| zone_id | INTEGER | 分区ID（对应TAZ.id） |
| production | NUMERIC | 出行生成量（线性模型计算） |
| attraction | NUMERIC | 出行吸引量（按总生成量配平） |

### 3.2 出行分布（Trip Distribution）

#### 输入表1：TAZ 坐标表（复用生成阶段的TAZ表）
| 字段名 | 类型 | 必需 | 描述 |
|--------|------|------|------|
| id | INTEGER | 是 | 分区唯一标识 |
| longitude | NUMERIC | 是 | 经度（WGS84，用于距离计算） |
| latitude | NUMERIC | 是 | 纬度（WGS84，用于距离计算） |

#### 输入表2：出行生成吸引表
| 字段名 | 类型 | 必需 | 描述 |
|--------|------|------|------|
| zone_id | INTEGER | 是 | 分区ID（对应TAZ.id） |
| production | NUMERIC | 是 | 出行生成量 |
| attraction | NUMERIC | 是 | 出行吸引量 |

#### 输入表3：历史OD表（可选，用于λ标定）
| 字段名 | 类型 | 必需 | 描述 |
|--------|------|------|------|
| id | BIGINT | 可选 | 建议为第一列，自增主键 |
| f_id | INTEGER | 是 | 起点分区ID |
| t_id | INTEGER | 是 | 终点分区ID |
| demand | NUMERIC | 是 | 历史出行需求量 |
| distance | NUMERIC | 是 | 距离 |

说明：算法仅读取 `f_id,t_id,demand` 三列用于标定，是否存在 `id` 不影响运行。

#### 算法参数（algorithm_params）
- **基本参数**：
  - `param_lambda`：阻抗参数（默认0.1）
  - `diag_eps`：对角线最小值（默认0.1）
  - `max_iter`：最大迭代次数（默认20）
  - `tol`：收敛容差（默认1e-6）
- **历史OD标定λ（自动）**：
  - 历史 OD 使用规则：仅当显式传入 `hist_od` 表名且能读取到数据（行数>0）时，才基于历史 OD 与距离矩阵标定 `λ`；否则跳过标定，直接使用 `param_lambda`。
  - 标定参数：`lambda_min, lambda_max`（搜索区间，默认 0.01/1.0）、`lambda_steps`（默认10）、`max_iter_calib, tol_calib`（默认30/1e-5）。

#### 输出表：OD需求表
| 字段名 | 类型 | 描述 |
|--------|------|------|
| id | INTEGER | 自增id |
| f_id | INTEGER | 起点分区ID |
| t_id | INTEGER | 终点分区ID |
| demand | NUMERIC | OD出行需求量（双约束重力模型结果） |
| m_special | VARCHAR | 特殊标记（当前固定为'N'） |

**注意**：当前实现未生成自增`id`列，消费端不应依赖`id`字段。

---

## 4. 统一返回 JSON 结构（单接口）

- 顶层键：`status`、`message`、`data`、`summary`、`inputs`
- 状态码（建议）
  - `1` Success：生成与分布均成功。
  - `0` Unknown/Error：其它失败；具体看 `summary.gen_status / dist_status` 与 `message`。
- 最小示例

  ```json
  {
    "status": 1,
    "message": "Success",
    "data": {
      "tables": {
        "trip_generation_table": "user_project.project59_other_trip_production_attraction",
        "trip_distribution_table": "user_project.project59_other_od"
      },
      "counts": {"pa_rows": 16, "od_rows": 256}
    },
    "summary": {
      "gen_status": 1,
      "dist_status": 1,
      "gen_message": "Success",
      "dist_message": "Success"
    },
    "inputs": {
      "taz_table": "user_project.project59_taz_socioeconomic",
      "hist_od_table_given": "",
      "default_hist_od_table": "user_project.project59_trip_distribution_hist",
      "hist_od_used": 0,
      "hist_od_rows": 0,
      "db": {"host": "localhost", "port": 5432, "dbname": "postgres", "user": "postgres", "role": "user_project", "scenario_prefix": "project59_"},
      "algorithm_params": {"param_lambda": 0.1, "diag_eps": 0.1, "max_iter": 20, "tol": 1e-6, "lambda_min": 0.01, "lambda_max": 1.0, "lambda_steps": 10}
    }
  }
  ```
- **说明**
  - `summary` 中新增 `tables` 字段，会再次回显本次实际写入的生成/分布输出表名，便于日志定位。
  - `inputs.default_hist_od_table` 为推荐命名的回显；当调用方传入非空 `hist_od` 时，算法会依次尝试“给定表→默认 hist → other_od 复制到默认 hist”，缺省/空字符串则跳过。
- **输出方式**
  - JSON 通过 `out_json` 指针返回，使用 `free_json` 释放内存
- **参数变更**
  - 新增可选的 `hist_od` 参数，支持指定历史OD表名
- **兼容性说明**
  - 若客户端环境加载到旧版 `trip_distribution`（仅4参），DLL 会自动回退到4参调用（不启用历史OD标定）。建议确保加载到最新版模块以启用标定。
