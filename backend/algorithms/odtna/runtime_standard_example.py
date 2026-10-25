#!/usr/bin/env python3
# -*- coding: utf-8 -*-way od
"""标准示例：调用 runtime/TNADriver.dll 并输出执行结果。"""

import argparse
import ctypes
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Sequence
import os, sys
from datetime import datetime
# # 1) 统一 Python 标准流为 UTF-8
# os.environ['PYTHONIOENCODING'] = 'utf-8'
# if hasattr(sys.stdout, 'reconfigure'):
#     sys.stdout.reconfigure(encoding='utf-8', errors='replace')
# if hasattr(sys.stderr, 'reconfigure'):
#     sys.stderr.reconfigure(encoding='utf-8', errors='replace')
# if hasattr(sys.stdin, 'reconfigure'):
#     try:
#         sys.stdin.reconfigure(encoding='utf-8', errors='replace')
#     except Exception:
#         pass

# # 2) Windows 控制台切到 UTF-8
# try:
#     import ctypes
#     ctypes.windll.kernel32.SetConsoleOutputCP(65001)
#     ctypes.windll.kernel32.SetConsoleCP(65001)
# except Exception:
#     pass

# # 3) 兜底：切换当前会话代码页（对某些老终端更稳）
# try:
#     os.system('chcp 65001 >NUL')
# except Exception:
#     pass

# # 4) 可选：打印当前编码，便于自检
# print('[DEBUG] sys.stdout.encoding =', getattr(sys.stdout, 'encoding', None))
# print('[DEBUG] sys.stderr.encoding =', getattr(sys.stderr, 'encoding', None))

@dataclass
class RuntimeOptions:
    """封装运行所需的参数。"""

    dll_path: Path
    config_path: Optional[Path]
    network_table: Optional[str]
    od_table: Optional[str]
    observed_table: Optional[str]
    extra_dll_dirs: List[Path]


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
RUNTIME_DIR = (SCRIPT_DIR / "runtime").resolve()
DEFAULT_DLL_PATH = (SCRIPT_DIR / "TNADriver.dll")
# 优先脚本同级的 alg.json，不存在则回退到 runtime/alg.json
PREFERRED_CONFIG_PATH = SCRIPT_DIR / "alg.json"
FALLBACK_CONFIG_PATH = RUNTIME_DIR / "alg.json"
import os, sys
os.environ['PYTHONIOENCODING'] = 'utf-8'
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')
try:
    import ctypes
    ctypes.windll.kernel32.SetConsoleOutputCP(65001)
    ctypes.windll.kernel32.SetConsoleCP(65001)
except Exception:
    pass
# 内嵌的算法配置（若未显式提供 --config，则默认使用此配置）
    # "role": "user_project",

EMBEDDED_CONFIG_JSON = """
{
  "db_conn_str": {
    "connection_string": "",
    "host": "211.149.178.166",
    "port": 5432,
    "dbname": "urban",
    "user": "postgres",
    "password": "<CONFIGURE_LOCALLY>",
    "scenario_prefix": "project33_user1_",
    "network_table": "user_project.project33_user1_road_way",
    "od_table": "user_project.project33_user1_other_od",
    "observed_flow_table": "user_project.project33_user1_other_observation"
  },
  "algorithm_params": {
    "use_external_observed_flow": true,
    "format_precision": 18,
    "format_scale": 6,
    "gamma": 0.5,
    "gamma1": 0.5,
    "gamma2": 0.5,
    "oblink_ratio": 0.3,
    "mainloop_maxiter": 300,
    "mainloop_conv": 0.0001,
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


class DllContext:
    """管理 DLL 搜索路径并加载 TNADriver。"""

    def __init__(self, dll_path: Path, extra_dirs: Sequence[Path]):
        self.dll_path = dll_path
        self.extra_dirs = [dll_path.parent, *extra_dirs]
        self._handles: List[object] = []
        self._dll: Optional[ctypes.CDLL] = None

    def __enter__(self) -> ctypes.CDLL:
        add_dir = getattr(os, "add_dll_directory", None)
        if add_dir is not None:
            for directory in self.extra_dirs:
                if directory and directory.is_dir():
                    handle = add_dir(str(directory))
                    self._handles.append(handle)
        loader = ctypes.CDLL
        self._dll = loader(str(self.dll_path))
        return self._dll

    def __exit__(self, exc_type, exc, tb) -> None:
        for handle in reversed(self._handles):
            try:
                handle.close()  # type: ignore[attr-defined]
            except Exception:
                pass
        self._handles.clear()
        self._dll = None


def read_config_text(path: Optional[Path]) -> Optional[str]:
    """读取 JSON 配置文本。"""

    if path is None:
        return None
    text = path.read_text(encoding="utf-8")
    return text if text.strip() else None


def call_test_estimate(options: RuntimeOptions) -> int:
    """调用 DLL 并根据状态码返回。"""

    # 配置来源：若未显式提供 --config，默认使用内嵌 JSON
    if options.config_path is not None:
        config_text = read_config_text(options.config_path)
        config_source = "文件"
    else:
        config_text = EMBEDDED_CONFIG_JSON
        config_source = "内嵌"
    config_bytes = config_text.encode("utf-8") if config_text else None
    network_bytes = options.network_table.encode("utf-8") if options.network_table else None
    od_bytes = options.od_table.encode("utf-8") if options.od_table else None
    observed_bytes = options.observed_table.encode("utf-8") if options.observed_table else None

    # 执行前打印将要传入 DLL 的参数
    print("即将调用 DLL，参数如下：")
    print(f"DLL 路径: {options.dll_path}")
    if options.extra_dll_dirs:
        print("额外 DLL 目录:")
        for d in options.extra_dll_dirs:
            print(f"  - {d}")
    else:
        print("额外 DLL 目录: (无)")
    print(f"配置来源: {config_source}")
    print(f"配置文件路径: {options.config_path if options.config_path else '(未提供，使用内嵌)'}")
    if config_text:
        preview = config_text[:200].replace("\n", " ")
        print(f"配置文本长度: {len(config_text)}，预览: {preview}")
    else:
        print("配置文本: (空)")
    print(f"网络表: {options.network_table if options.network_table else '(未提供)'}")
    print(f"OD 表: {options.od_table if options.od_table else '(未提供)'}")
    print(f"观测表: {options.observed_table if options.observed_table else '(未提供)'}")

    result_text = ""
    with DllContext(options.dll_path, options.extra_dll_dirs) as dll:
        entry = dll.TestOD_Estimate
        entry.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        entry.restype = ctypes.c_void_p
        result_ptr = entry(config_bytes, network_bytes, od_bytes, observed_bytes)
        if not result_ptr:
            print("未收到返回内容。(返回指针为空)")
            return 1
        try:
            # print(f"[DEBUG] 返回指针: 0x{int(result_ptr):016X}")
            result_text = ctypes.string_at(result_ptr).decode("utf-8", errors="replace")
        except Exception as exc:
            print(f"返回内容读取失败: {exc}")
            return 1

    if not result_text:
        print("未收到返回内容。", file=sys.stderr)
        return 1

    try:
        out_name = f"od_estimate_result_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        out_path = Path.cwd() / out_name
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(result_text)
        print(f"已保存结构化JSON: {out_path}")
    except Exception as exc:
        print(f"保存JSON失败: {exc}", file=sys.stderr)


    import json

    print("结构化返回:")
    try:
        resp = json.loads(result_text)
        status = resp.get("status")
        message = resp.get("message")
        data = resp.get("data", {})

        print(f"status: {status}")
        print(f"message: {message}")

        if isinstance(data, dict):
            summary = data.get("summary", {})
            tables = data.get("tables", {})
            history = data.get("history_tail", {})
            reports = data.get("reports", {})

            print("summary:")
            print(json.dumps(summary, ensure_ascii=False, indent=2))

            print("tables:")
            print(json.dumps(tables, ensure_ascii=False, indent=2))

            # 可选：查看历史尾部
            print("history_tail:")
            print(json.dumps(history, ensure_ascii=False, indent=2))

            # 可选：将 CSV 写回文件，便于外部工具查看
            if "top20_link_flow_csv" in reports:
                with open("report_local_analysis/parsed_top20_link_flow.csv", "w", encoding="utf-8") as f:
                    f.write(reports["top20_link_flow_csv"])
            if "top_vc_ratio_links_csv" in reports:
                with open("report_local_analysis/parsed_top_vc_ratio_links.csv", "w", encoding="utf-8") as f:
                    f.write(reports["top_vc_ratio_links_csv"])
        else:
            # 兼容旧版 data 为文本的返回
            print("raw data:")
            print(str(data))
    except Exception as ex:
        print("解析 DLL 返回 JSON 失败，原文如下：")
        print(result_text)
    try:
        payload = json.loads(result_text)
    except json.JSONDecodeError as exc:
        print(f"返回内容解析失败: {exc}", file=sys.stderr)
        return 1

    status = int(payload.get("status", 1))
    message = payload.get("message", "")
    print(f"状态码: {status}")
    if message:
        print(f"提示信息: {message}")
    return 0 if status == 0 else 1


def create_parser() -> argparse.ArgumentParser:
    """创建命令行参数解析器。"""

    parser = argparse.ArgumentParser(description="使用 TNADriver.dll 的标准示例。")
    parser.add_argument("--dll", dest="dll_path", type=Path, default=None, help="指定 TNADriver.dll 的路径，未提供时使用脚本同目录。")
    parser.add_argument("--config", dest="config_path", type=Path, default=None, help="指定 JSON 配置文件路径，未提供时使用脚本内嵌配置。")
    parser.add_argument("--network-table", dest="network_table", type=str, default=None, help="覆盖网络表名。")
    parser.add_argument("--od-table", dest="od_table", type=str, default=None, help="覆盖 OD 表名。")
    parser.add_argument("--observed-table", dest="observed_table", type=str, default=None, help="覆盖观测流量表名。")
    parser.add_argument("--extra-dll-dir", dest="extra_dll_dirs", action="append", type=Path, default=[], help="额外的 DLL 搜索目录。")
    return parser


def options_from_args(args: argparse.Namespace) -> RuntimeOptions:
    """根据参数对象组装运行选项。"""

    network_table = args.network_table.strip() if isinstance(args.network_table, str) and args.network_table.strip() else None
    od_table = args.od_table.strip() if isinstance(args.od_table, str) and args.od_table.strip() else None
    observed_table = args.observed_table.strip() if isinstance(args.observed_table, str) and args.observed_table.strip() else None
    extra_dirs = [path for path in args.extra_dll_dirs if isinstance(path, Path)]
    dll_path = args.dll_path if isinstance(args.dll_path, Path) else None
    if dll_path is None:
        dll_path = DEFAULT_DLL_PATH
    return RuntimeOptions(
        dll_path=dll_path,
        config_path=args.config_path,
        network_table=network_table,
        od_table=od_table,
        observed_table=observed_table,
        extra_dll_dirs=extra_dirs,
    )


def main(argv: Optional[Sequence[str]] = None) -> int:
    """脚本入口。"""

    parser = create_parser()
    args = parser.parse_args(argv)
    options = options_from_args(args)
    if not options.dll_path.exists():
        print(f"未找到 DLL: {options.dll_path}", file=sys.stderr)
        return 1
    if options.config_path is not None and not options.config_path.exists():
        print(f"未找到配置文件: {options.config_path}", file=sys.stderr)
        return 1
    return call_test_estimate(options)


if __name__ == "__main__":
    sys.exit(main())
