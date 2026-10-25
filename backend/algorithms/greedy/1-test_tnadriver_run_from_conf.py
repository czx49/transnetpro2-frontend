import os
import sys
import json
import platform
from ctypes import cdll, c_char_p, c_int, c_double, create_string_buffer, CFUNCTYPE, string_at
from datetime import datetime
import threading
import time
import math

try:
    from tqdm import tqdm
except Exception:
    tqdm = None


def add_dll_search_paths(paths):
    for p in paths:
        if p and os.path.isdir(p):
            try:
                os.add_dll_directory(p)
                print(f"✓ 添加DLL搜索目录: {p}")
            except Exception:
                os.environ['PATH'] = p + os.pathsep + os.environ.get('PATH', '')
                print(f"! os.add_dll_directory不可用，已添加到PATH: {p}")


def _normalize_scenario_prefix(raw):
    if not raw:
        return "", ""
    s = str(raw).strip()
    while s.endswith('_'):
        s = s[:-1]
    prefix = f"{s}_" if s else ""
    return s, prefix


def _build_output_tables(db_conf, network_table_bytes):
    scenario_raw = db_conf.get('scenario_prefix', '')
    _, prefix = _normalize_scenario_prefix(scenario_raw)
    schema = (db_conf.get('role') or '').strip()

    network_table = network_table_bytes.decode('utf-8') if isinstance(network_table_bytes, (bytes, bytearray)) else str(network_table_bytes)
    if not schema and '.' in network_table:
        schema = network_table.split('.')[0]

    def qualify(base):
        return f"{schema}.{base}" if schema else base

    tables = {
        "link_flow_results": qualify(prefix + "greedy_link_flow_results"),
        "path_results": qualify(prefix + "greedy_path_results"),
        "iteration_history": qualify(prefix + "greedy_iteration_history"),
        "summary_statistics": qualify(prefix + "greedy_summary_statistics"),
    }
    return tables


def _print_output_tables_and_columns(db_conf, network_table_bytes):
    tables = _build_output_tables(db_conf, network_table_bytes)
    print("=== 输出结果表（含 schema） ===")
    for key, value in tables.items():
        print(f"{key}: {value}")

    network_table = network_table_bytes.decode('utf-8') if isinstance(network_table_bytes, (bytes, bytearray)) else str(network_table_bytes)
    new_columns = ["volume", "v_c", "bottleneck", "volume_edit", "saturation_edit", "saturation_diff", "link_flow_diff"]
    print("=== 网络拓扑表更新 ===")
    print(f"network_table: {network_table}")
    print("新增/更新列：", ", ".join(new_columns))


def load_tnadriver(dll_path):
    print(f"=== 准备加载 DLL: {dll_path} ===")
    if not os.path.exists(dll_path):
        print(f"✗ 未找到 DLL: {dll_path}")
        return None
    try:
        dll = cdll.LoadLibrary(dll_path)
        print("✓ DLL 加载成功")
        return dll
    except Exception as e:
        print(f"✗ DLL 加载失败: {e}")
        return None


def read_progress_cache(dir_path):
    p = os.path.join(dir_path, 'greedy_progress.json')
    try:
        with open(p, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return None


def read_progress_history(dir_path):
    p = os.path.join(dir_path, 'greedy_progress_history.jsonl')
    out = []
    try:
        with open(p, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    out.append(json.loads(line))
                except Exception:
                    pass
    except Exception:
        return None
    return out


class _ProgMon:
    def __init__(self, json_path, conv):
        self.json_path = json_path
        self.conv = max(float(conv), 1e-16) if conv else 1e-6
        self.t0 = None
        self.stop_evt = threading.Event()
        self.th = None
        self.bar = None
        self.spinner_idx = 0
        self.total = 1000

    def _compute(self, gap):
        try:
            g = float(gap)
        except Exception:
            return 0.0
        if g <= 0:
            return 1.0
        if self.t0 is None:
            self.t0 = max(g, self.conv * 10.0)
        denom = math.log(max(self.t0 / self.conv, 1.0))
        if denom <= 0:
            return 0.0
        x = math.log(max(g / self.conv, 1.0)) / denom
        p = 1.0 - max(0.0, min(1.0, x))
        return p

    def _style(self, gap):
        try:
            r = float(gap) / self.conv
        except Exception:
            r = 1e6
        if r > 100:
            return '阶段: 初始'
        elif r > 10:
            return '阶段: 逼近'
        elif r > 1.2:
            return '阶段: 临近'
        else:
            return '阶段: 收敛'

    def start(self):
        self.th = threading.Thread(target=self._loop, daemon=True)
        self.th.start()

    def stop(self):
        self.stop_evt.set()
        if self.th:
            self.th.join(timeout=2.0)
        if self.bar is not None:
            try:
                self.bar.close()
            except Exception:
                pass

    def _loop(self):
        use_tqdm = False
        if use_tqdm:
            self.bar = tqdm(total=self.total, ascii=True, dynamic_ncols=True, bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}<{remaining}, {rate_fmt}] {postfix}')
        frames = ['|', '/', '-', '\\']
        while not self.stop_evt.is_set():
            try:
                with open(self.json_path, 'r', encoding='utf-8') as f:
                    d = json.load(f)
                it = int(d.get('iter', 0))
                gap = d.get('gap', None)
                p = self._compute(gap)
                desc = self._style(gap)
                if use_tqdm and self.bar is not None:
                    val = int(p * self.total)
                    if val < 0:
                        val = 0
                    if val > self.total:
                        val = self.total
                    self.bar.n = val
                    self.bar.set_description_str(desc)
                    self.bar.set_postfix_str(f"iter={it} gap={gap}")
                    self.bar.refresh()
                else:
                    ch = frames[self.spinner_idx % len(frames)]
                    self.spinner_idx += 1
                    s = f"{ch} {desc} iter={it} gap={gap}\r"
                    sys.stdout.write(s)
                    sys.stdout.flush()
            except Exception:
                if not use_tqdm:
                    ch = frames[self.spinner_idx % len(frames)]
                    self.spinner_idx += 1
                    sys.stdout.write(f"{ch} 等待进度...\r")
                    sys.stdout.flush()
            time.sleep(0.25)


def _load_conf_json(conf_path):
    if not os.path.exists(conf_path):
        raise FileNotFoundError(f"未找到配置文件: {conf_path}")
    with open(conf_path, 'r', encoding='utf-8') as f:
        d = json.load(f)
    if not isinstance(d, dict):
        raise ValueError("conf.json 内容必须是 JSON object（字典）")
    return d


def _parse_args(argv):
    conf = None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--conf', '-c'):
            if i + 1 >= len(argv):
                raise ValueError("--conf 后必须跟配置文件路径")
            conf = argv[i + 1]
            i += 2
            continue
        if a in ('--help', '-h'):
            print("用法: python 1-test_tnadriver_run_from_conf.py [--conf conf.json]")
            return {'help': True}
        i += 1
    return {'conf': conf, 'help': False}


def main():
    print("=== TNADriver.dll 测试（从 conf.json 读取配置）===")
    print(f"Python: {sys.version.split()[0]} | OS: {platform.platform()} | Arch: {platform.architecture()}")

    current_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.join(current_dir, 'TNADriver.dll')

    pg_bin = os.environ.get('PG_BIN', r'D:\postgresql\16\bin')
    add_dll_search_paths([current_dir, pg_bin])

    dll = load_tnadriver(dll_path)
    if dll is None:
        return 1

    try:
        func = dll.TestGreedy_dijkWithPostgreSQLEx
        CBK = CFUNCTYPE(None, c_int, c_double, c_int)
        func.argtypes = [CBK, c_char_p, c_char_p, c_char_p]
        func.restype = c_char_p
    except AttributeError as e:
        print(f"✗ 未找到导出函数 TestGreedy_dijkWithPostgreSQLEx: {e}")
        return 2

    last_cb = {'iter': None, 'precision': None, 'percent': None}

    def _py_progress(iter, precision, percent):
        try:
            last_cb['iter'] = int(iter)
            last_cb['precision'] = float(precision)
            last_cb['percent'] = int(percent)
            print(f"[CBK] iter={iter} precision={precision:.6e} percent={percent}")
        except Exception:
            pass

    try:
        args = _parse_args(sys.argv[1:])
        if args.get('help'):
            return 0

        conf_path = args.get('conf')
        if not conf_path:
            conf_path = os.path.join(current_dir, 'conf.json')
        if not os.path.isabs(conf_path):
            conf_path = os.path.join(current_dir, conf_path)

        config = _load_conf_json(conf_path)

        json_str = json.dumps(config, separators=(',', ':'), ensure_ascii=False)
        print("=== 发送 JSON 配置 ===")
        print(json_str)
        json_buf = create_string_buffer(json_str.encode('utf-8'))

        net = config.get('network_table')
        od = config.get('od_table')
        if not net:
            net = "user_project.project63_user1_case41_road_way"
        if not od:
            od = "user_project.project63_user1_case41_other_od"

        network_table = net.encode('utf-8') if isinstance(net, str) else net
        od_table = od.encode('utf-8') if isinstance(od, str) else od

        print("=== 开始调用 TestGreedy_dijkWithPostgreSQLEx ===")
        conv_target = 1e-6
        try:
            conv_target = float(str(config.get('algorithm_params', {}).get('conv', '1e-06')))
        except Exception:
            conv_target = 1e-6

        mon = _ProgMon(os.path.join(current_dir, 'greedy_progress.json'), conv_target)
        mon.start()
        cb = CBK(_py_progress)
        print(f"[CALL] net={network_table!r} od={od_table!r} conf={conf_path}")
        resp_bytes = func(cb, json_buf, network_table, od_table)
        if not resp_bytes:
            print("✗ 无返回数据（NULL）")
            mon.stop()
            return 3

        raw = string_at(resp_bytes)
        print(f"=== DLL 返回长度: {len(raw)} 字节 ===")

        def _decode_clean(b: bytes) -> str:
            s = b.decode('utf-8', errors='replace')
            if s.startswith('\ufeff'):
                s = s.lstrip('\ufeff')
            i = s.find('{')
            j = s.rfind('}')
            if i >= 0 and j > i:
                return s[i:j + 1]
            return s

        resp_str = _decode_clean(raw)

        try:
            resp = json.loads(resp_str)
        except json.JSONDecodeError:
            si = raw.find(b'{')
            sj = raw.rfind(b'}')
            if si >= 0 and sj > si:
                resp_str = raw[si:sj + 1].decode('utf-8', errors='replace')
                resp = json.loads(resp_str)
            else:
                print("! 返回内容非合法JSON：未找到外层花括号")
                mon.stop()
                return 5

        try:
            ts = datetime.now().strftime('%Y%m%d_%H%M%S')
            out_path = os.path.join(current_dir, f'greedy_response_{ts}.json')
            with open(out_path, 'w', encoding='utf-8') as f:
                f.write(resp_str)
            print(f"✓ 已保存返回JSON到: {out_path}")
        except Exception as e:
            print(f"! 保存返回JSON失败: {e}")

        try:
            data = resp.get('data')
            if isinstance(data, dict):
                if 'summary' in data:
                    print("=== Summary (data.summary) ===")
                    print(json.dumps(data.get('summary'), ensure_ascii=False))
                if 'params' in data:
                    print("=== Params (data.params) ===")
                    print(json.dumps(data.get('params'), ensure_ascii=False))
                ss = data.get('saturation_summary')
                if isinstance(ss, str):
                    try:
                        ss = json.loads(ss)
                    except Exception:
                        ss = None
                if isinstance(ss, dict):
                    if 'summary' in ss:
                        print("=== Summary ===")
                        print(json.dumps(ss.get('summary'), ensure_ascii=False))
                    if 'params' in ss:
                        print("=== Params ===")
                        print(json.dumps(ss.get('params'), ensure_ascii=False))

            conv_v = None
            max_iter_v = None
            if isinstance(data, dict):
                p = data.get('params')
                if isinstance(p, dict):
                    conv_v = p.get('conv') if p.get('conv') is not None else p.get('收敛阈值')
                    max_iter_v = p.get('max_iter') if p.get('max_iter') is not None else p.get('最大迭代')
            prog = read_progress_cache(current_dir)
            it_v = prog.get('iter') if isinstance(prog, dict) else None
            gap_v = prog.get('gap') if isinstance(prog, dict) else None
            percent_v = last_cb.get('percent')
            print(f"=== 迭代信息 ===\nconv={conv_v} max_iter={max_iter_v} iter={it_v} gap={gap_v} percent={percent_v}")
            status = resp.get('status')
            print(f"=== 状态码: {status}")
            _print_output_tables_and_columns(config.get('db_conn_str', {}), network_table)
            mon.stop()
            return 0 if status == 1 else 4
        except Exception as e:
            print(f"! 解析/输出失败: {e}")
            mon.stop()
            return 6

    except Exception as e:
        print(f"✗ 调用失败: {e}")
        return 6


if __name__ == '__main__':
    sys.exit(main())
