# -*- coding: utf-8 -*-
import os
import sys
import json
import ctypes
import platform
import locale
from ctypes import c_wchar_p

# Resolve DLL path (override via CTAP_DLL env if needed)
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DLL_PATH = os.environ.get('CTAP_DLL', os.path.join(THIS_DIR, 'CTAPDriver.dll'))

# 与 1-test_ctap_new_ret.py 保持一致的参数
ROLE = "user_project"
DLL_NAME = 'CTAPDriver.dll'
NETWORK_TABLE = ROLE + ".project33_user1_road_way"
OD_TABLE = ROLE + ".project33_user1_other_od"

CONFIG = {
    "db_conn_str": {
        "host": "211.149.178.166",
        "port": 5432,
        "dbname": "urban",
        "user": "postgres",
        "password": "<CONFIGURE_LOCALLY>",
        # "user": "uproject",
        # "password": "<CONFIGURE_LOCALLY>",
        # "role": "user_project",
        "scenario_prefix": "project33_user1_",
    },
    # "db_conn_str": {
    #     "host": "127.0.0.1",
    #     "port": 54321,
    #     "dbname": "test2",
    #     "user": "postgres",
    #     "password": "<CONFIGURE_LOCALLY>",
    #     # "scenario_prefix": "project33_user1_",
    # },
    "algorithm_params": {
        "ly_MaxFlowInfeasibility": 1e-7,
        "ly_MaxMainConv": 1e-6,
        "ly_MaxSubConv": 1e-6,
        "ly_MaxSubIter": 5,
        "max_iter": 500,
        "cost_scalar": 60.0,
    }
}

# Callback: void progress(int percent)
CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_int)

def on_progress(percent: int):
    try:
        print(f"[CALLBACK] progress={percent}%")
    except Exception:
        # keep it resilient
        pass


def add_search_paths():
    """为 DLL 及其依赖（如 libpq.dll）添加搜索路径"""
    this_dir = THIS_DIR
    pg_bin_path = r"D:\\postgresql\\16\\bin"
    paths = [this_dir]
    if os.path.isdir(pg_bin_path):
        paths.append(pg_bin_path)
    else:
        print(f"[警告] PostgreSQL bin 目录未找到: {pg_bin_path}")
    try:
        for p in paths:
            if os.path.isdir(p):
                os.add_dll_directory(p)
                print(f"Added DLL search path: {p}")
    except AttributeError:
        os.environ['PATH'] = os.pathsep.join(paths) + os.pathsep + os.environ.get('PATH', '')


def main():
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass
    print('=== CTAP JSON 标准返回结构测试脚本（带回调）===')
    print(f'Python: {sys.version.splitlines()[0]}')
    print(f'OS: {platform.platform()}')
    print(f'Arch: {platform.architecture()}')

    json_str = json.dumps(CONFIG, ensure_ascii=False)
    add_search_paths()

    dll_path = os.path.join(THIS_DIR, DLL_NAME)
    if not os.path.exists(dll_path):
        print(f"[错误] DLL 文件未找到: {dll_path}")
        return 1
    print(f"[INFO] Loading DLL: {dll_path}")
    dll = ctypes.CDLL(dll_path)

    # Signature: const char* TestCTAP_GREEDY_ALM_II_PG_CB(TNADriver_ProgressCallback, const wchar_t*, const wchar_t*, const wchar_t*)
    dll.TestCTAP_GREEDY_ALM_II_PG_CB.argtypes = [CALLBACK, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p]
    dll.TestCTAP_GREEDY_ALM_II_PG_CB.restype = ctypes.c_char_p

    # JSON 参数：与 1-test_ctap_new_ret.py 保持一致
    json_params = json_str

    network_table = NETWORK_TABLE
    od_table = OD_TABLE

    cb = CALLBACK(on_progress)
    print("[INFO] Starting algorithm with callback...")
    ret_ptr = dll.TestCTAP_GREEDY_ALM_II_PG_CB(
        cb,
        json_params,
        network_table,
        od_table
    )

    ret = ret_ptr.decode('utf-8', errors='ignore') if ret_ptr else ''
    # 解析 JSON
    try:
        result_json = json.loads(ret)
    except Exception as e:
        print(f"[错误] 返回不是有效的 JSON: {e}")
        print("[RAW]", ret[:500])
        return 1

    # 保存结果 JSON 到 bin/x64 目录，命名与 1-test 脚本一致
    scenario_id = str(CONFIG.get('db_conn_str', {}).get('scenario_prefix', '') or '').rstrip('_')
    base_name = f"ctap_result_{scenario_id}.json" if scenario_id else "ctap_result.json"
    out_path = os.path.join(THIS_DIR, base_name)
    try:
        with open(out_path, 'w', encoding='utf-8') as f:
            json.dump(result_json, f, ensure_ascii=False, indent=2)
        print(f"Saved result JSON to: {out_path}")
    except Exception as e:
        print(f"[警告] 结果保存失败: {e}")

    print('\n--- DLL 返回结果（节选）---')
    data_obj = result_json.get('data', {}) if isinstance(result_json, dict) else {}

    # 打印核心指标（从 data.summary 提取）
    core_keys = [
        'iterations', 'gap', 'ofv', 'termination_status', 'convergence_indicator', 'd_value'
    ]
    if isinstance(data_obj, dict):
        summary_for_core = data_obj.get('summary', {})
        core = {k: summary_for_core.get(k) for k in core_keys if k in summary_for_core}
        if core:
            print("【核心指标】")
            print(json.dumps(core, ensure_ascii=False, indent=2))

    # 打印 summary（从 data.summary 读取）
    summary_obj = data_obj.get('summary')
    if summary_obj is not None:
        print("\n【summary】")
        print(json.dumps(summary_obj, ensure_ascii=False, indent=2))
    else:
        print("\n【summary】未返回或为空")

    # 打印 params（完全输出）：沿用输入配置
    print("\n【params - algorithm_params】")
    print(json.dumps(CONFIG.get('algorithm_params', {}), ensure_ascii=False, indent=2))
    print("\n【params - db_conn_str】")
    print(json.dumps(CONFIG.get('db_conn_str', {}), ensure_ascii=False, indent=2))

    # 其它项的前几项（各取前 5 条）
    def print_first_n(label, arr, n=5):
        print(f"\n【{label} - 前{n}条】")
        if isinstance(arr, list) and arr:
            for i, item in enumerate(arr[:n]):
                print(f"  [{i}] " + json.dumps(item, ensure_ascii=False))
        else:
            print("  (空)")

    print_first_n('saturation_histogram', data_obj.get('saturation_histogram', []), 5)
    print_first_n('top20_congested_links', data_obj.get('top20_congested_links', []), 5)

    # 退出码与提示
    status_code = result_json.get('status', 0)
    if status_code == 1:
        print("\n算法执行成功。")
        return 0
    else:
        print(f"\n算法执行失败，状态码: {status_code}")
        return 1


if __name__ == '__main__':
    sys.exit(main())
