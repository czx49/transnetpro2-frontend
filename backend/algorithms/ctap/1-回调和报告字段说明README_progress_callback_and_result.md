# 迭代进度回调与返回结果字段说明

本文档说明 CTAP 动态库的迭代进度回调接口用法，以及返回 JSON 中 `data.summary` 与 `data.params` 字段的含义与示例。

## 概览

- **导出函数（带回调）**
  - `const char* TestCTAP_GREEDY_ALM_II_PG_CB(TNADriver_ProgressCallback progress, const wchar_t* json_params, const wchar_t* network_table, const wchar_t* od_table)`
- **回调原型**
  - `typedef void (*TNADriver_ProgressCallback)(int percent);`
- **返回结构（成功）**
  - `{"status":1, "message":"Success", "data":{...}}`
  - 其中 `data` 下包含：`summary`、`params`、以及两个报表数组：`saturation_histogram`、`top20_congested_links`。

---

## 进度回调接口

- **[签名]**
  - C/C++：

    ```cpp
    extern "C" __declspec(dllexport)
    const char* TestCTAP_GREEDY_ALM_II_PG_CB(
        TNADriver_ProgressCallback progress,
        const wchar_t* json_params,
        const wchar_t* network_table,
        const wchar_t* od_table);
    ```

  - 回调类型：

    ```cpp
    typedef void (*TNADriver_ProgressCallback)(int percent);
    // percent 取值范围：0~100，表示当前阶段进度百分比
    ```
- **[触发时机（典型）]**
  - 网络构建、增强网络生成、主/内层迭代、报表导出等阶段会多次调用。
  - `percent` 为 0~100 的整数；不同阶段累计更新，非严格线性。
- **[Python 使用示例]**（简化自 bin/x64/test_ctap_new_ret_cb.py）

  ```python
  import ctypes
  CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_int)

  def on_progress(percent: int):
      print(f"[CALLBACK] progress={percent}%")

  dll = ctypes.CDLL('CTAPDriver.dll')
  dll.TestCTAP_GREEDY_ALM_II_PG_CB.argtypes = [CALLBACK, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p]
  dll.TestCTAP_GREEDY_ALM_II_PG_CB.restype = ctypes.c_char_p

  cb = CALLBACK(on_progress)
  ret_ptr = dll.TestCTAP_GREEDY_ALM_II_PG_CB(cb, json_params_str, network_table, od_table)
  ret = ret_ptr.decode('utf-8') if ret_ptr else ''
  ```

- **[注意]**
  - Windows 平台下，若依赖 `libpq.dll` 等，请将其路径加入 DLL 搜索路径（示例脚本已演示 `os.add_dll_directory`）。

---

## data.params 字段（中英双键）

调用成功后，返回 JSON 的 `data.params` 回显本次关键输入。为便于中英文场景下使用，提供双键：

- **scenario_prefix / 场景前缀**
  - 说明：生成本地报表文件与数据库表名前缀；如 `project33_user1_`。
- **network_table / 网络表**
  - 说明：数据库中的网络表全名（含 schema/role），例如 `user_project.project33_user1_road_way`。
- **od_table / OD表**
  - 说明：数据库中的 OD 表全名，例如 `user_project.project33_user1_other_od`。

示例：

```json
{
  "params": {
    "scenario_prefix": "project33_user1_",
    "场景前缀": "project33_user1_",
    "network_table": "user_project.project33_user1_road_way",
    "网络表": "user_project.project33_user1_road_way",
    "od_table": "user_project.project33_user1_other_od",
    "OD表": "user_project.project33_user1_other_od"
  }
}
```

---

## data.summary 字段（中英双键）

算法运行后的关键指标摘要，含中英文键，便于直观展示与程序消费：

- **iterations / 迭代次数**
  - 当前求解共进行的迭代步数。
- **gap / 间隙**
  - 相对间隙指标（收敛度量），越接近 0 越好。
- **ofv / 目标值**
  - 目标函数值（Objective Function Value）。
- **termination_status / 终止状态**
  - 终止原因代码（例如达到迭代上限、满足收敛阈值等）。
- **convergence_indicator / 收敛指标**
  - 内部收敛指示量，用于步长与循环控制。
- **d_value / 需求放大系数**
  - 需求放大系数 `d` 的取值（网络承载力刻画指标）。
- **avg_cost / 即时成本**
  - 全网链路即时成本（`link->buffer[0]`）的均值。
- **avg_gcost / 广义成本**
  - 全网链路广义成本（`link->cost`）的均值。
- **dlink_first_msg**
  - 首次在虚拟链路（dlink）上出现正流的提示文本，包含触发的 `d` 与对应 OD。
- **dlink_last_msg**
  - 最近一次发现 dlink 上存在流量的提示文本。

示例：

```json
{
  "summary": {
    "iterations": 87,
    "迭代次数": 87,
    "gap": 0.0000012,
    "间隙": 0.0000012,
    "ofv": 12345.6789,
    "目标值": 12345.6789,
    "termination_status": 0,
    "终止状态": 0,
    "convergence_indicator": 0.00045,
    "收敛指标": 0.00045,
    "d_value": 2.7,
    "需求放大系数": 2.7,
    "avg_cost": 15.2345,
    "即时成本": 15.2345,
    "avg_gcost": 18.9012,
    "广义成本": 18.9012,
    "dlink_first_msg": "volume appeared on dlink: 1001 firstly, under 2.4 times initial demand, between od-pair: 12 and 45",
    "dlink_last_msg": "volume appeared on dlink: 1001, under 2.7 times initial demand, between od-pair: 12 and 45"
  }
}
```

---

## 完整返回示例（节选）

```json
{
  "status": 1,
  "message": "Success",
  "data": {
    "summary": { /* 见上 */ },
    "params": { /* 见上 */ },
    "saturation_histogram": [ /* 直方图数组 */ ],
    "top20_congested_links": [ /* Top-20 拥堵路段数组 */ ]
  }
}
```

---

## 常见问题

- **[回调不触发]**
  - 请确认使用了 `..._PG_CB` 的导出函数并正确传入回调；检查依赖 DLL 搜索路径。
- **[字段为空]**
  - 当本地报表文件尚未生成或为空时，`saturation_histogram/top20_congested_links` 可能为空数组；`summary` 由 DLL 构造，不依赖本地报表文件。
- **[dlink 信息缺失]**
  - 若本次运行过程中未在虚拟链路上出现流量，则 `dlink_first_msg/dlink_last_msg` 可能为空字符串。
