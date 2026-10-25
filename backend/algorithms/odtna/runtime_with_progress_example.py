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

#   "db_conn_str": {
#     "connection_string": "",
#     "host": "211.149.178.166",
#     "port": 5432,
#     "dbname": "urban",
#     "user": "postgres",
#     "password": "<CONFIGURE_LOCALLY>",
#     "scenario_prefix": "project63_user1_case41_",
#     "network_table": "user_project.project63_user1_case41_road_way",
#     "od_table": "user_project.project63_user1_case41_other_od",
#     "observed_flow_table": "user_project.project63_user1_case41_other_observation"
#   },

# 当未提供 --config 时使用的内嵌配置（与标准示例保持一致）
EMBEDDED_CONFIG_JSON = """
{
  "db_conn_str": {
    "connection_string": "",
    "host": "211.149.178.166",
    "port": 5432,
    "dbname": "urban",
    "user": "uproject",
    "password": "<CONFIGURE_LOCALLY>",
    "scenario_prefix": "project63_user1_case41",
    "network_table": "user_project.project63_user1_case41_road_way",
    "od_table": "user_project.project63_user1_case41_other_od",
    "observed_flow_table": "user_project.project63_user1_case41_other_observation"
  },
  "algorithm_params": {
    "use_external_observed_flow": true,
    "use_roadway_observed": false,
    "observed_flow_scale": 1,
    "format_precision": 18,
    "format_scale": 6,
    "gamma": 0.5,
    "gamma1": 0.5,
    "gamma2": 0.5,
    "oblink_ratio": 0.3,
    "mainloop_maxiter": 5,
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
    parser = argparse.ArgumentParser(description="使用 TNADriver.dll 的进度回调示例。")
    parser.add_argument("--dll", dest="dll_path", type=Path, default=None, help="指定 TNADriver.dll 的路径，未提供时自动搜索脚本目录与 runtime_dll/")
    parser.add_argument("--config", dest="config_path", type=Path, default=None, help="指定 JSON 配置文件路径，未提供时使用脚本内嵌配置。")
    parser.add_argument("--network-table", dest="network_table", type=str, default=None, help="覆盖网络表名。")
    parser.add_argument("--od-table", dest="od_table", type=str, default=None, help="覆盖 OD 表名。")
    parser.add_argument("--observed-table", dest="observed_table", type=str, default=None, help="覆盖观测流量表名。")
    parser.add_argument("--extra-dll-dir", dest="extra_dll_dirs", action="append", type=Path, default=[], help="额外的 DLL 搜索目录。")
    parser.add_argument("--use-capacity-business", dest="use_capacity_business", action="store_true", help="启用承载力业务：从 roadway 表读取分配结果作为观测流量")
    parser.add_argument("--use-roadway-observed", dest="use_roadway_observed", action="store_true", help="在 JSON 中开启 use_roadway_observed（通常接口参数已可覆盖）")
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

    # 覆盖 algorithm_params 的逻辑移动到读取 config_text 之后

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
    # 根据命令行覆盖/补全 algorithm_params 中的承载力业务参数
    try:
        cfg_obj2 = json.loads(config_text) if config_text else {}
    except Exception:
        cfg_obj2 = {}
    alg = cfg_obj2.get("algorithm_params") or {}
    # 接口参数已可强制启用，此处在 JSON 中同步开启仅为可见性
    if args.use_roadway_observed or args.use_capacity_business:
        alg["use_roadway_observed"] = True
    if isinstance(args.observed_flow_scale, float) and args.observed_flow_scale and args.observed_flow_scale > 0:
        alg["observed_flow_scale"] = float(args.observed_flow_scale)
    # 补默认值
    if "use_roadway_observed" not in alg:
        alg["use_roadway_observed"] = False
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

    # 动态库加载（支持 --dll 与 --extra-dll-dir）
    dll = None
    loaded_path = None
    handles = []
    if dll_path is not None:
        add_dir = getattr(os, "add_dll_directory", None)
        try:
            if add_dir is not None:
                # 先加入 DLL 所在目录，再加入额外目录
                dirs = [dll_path.parent] + extra_dirs
                for d in dirs:
                    if d and d.exists():
                        handles.append(add_dir(str(d)))
            dll = ctypes.CDLL(str(dll_path))
            loaded_path = str(dll_path)
        except Exception as e:
            print(f"Failed to load DLL: {dll_path}\n{e}", file=sys.stderr)
            # 回退到自动搜索
    if dll is None:
        dll, loaded_path = load_tnadriver()
    print("Loaded DLL:", loaded_path)

    # 进度回调：(int iter, double value)，此处 value 表示 z 值（整数）
    ProgressCB = CFUNCTYPE(None, c_int, c_double)

    def on_progress(iteration, z_value):
        # Print progress percentage and current iteration
        progress = int(z_value)
        # if max_main_iter is not None:
        #     sys.stdout.write("\rprogress: {}% (iter={}/{})".format(progress, iteration, max_main_iter))
        # else:
        #     sys.stdout.write("\rprogress: {}% (iter={})".format(progress, iteration))
        sys.stdout.flush()

    progress_cb = ProgressCB(on_progress)  # 必须持有引用避免被 GC

    # 声明导出函数
    has_with_progress = hasattr(dll, "TestOD_Estimate_WithProgress")
    if not has_with_progress:
        print("\nWarning: DLL does not contain TestOD_Estimate_WithProgress. Falling back to TestOD_Estimate (no progress callback)")

    if has_with_progress:
        dll.TestOD_Estimate_WithProgress.argtypes = [ProgressCB, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
        dll.TestOD_Estimate_WithProgress.restype  = ctypes.c_void_p
    else:
        dll.TestOD_Estimate.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        dll.TestOD_Estimate.restype  = ctypes.c_void_p

    # 参数：与标准示例保持一致（未提供 --config 则传内嵌 JSON）
    json_config = config_bytes
    network_bytes = network_table.encode('utf-8') if network_table else None
    od_bytes = od_table.encode('utf-8') if od_table else None
    observed_bytes = observed_table.encode('utf-8') if observed_table else None

    print("\nAbout to call DLL with parameters:")
    print(f"DLL path: {loaded_path}")
    if extra_dirs:
        print("Extra DLL directories:")
        for d in extra_dirs:
            print(f"  - {d}")
    else:
        print("Extra DLL directories: (none)")
    print(f"Config source: {config_source}")
    print(f"Config file path: {args.config_path if args.config_path else '(embedded default)'}")
    if config_text:
        preview = config_text[:200].replace("\n", " ")
        print(f"Config text length: {len(config_text)}, preview: {preview}")
    else:
        print("Config text: (empty)")
    print(f"Planned max iterations: {max_main_iter if max_main_iter is not None else '(unknown)'}")
    print(f"Network table: {network_table if network_table else '(not provided)'}")
    print(f"OD table: {od_table if od_table else '(not provided)'}")
    print(f"Observed flow table: {observed_table if observed_table else '(not provided)'}")

    print("\nStart calling algorithm...")
    if has_with_progress:
        ret_ptr = dll.TestOD_Estimate_WithProgress(progress_cb, json_config, network_bytes, od_bytes, observed_bytes, 1 if args.use_capacity_business else 0)
    else:
        ret_ptr = dll.TestOD_Estimate(json_config, network_bytes, od_bytes, observed_bytes)

    # 换行收尾进度条
    print("\nAlgorithm finished. Parsing response...")

    if not ret_ptr:
        print("No return data (null pointer)")
        return 1
    result_text = ctypes.string_at(ret_ptr).decode("utf-8", errors="replace")

    # 保存完整结构化 JSON 到 DLL/脚本目录（更符合部署场景）
    try:
        out_name = "od_estimate_result_{}".format(datetime.now().strftime('%Y%m%d_%H%M%S')) + ".json"
        try:
            base_dir = Path(loaded_path).resolve().parent if loaded_path else Path(__file__).resolve().parent
        except Exception:
            base_dir = Path(__file__).resolve().parent
        out_path = base_dir / out_name
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

    # 将最后一条进度行断行
    try:
        sys.stdout.write("\n")
    except Exception:
        pass

    if isinstance(data, dict):
        summary = data.get("summary", {})
        params = data.get("params", {})
        tables = data.get("tables", {})
        history = data.get("history_tail", {})
        reports = data.get("reports", {})

        def get_value_by_en_key(d: dict, en_key: str):
            try:
                for k, v in d.items():
                    base = k.split('|', 1)[0].strip()
                    if base == en_key:
                        return v
            except Exception:
                pass
            return None

        # 完整打印 summary 与 params
        print("summary:")
        print(json.dumps(summary, ensure_ascii=False, indent=2))
        print("params:")
        print(json.dumps(params, ensure_ascii=False, indent=2))

        # 从 summary 取关键统计
        try:
            ic = get_value_by_en_key(summary, "iteration_count")
            if ic is None:
                ic = summary.get("iteration_count")
            mi = get_value_by_en_key(summary, "max_main_iter")
            if mi is None:
                mi = get_value_by_en_key(params, "max_main_iter")
            if mi is None:
                mi = params.get("max_main_iter")
            if ic is not None or mi is not None:
                print(f"Iteration: {ic}/{mi}")
        except Exception:
            pass

        # 其他部分仅打印前几项
        print("tables (preview):")
        try:
            items = list(tables.items())
            preview_dict = dict(items[:3])
            print(json.dumps(preview_dict, ensure_ascii=False, indent=2))
        except Exception:
            print(json.dumps(tables, ensure_ascii=False, indent=2))

        print("history_tail (preview):")
        try:
            preview_hist = {}
            for k, arr in history.items():
                if isinstance(arr, list):
                    preview_hist[k] = arr[:5]
                else:
                    preview_hist[k] = arr
            print(json.dumps(preview_hist, ensure_ascii=False, indent=2))
        except Exception:
            print(json.dumps(history, ensure_ascii=False, indent=2))

        # 将 CSV 落盘，并打印前几行预览
        out_dir = "report_local_analysis"
        try:
            os.makedirs(out_dir, exist_ok=True)
        except TypeError:
            # 兼容旧 Python
            if not os.path.isdir(out_dir):
                os.makedirs(out_dir)

        csv1 = reports.get("top20_link_flow_csv") or ""
        if csv1:
            path1 = os.path.join(out_dir, "parsed_top20_link_flow.csv")
            with open(path1, "w", encoding="utf-8") as f:
                f.write(csv1)
            print("top20_link_flow.csv 预览:")
            print("\n".join(csv1.splitlines()[:5]))

        csv2 = reports.get("top_vc_ratio_links_csv") or ""
        if csv2:
            path2 = os.path.join(out_dir, "parsed_top_vc_ratio_links.csv")
            with open(path2, "w", encoding="utf-8") as f:
                f.write(csv2)
            print("top_vc_ratio_links.csv 预览:")
            print("\n".join(csv2.splitlines()[:5]))
    else:
        print("raw data:")
        print(str(data))

if __name__ == "__main__":
    sys.exit(main())
