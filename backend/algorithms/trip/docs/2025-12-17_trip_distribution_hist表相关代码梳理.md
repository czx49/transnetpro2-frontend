# trip_distribution_hist表相关代码梳理（2025-12-17）

## 1. 表名来源/拼接规则

- **表名约定**：默认历史OD表为`<role>.<scenario_prefix>trip_distribution_hist`。
- **role / scenario_prefix来源**：来自`cfg_json`里的`db_conn_str.role`与`db_conn_str.scenario_prefix`。

### 1.1 C++侧（DLL聚合接口）默认表名回显

- **位置**：`bin/algorithm/trip/dist_dll/wrapper.cpp`，函数`call_py_model_all`
- **关键逻辑**：
  - `default_hist`在C++侧仅用于**回显到返回JSON的inputs**，方便日志定位；并不直接执行数据库读写。
  - 拼接方式：
    - `std::string default_hist = role_in + "." + prefix_in + "trip_distribution_hist";`

## 2. trip_distribution_hist的实际数据库操作（Python侧）

> 实际对`trip_distribution_hist`进行建表/清空/写入/查询的逻辑都在Python模块`trip_distribution/trip_distribution.py`中。

### 2.1 “启用历史OD”触发条件

- **入口**：`trip_distribution/trip_distribution.py` -> `fs_run_trip_distribution(on_progress, cfg, taz_table_name, trip_generation_table_name, hist_od_table_name=None)`
- **关键规则**：
  - 只有当调用方**显式传入非空**`hist_od_table_name`（DLL侧对应参数`hist_od`非空字符串）时，才会进入历史OD流程。
  - 如果`hist_od_table_name`为空/未传，则不会读取任何历史OD，也不会做任何回退拷贝；直接跳过历史OD标定：`logs.append("hist.od not provided: skip calibration")`，并用默认参数（如`param_lambda`）计算现状OD。

### 2.2 历史OD表选择/回退链路（给定表 -> 默认hist表 -> other_od复制）

- **位置**：`trip_distribution/trip_distribution.py`，`fs_run_trip_distribution`中“历史OD处理”段落
- **默认历史OD表名**：
  - `sep = "" if prefix.endswith("_") else "_"`
  - `default_hist_od_table = f"{role}.{prefix}{sep}trip_distribution_hist"`
- **other_od表名（用于拷贝回退）**：
  - `sep = "" if prefix.endswith("_") else "_"`
  - `other_od_table = f"{role}.{prefix}{sep}other_od"`

回退顺序如下：

1. **优先使用调用方给定历史OD表**
   - 判定函数：`_table_has_rows(conn, hist_od_table_given)`
   - 逻辑：若存在且行数>0，则作为历史OD来源。

2. **给定表不可用时，尝试默认历史OD表 trip_distribution_hist**
   - 判定函数：`_table_has_rows(conn, default_hist_od_table)`
   - 逻辑：若存在且行数>0，则使用该表。

3. **给定表与默认hist表都不可用时，尝试用other_od复制生成默认hist表**
   - 前置判定：`_table_has_rows(conn, other_od_table)`
   - 若other_od可用：
     - **建表（如不存在）**：
       - `CREATE TABLE IF NOT EXISTS {default_hist_od_table} (f_id INTEGER NOT NULL, t_id INTEGER NOT NULL, demand NUMERIC)`
     - **清空默认hist表**：
       - `DELETE FROM {default_hist_od_table}`
     - **插入复制**：
       - `INSERT INTO {default_hist_od_table}(f_id, t_id, demand) SELECT f_id, t_id, demand FROM {other_od_table}`
     - 复制后再次用`_table_has_rows`验证是否成功。

   说明：这一步发生在**算法运行前**，用途是“兜底给标定阶段提供一份可读的历史OD”，因此采用尽量不破坏既有表结构的方式（`CREATE IF NOT EXISTS + DELETE + INSERT`）。

4. **最终读取历史OD（用于λ标定）**
   - 读取函数：`_read_hist_od(conn, hist_od_table_name)`
   - SQL：`SELECT f_id, t_id, demand FROM {hist_od_table}`
   - 若读取失败或行数<=0：
     - 会关闭历史OD标定并记录中文提示：`"没有可用的历史OD数据，请检查数据表"`

### 2.3 trip_distribution_hist参与的算法用途（λ标定）

- 历史OD读入后会被传入：`_trip_distribution(..., hist_od_df=hist_od_df)`
- 若`hist_od_df`非空：
  - 调用`_calibrate_lambda_with_hist_od(taz_df, hist_od_df, C, alg)`
  - 本质是：把历史OD转为矩阵$T_{hist}$，计算观测平均阻抗$c_{obs}$，并在$[\lambda_{min},\lambda_{max}]$上离散搜索（步数`lambda_steps`），选取使模型平均阻抗$c_{model}$最接近$c_{obs}$的$\lambda$。

### 2.4 算法运行结束后：用最新other_od强制覆盖trip_distribution_hist

- **时机**：`other_od`（即`out_distribution_table_name`）写入完成之后。
- **触发条件**：默认每次运行结束后都会执行（与是否显式传入`hist_od_table_name`无关）。
- **行为**：以本次运行得到的最新OD为准，对默认历史OD表做“结构+数据”的完全覆盖：
  - `DROP TABLE IF EXISTS {default_hist_od_table}`
  - `CREATE TABLE {default_hist_od_table} (LIKE {out_distribution_table_name} INCLUDING ALL)`
  - `INSERT INTO {default_hist_od_table} SELECT * FROM {out_distribution_table_name}`

说明：覆盖后`trip_distribution_hist`的字段、类型、默认值、约束会与`other_od`保持一致（不再只保留`f_id/t_id/demand`三列）。

## 3. 关联模块中对trip_distribution_hist的“测试/造数”操作

### 3.1 生成测试历史OD表（建表+清空+插入）

- **位置**：`bin/algorithm/trip/tools/generate_test_data.py`
- **作用**：用于快速生成一批TAZ与一批历史OD数据，写入数据库。
- **涉及trip_distribution_hist的操作**：
  - 默认表名：
    - `sep = "" if prefix.endswith("_") else "_"`
    - `hist_od_table = args.hist_od_table or f"{role}.{prefix}{sep}trip_distribution_hist"`
  - 建表：`CREATE TABLE IF NOT EXISTS {fq_table} (f_id INTEGER NOT NULL, t_id INTEGER NOT NULL, demand NUMERIC)`
  - 清空：`DELETE FROM {fq_table}`
  - 插入：`INSERT INTO {fq_table}(f_id,t_id,demand) VALUES (%s,%s,%s)`（批量executemany）

### 3.2 DLL测试脚本中默认历史OD表名（仅用于传参/回显）

- **位置**：`bin/algorithm/trip/tools/test_dll.py`
- **逻辑**：
  - `sep = "" if prefix.endswith("_") else "_"`
  - `hist_default = f"{role}.{prefix}{sep}trip_distribution_hist"`
  - 在`test_run_all(...)`中作为`hist_od`参数传给`run_trip_model_all`，从而触发Python侧历史OD流程。

## 4. 文档/说明里提到的规则（与代码一致性核对）

- **位置**：`bin/algorithm/trip/DLL单接口-使用说明.md`
- **关键描述**：
  - `hist_od`传空字符串或NULL时：不使用历史OD标定；不会自动尝试默认历史OD表。
  - 当`hist_od`为非空时：若给定表不可读，会按“给定表→默认hist→other_od复制到默认hist”的顺序尝试。

## 5. 小结：trip_distribution_hist表相关“有哪些操作”

- **建表**：
  - Python：当回退到“从other_od复制”时，`CREATE TABLE IF NOT EXISTS <role>.<prefix>trip_distribution_hist (...)`
  - 工具脚本：`generate_test_data.py`显式建表
- **清空**：
  - Python：`DELETE FROM <role>.<prefix>trip_distribution_hist`（复制前）
  - 工具脚本：`DELETE FROM <role>.<prefix>trip_distribution_hist`
- **写入/插入**：
  - Python：从`other_od`整表复制插入
  - 工具脚本：批量插入造数
- **查询/读取**：
  - Python：`SELECT f_id,t_id,demand FROM <role>.<prefix>trip_distribution_hist`用于读取历史OD
  - Python：`SELECT count(*) FROM <table>`用于判定是否有数据（_table_has_rows）
- **用途**：
  - 仅作为出行分布阶段的历史OD标定数据源，用于自动标定$\lambda$；若不可用则退回使用`param_lambda`。
