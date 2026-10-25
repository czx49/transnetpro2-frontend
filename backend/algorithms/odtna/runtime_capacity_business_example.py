# -*- coding: utf-8 -*-
import os
import sys
import json
import argparse
import ctypes
from pathlib import Path
from ctypes import cdll, CFUNCTYPE, c_int, c_double, c_char_p
from datetime import datetime

def load_tnadriver():
    # 优先从脚本目录查找 TNADriver.dll；若存在 runtime_dll 子目录则也尝试
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(script_dir, "TNADriver.dll"),
        os.path.join(script_dir, "runtime_dll", "TNADriver.dll"),
    ]
    last_err = None
    for p in candidates:
        if os.path.isfile(p):
            try:
                return cdll.LoadLibrary(p), p
            except Exception as e:
                last_err = e
    raise RuntimeError("无法加载 TNADriver.dll，尝试路径：{}\n最后错误: {}".format(candidates, last_err))

# 当未提供 --config 时使用的内嵌配置
EMBEDDED_CONFIG_JSON = """
{
  "db_conn_str": {
    "connection_string": "",
    "host": "211.149.178.166",
    "port": 5432,
    "dbname": "urban",
    "user": "uproject",
    "password": "<CONFIGURE_LOCALLY>",
    "scenario_prefix": "project62_user1_",
    "scenario_id": 0,
    "network_table": "user_project.project62_user1_road_way",
    "od_table": "user_project.project62_user1_case37_other_od",
    "observed_flow_table": "user_project.project62_user1_other_observation"
  },
  "algorithm_params": {
    "use_external_observed_flow": false,
    "use_roadway_observed": true,
    "observed_flow_scale": 1,
    "format_precision": 18,
    "format_scale": 6,
    "gamma": 0.5,
    "gamma1": 0.5,
    "gamma2": 0.5,
    "oblink_ratio": 0.3,
    "mainloop_maxiter": 300,
    "mainloop_conv": 0.00001,
    "armijo_maxiter": 5,
    "armijo_stopcriterion": 100,
    "armijo_coefficient": 10.0,
    "armijo_alphamax": 1000.0,
    "conv": 1e-6,
    "max_iter": 500,
    "lpf": "BPRLK",
    "cost_scalar": 60,
    "cost_coef_time": 1.0,
    "cost_coef_distance": 0.0
  },
  "report_iter_history": true,
  "report_link_detail": true,
  "report_path_detail": true,
  "report_demand_detail": true,
  "report_linkinfo_detail": true,
  "report_upper_objective": true
}
"""

def read_config_text(path: Path) -> str:
    text = path.read_text(encoding="utf-8")
    return text if text.strip() else ""

def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="使用 TNADriver.dll 的承载力分析业务接口示例 (use_capacity_business=True)。")
    parser.add_argument("--dll", dest="dll_path", type=Path, default=None, help="指定 TNADriver.dll 的路径，未提供时自动搜索脚本目录与 runtime_dll/")
    parser.add_argument("--config", dest="config_path", type=Path, default=None, help="指定 JSON 配置文件路径，未提供时使用脚本内嵌配置。")
    parser.add_argument("--network-table", dest="network_table", type=str, default=None, help="覆盖网络表名。")
    parser.add_argument("--od-table", dest="od_table", type=str, default=None, help="覆盖 OD 表名。")
    parser.add_argument("--observed-table", dest="observed_table", type=str, default=None, help="覆盖观测流量表名。")
    parser.add_argument("--extra-dll-dir", dest="extra_dll_dirs", action="append", type=Path, default=[], help="额外的 DLL 搜索目录。")
    # 移除 --use-capacity-business 参数，因为本脚本强制启用
    parser.add_argument("--observed-flow-scale", dest="observed_flow_scale", type=float, default=None, help="设置 algorithm_params.observed_flow_scale（>0）")
    return parser

def main(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)

    # 统一输出编码为 UTF-8
    os.environ['PYTHONIOENCODING'] = 'utf-8'
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    if hasattr(sys.stderr, 'reconfigure'):
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    try:
        ctypes.windll.kernel32.SetConsoleOutputCP(65001)
        ctypes.windll.kernel32.SetConsoleCP(65001)
    except Exception:
        pass

    # 解析参数
    dll_path: Path = args.dll_path if isinstance(args.dll_path, Path) else None
    network_table = args.network_table.strip() if isinstance(args.network_table, str) and args.network_table.strip() else None
    od_table = args.od_table.strip() if isinstance(args.od_table, str) and args.od_table.strip() else None
    observed_table = args.observed_table.strip() if isinstance(args.observed_table, str) and args.observed_table.strip() else None
    extra_dirs = [d for d in args.extra_dll_dirs if isinstance(d, Path)]

    # 准备配置文本
    if isinstance(args.config_path, Path):
        if not args.config_path.exists():
            print(f"未找到配置文件: {args.config_path}", file=sys.stderr)
            return 1
        config_text = read_config_text(args.config_path)
        config_source = "文件"
    else:
        config_text = EMBEDDED_CONFIG_JSON
        config_source = "内嵌"

    # 根据命令行覆盖/补全 algorithm_params
    try:
        cfg_obj2 = json.loads(config_text) if config_text else {}
    except Exception:
        cfg_obj2 = {}
    alg = cfg_obj2.get("algorithm_params") or {}

    # 强制启用 use_roadway_observed (承载力分析业务要求)
    alg["use_roadway_observed"] = True
    # 强制关闭 external observed flow (承载力分析业务要求)
    alg["use_external_observed_flow"] = False

    if isinstance(args.observed_flow_scale, float) and args.observed_flow_scale and args.observed_flow_scale > 0:
        alg["observed_flow_scale"] = float(args.observed_flow_scale)
    if "observed_flow_scale" not in alg:
        alg["observed_flow_scale"] = 1.0

    cfg_obj2["algorithm_params"] = alg
    config_text = json.dumps(cfg_obj2, ensure_ascii=False, indent=2)
    config_bytes = config_text.encode('utf-8') if config_text else None

    # 解析计划最大迭代次数（来自配置）
    max_main_iter = None
    try:
        cfg_obj = json.loads(config_text) if config_text else {}
        v = cfg_obj.get("algorithm_params", {}).get("mainloop_maxiter")
        if isinstance(v, int) and v > 0:
            max_main_iter = v
        else:
            # 兼容字符串数字
            if isinstance(v, str) and v.strip().isdigit():
                max_main_iter = int(v.strip())
    except Exception:
        pass

    # 动态库加载
    dll = None
    loaded_path = None
    handles = []
    if dll_path is not None:
        add_dir = getattr(os, "add_dll_directory", None)
        try:
            if add_dir is not None:
                dirs = [dll_path.parent] + extra_dirs
                for d in dirs:
                    if d and d.exists():
                        handles.append(add_dir(str(d)))
            dll = ctypes.CDLL(str(dll_path))
            loaded_path = str(dll_path)
        except Exception as e:
            print(f"Failed to load DLL: {dll_path}\n{e}", file=sys.stderr)
    if dll is None:
        dll, loaded_path = load_tnadriver()
    print("Loaded DLL:", loaded_path)

    # 进度回调
    ProgressCB = CFUNCTYPE(None, c_int, c_double)
    def on_progress(iteration, z_value):
        # 进度展示
        progress = int(z_value)
        sys.stdout.flush()
    progress_cb = ProgressCB(on_progress)

    # 声明导出函数
    has_with_progress = hasattr(dll, "TestOD_Estimate_WithProgress")
    if not has_with_progress:
        print("\nError: DLL does not contain TestOD_Estimate_WithProgress. Capacity business requires this interface.", file=sys.stderr)
        return 1

    if has_with_progress:
        dll.TestOD_Estimate_WithProgress.argtypes = [ProgressCB, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
        dll.TestOD_Estimate_WithProgress.restype  = ctypes.c_void_p

    # 参数
    json_config = config_bytes
    network_bytes = network_table.encode('utf-8') if network_table else None
    od_bytes = od_table.encode('utf-8') if od_table else None
    observed_bytes = observed_table.encode('utf-8') if observed_table else None

    print("\nAbout to call DLL (Capacity Analysis Business)...")
    print(f"DLL path: {loaded_path}")
    print(f"Config source: {config_source}")
    print(f"Config file path: {args.config_path if args.config_path else '(embedded default)'}")
    print(f"Network table: {network_table if network_table else '(not provided)'}")
    print(f"OD table: {od_table if od_table else '(not provided)'}")
    print(f"Observed flow table: {observed_table if observed_table else '(not provided)'}")
    print("use_capacity_business: True (Forced)")

    print("\nStart calling algorithm...")
    # 最后一个参数传 1，启用承载力业务
    ret_ptr = dll.TestOD_Estimate_WithProgress(progress_cb, json_config, network_bytes, od_bytes, observed_bytes, 1)

    print("\nAlgorithm finished. Parsing response...")
    if not ret_ptr:
        print("No return data (null pointer)")
        return 1
    result_text = ctypes.string_at(ret_ptr).decode("utf-8", errors="replace")

    # 保存完整结构化 JSON 到当前目录
    try:
        out_name = "od_capacity_result_{}".format(datetime.now().strftime('%Y%m%d_%H%M%S')) + ".json"
        out_path = Path.cwd() / out_name
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(result_text)
        print("saved json:", str(out_path))
    except Exception as exc:
        print("保存JSON失败:", exc, file=sys.stderr)

    try:
        resp = json.loads(result_text)
    except Exception:
        print("Return is not valid JSON. Raw text:")
        print(result_text)
        return 1

    status = resp.get("status")
    message = resp.get("message")
    data = resp.get("data", {})

    print("status:", status)
    print("message:", message)

    if status != 1:
        print("Run failed. Raw data:")
        print(data if isinstance(data, str) else json.dumps(data, ensure_ascii=False, indent=2))
        return 1

    # 简单打印 Summary
    if isinstance(data, dict):
        summary = data.get("summary", {})
        print("summary:")
        print(json.dumps(summary, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    sys.exit(main())
