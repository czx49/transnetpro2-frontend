# 四阶段法-嵌入式 Python 修复指南（tnpapp1203/bin/algorithm/trip）

## 背景
- Qt 客户端在 `Py_Initialize()` 崩溃，或运行四阶段后导致其它算法（如 TNADriver）加载失败。
- 根因包括：客户端 `pyhome/Lib` 缺失、误加载 Anaconda/错误版本的 `python311.dll`、把 Python 的 `sqlite3.dll` 等运行时放进 `bin` 污染进程、以及进程级 DLL 搜索路径未隔离。

## 关键修复点（已实现于本目录 dist_dll/wrapper.cpp）
- 采用 PyConfig 显式初始化：固定 `home` 与 `module_search_paths`（`python311.zip`、`DLLs`、`Lib`、`Lib/site-packages`、`dist_dll`）。
- 仅在调用期间临时注入 DLL 搜索目录：`AddDllDirectory(pyhome)`、`AddDllDirectory(pyhome\\DLLs)`、`AddDllDirectory(dist_dll)`。
- 调用结束后立刻回滚：`RemoveDllDirectory(...)` 三条日志可见，保证其它算法不受影响。
- 仅预加载 Python 自身 DLL：`python311.dll`、`python3.dll`。不预加载 `vcruntime`/`libssl`/`sqlite3` 等，避免与 GDAL/Qt 版本冲突。
- 全量时间戳日志，定位更快。

## 部署步骤
1. 就地编译（当前目录）
   - 运行 `build-dll.bat` 或 `install_deps.bat`。
2. 覆盖 DLL
   - 将 `dist_dll/trip_model.dll` 保持在本目录下使用（Qt 按绝对路径加载）。
3. 清理 `bin` 目录中因排障临时复制的 Python 运行时 DLL（若存在）
   - 不要把以下文件留在 `.../tnpapp1203/bin/`：`python311.dll`、`python3.dll`、`vcruntime140*.dll`、`libssl-3.dll`、`libcrypto-3.dll`、`libffi-8.dll`、`sqlite3.dll`。
4. 确保 `pyhome` 完整
   - `dist_dll/pyhome/python311.zip` 必须存在；建议 `pyhome/Lib` 与 `pyhome/Lib/site-packages` 同步完整。
   - `pyhome/python311._pth` 示例：
     ```
     python311.zip
     .
     .\Lib
     .\Lib\site-packages
     .\DLLs

     import site
     ```

## 验证要点
- 成功初始化的日志关键行：
  - `[ensure_python_initialized] Using PyConfig to initialize...`
  - 多条 `[config] append path: ... (OK)`
  - `[ensure_python_initialized] Py_Initialize SUCCESS`
- 调用收尾三条（必须出现）：
  - `[dll_search] RemoveDllDirectory(pyhome)`
  - `[dll_search] RemoveDllDirectory(pyhome\DLLs)`
  - `[dll_search] RemoveDllDirectory(dist_dll)`
- 典型冲突（已规避）：GDAL 提示 `sqlite3_column_table_name` 缺失 → 原因是把 Python 的 `sqlite3.dll` 放进 `bin` 抢占了 GDAL 期望版本。现方案通过“最小预加载 + 回滚”避免此类覆盖。

## 进度回调规范（已修复）
- 生成阶段（0..50）：C++ 使用 `progress_gen`，将 Python 传入的百分比 `pct` 映射为 `pct*0.5`。
- 分布阶段（50..100）：C++ 使用 `progress_dist`，映射为 `pct*0.5+50`，并在迭代号上承接生成阶段（不重置）。
- 你如果看到“迭代号 0..7 又回到 0..7”的现象，那是旧版回调或 UI 读取逻辑；目前已在 `call_py_model_all` 中：
  - 生成：`create_progress_callable_gen()`；
  - 分布：`create_progress_callable_dist()` 并将 `g_iter_base = g_last_iter`；
  - 这样迭代号单调递增，不会回跳。

## 数据库表名（根据 cfg.json 推导）
- cfg 片段：
  ```json
  {
    "db_conn_str": {
      "role": "user_project",
      "scenario_prefix": "project59_"
    }
  }
  ```
- 生成阶段输出表（写入）：
  - `user_project.project59_trip_production_attraction`
- 分布阶段输出表（写入）：
  - `user_project.project59_other_od`
- TAZ 输入表默认：
  - 生成：`user_project.project59__taz_socioeconomic`（注意：cfg 的 `scenario_prefix` 已带下划线，生成侧拼接的是 `prefix + "_taz_socioeconomic"`，会出现双下划线）
  - 分布：`user_project.project59_taz_socioeconomic`
- 建议：
  - 为避免双下划线，客户端显式传入 TAZ 表名；或将 cfg 的 `scenario_prefix` 去掉尾部下划线，或统一生成侧/分布侧的拼接规则。

## 常见问题与排查
- Qt 启动即崩：检查 `pyhome/python311.zip` 与 `python311._pth`；确认日志是否有 `Using PyConfig...`；确认未误加载 Anaconda 的 `python311.dll`。
- 运行四阶段后其它算法加载失败：检查日志是否出现三条 `RemoveDllDirectory(...)`；确认 `bin` 没有残留 Python 运行时 DLL。
- GDAL 报找不到 `sqlite3_column_table_name`：删除 `bin/sqlite3.dll`（Python 版本），恢复 GDAL/Qt 自带版本。

## 版本说明
- 所有修复均已在本目录 `dist_dll/wrapper.cpp` 落实；编译脚本在本目录 `build-dll.bat` / `install_deps.bat`。按本文步骤就地编译和部署即可。
