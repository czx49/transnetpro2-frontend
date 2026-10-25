import os
import sys
import json
import csv
import ctypes
from ctypes import c_int, c_double, c_char_p, c_void_p, POINTER, byref, CFUNCTYPE

import psycopg2

# 本脚本用于测试 dist_dll\trip_model.dll：
# - 回调是否触发（iteration, percent）
# - 输入参数是否生效（taz/pa 名称、cfg_json）
# - 返回值（rc、JSON）
# - 数据库读取是否正常（表写入、行数、示例行）

PROJECT_ROOT = os.path.dirname(os.path.dirname(__file__))
DLL_PATH = os.environ.get("TRIP_MODEL_DLL", os.path.join(PROJECT_ROOT, "dist_dll", "trip_model.dll"))
CFG_JSON_PATH = os.path.join(PROJECT_ROOT, "tools", "cfg.json")

PROGCB = CFUNCTYPE(None, c_int, c_double)  # cdecl
_KEEP = []  # 防止回调被 GC


def _load_cfg_text(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def _connect_db(cfg: dict):
    db = cfg.get("db_conn_str", {})
    if db.get("connection_string"):
        conn = psycopg2.connect(db.get("connection_string"))
    else:
        conn = psycopg2.connect(
            host=db.get("host", "localhost"),
            port=db.get("port", 5432),
            dbname=db.get("dbname"),
            user=db.get("user"),
            password=db.get("password"),
        )
    conn.autocommit = True
    return conn


def _resolve_defaults(cfg: dict):
    db = cfg.get("db_conn_str", {})
    role = db.get("role", "user_project")
    prefix = db.get("scenario_prefix", "")

    sep = "" if prefix.endswith("_") else "_"

    taz = f"{role}.{prefix}{sep}other_taz_socioeconomic"
    pa = f"{role}.{prefix}{sep}other_trip_production_attraction"
    od = f"{role}.{prefix}{sep}other_od"
    return taz, pa, od


def _resolve_hist_default(cfg: dict):
    db = cfg.get("db_conn_str", {})
    role = db.get("role", "user_project")
    prefix = db.get("scenario_prefix", "")
    sep = "" if (not prefix or prefix.endswith("_")) else "_"
    return f"{role}.{prefix}{sep}other_trip_distribution_hist"


def _print_kv(title: str, data: dict):
    print(f"\n=== {title} ===")
    for k, v in data.items():
        print(f"{k}: {v}")


def _print_summary_logs(tag: str, json_text: str):
    try:
        obj = json.loads(json_text) if json_text else {}
    except Exception:
        obj = {}
    summary = obj.get("summary") if isinstance(obj, dict) else None
    logs = summary.get("logs") if isinstance(summary, dict) else None
    if isinstance(logs, list) and logs:
        print(f"\n=== {tag} LOGS ===")
        for line in logs:
            try:
                print(str(line))
            except Exception:
                pass


def test_run_generation(dll, cfg_text: str, taz_table: str):
    run_gen = dll.run_trip_generation
    run_gen.argtypes = [c_char_p, c_char_p, POINTER(c_char_p)]
    run_gen.restype = c_int

    free_json = dll.free_json
    free_json.argtypes = [c_char_p]
    free_json.restype = None

    callbacks = []  # 单接口已取消回调

    out_ptr = c_char_p()
    rc = run_gen(cfg_text.encode('utf-8'), taz_table.encode('utf-8'), byref(out_ptr))
    try:
        result_json = out_ptr.value.decode('utf-8') if out_ptr.value else ""
    finally:
        if out_ptr.value:
            free_json(out_ptr)

    print("\n[GEN-RC]", rc)
    print("[GEN-JSON]", result_json)
    return rc, result_json, callbacks


def test_run_distribution(dll, cfg_text: str, taz_table: str, pa_table: str):
    run_dist = dll.run_trip_distribution
    run_dist.argtypes = [c_char_p, c_char_p, c_char_p, POINTER(c_char_p)]
    run_dist.restype = c_int

    free_json = dll.free_json
    free_json.argtypes = [c_char_p]
    free_json.restype = None

    callbacks = []  # 单接口已取消回调

    out_ptr = c_char_p()
    rc = run_dist(cfg_text.encode('utf-8'), taz_table.encode('utf-8'), pa_table.encode('utf-8'), byref(out_ptr))
    try:
        result_json = out_ptr.value.decode('utf-8') if out_ptr.value else ""
    finally:
        if out_ptr.value:
            free_json(out_ptr)

    print("\n[DIST-RC]", rc)
    print("[DIST-JSON]", result_json)
    return rc, result_json, callbacks


def test_run_all(dll, cfg_text: str, taz_table: str, hist_od_table: str = None):
    run_all = dll.run_trip_model_all
    run_all.argtypes = [PROGCB, c_char_p, c_char_p, c_char_p, POINTER(c_char_p)]
    run_all.restype = c_int

    free_json = dll.free_json
    free_json.argtypes = [c_char_p]
    free_json.restype = None

    callbacks = []

    cb_disable = os.environ.get("TEST_DLL_NO_CALLBACK") == "1"
    if not cb_disable:
        @PROGCB
        def on_progress(it, pct):
            callbacks.append((int(it), float(pct)))
            print(f"[ALL-CB] {it} {pct}")
        _KEEP.append(on_progress)
    else:
        @PROGCB
        def on_progress(it, pct):
            pass
        _KEEP.append(on_progress)

    hist_od_bytes = hist_od_table.encode('utf-8') if hist_od_table else b''
    out_ptr = c_char_p()
    rc = run_all(on_progress, cfg_text.encode('utf-8'), taz_table.encode('utf-8'), hist_od_bytes, byref(out_ptr))
    try:
        result_json = out_ptr.value.decode('utf-8') if out_ptr.value else ""
    finally:
        if out_ptr.value:
            free_json(out_ptr)

    print("\n[ALL-RC]", rc)
    print("[ALL-JSON]", result_json)
    return rc, result_json, callbacks


def verify_db_outputs(cfg: dict, pa_table: str, od_table: str):
    ok = True
    msgs = []
    pa_cnt = None
    od_cnt = None
    conn = _connect_db(cfg)
    try:
        with conn.cursor() as cur:
            # 检查 PA 表
            try:
                cur.execute(f"SELECT count(*) FROM {pa_table}")
                pa_cnt = cur.fetchone()[0]
                cur.execute(f"SELECT zone_id,production,attraction FROM {pa_table} ORDER BY zone_id LIMIT 5")
                pa_head = cur.fetchall()
                msgs.append(f"PA rows: {pa_cnt}, head: {pa_head}")
            except Exception as e:
                ok = False
                msgs.append(f"PA check error: {e}")

            # 检查 OD 表（不依赖是否存在 id 列，只读核心列）
            try:
                cur.execute(f"SELECT count(*) FROM {od_table}")
                od_cnt = cur.fetchone()[0]
                cur.execute(f"SELECT f_id,t_id,demand FROM {od_table} ORDER BY f_id,t_id LIMIT 5")
                od_head = cur.fetchall()
                msgs.append(f"OD rows: {od_cnt}, head: {od_head}")
            except Exception as e:
                ok = False
                msgs.append(f"OD check error: {e}")
    finally:
        conn.close()

    return ok, msgs, {"pa_rows": pa_cnt, "od_rows": od_cnt}


def main():
    if not os.path.exists(DLL_PATH):
        print(f"[ERROR] DLL 未找到: {DLL_PATH}")
        sys.exit(1)

    cfg_text = _load_cfg_text(CFG_JSON_PATH)
    cfg = json.loads(cfg_text)
    print("\n======Config======\n")
    print(cfg_text, "\n")
    taz, pa_default, od_default = _resolve_defaults(cfg)
    hist_default = _resolve_hist_default(cfg)

    # hist_default = None
    dll = ctypes.CDLL(DLL_PATH)

    _print_kv("输入参数", {
        "dll": DLL_PATH,
        "cfg_json": CFG_JSON_PATH,
        "taz": taz,
        "pa(default)": pa_default,
        "od(default)": od_default,
        "hist_od(default)": hist_default,
    })

    separate = os.environ.get("TEST_DLL_SEPARATE") == "1"
    if separate:
        gen_rc, gen_json, _ = test_run_generation(dll, cfg_text, taz)
        _print_summary_logs("GEN", gen_json)
        try:
            gen_ret = json.loads(gen_json) if gen_json else {}
        except Exception:
            gen_ret = {}
        pa_table = gen_ret.get("data", {}).get("tables", {}).get("trip_generation_table", pa_default)
        dist_rc, dist_json, _ = test_run_distribution(dll, cfg_text, taz, pa_table)
        _print_summary_logs("DIST", dist_json)
        all_rc, all_json, all_cbs = dist_rc, dist_json, []
        all_ret = json.loads(all_json) if all_json else {}
    else:
        all_rc, all_json, all_cbs = test_run_all(dll, cfg_text, taz, hist_default)
        try:
            all_ret = json.loads(all_json) if all_json else {}
        except Exception:
            all_ret = {}
    try:
        _print_summary_logs("ALL", all_json)
    except Exception:
        pass
    pa_table = all_ret.get("data", {}).get("tables", {}).get("trip_generation_table", pa_default)
    od_table = all_ret.get("data", {}).get("tables", {}).get("trip_distribution_table", od_default)

    print("\n=== 输出表名 ===")
    print(f"trip_generation_table: {pa_table}")
    print(f"trip_distribution_table: {od_table}")

    try:
        inputs = all_ret.get("inputs", {}) if isinstance(all_ret, dict) else {}
        _print_kv("inputs echo", {
            "taz_table": inputs.get("taz_table"),
            "hist_od_table_given": inputs.get("hist_od_table_given"),
            "default_hist_od_table": inputs.get("default_hist_od_table"),
            "hist_od_used": inputs.get("hist_od_used"),
            "hist_od_rows": inputs.get("hist_od_rows"),
        })
    except Exception:
        pass

    # 导出 CSV 到 dist_dll
    try:
        dist_dir = os.path.join(PROJECT_ROOT, 'dist_dll')
        os.makedirs(dist_dir, exist_ok=True)
        # 保存聚合 JSON 结果
        json_path = os.path.join(dist_dir, 'result.json')
        with open(json_path, 'w', encoding='utf-8') as f:
            f.write(all_json if isinstance(all_json, str) else '')
        print(f"[JSON] saved: {json_path}")
        pa_csv = os.path.join(dist_dir, 'trip_generation.csv')
        od_csv = os.path.join(dist_dir, 'other_od.csv')

        # 导出 PA
        try:
            conn = _connect_db(cfg)
            try:
                with conn.cursor() as cur:
                    cur.execute(f"SELECT zone_id,production,attraction FROM {pa_table} ORDER BY zone_id")
                    rows = cur.fetchall()
                    cols = [d[0] for d in cur.description]
                with open(pa_csv, 'w', newline='', encoding='utf-8-sig') as f:
                    w = csv.writer(f)
                    w.writerow(cols)
                    w.writerows(rows)
                print(f"[CSV] saved: {pa_csv}")
            finally:
                conn.close()
        except Exception as _:
            pass

        # 导出 OD（优先含 id,m_special，失败则降级）
        try:
            conn = _connect_db(cfg)
            try:
                with conn.cursor() as cur:
                    try:
                        cur.execute(f"SELECT id,f_id,t_id,demand,m_special FROM {od_table} ORDER BY id,f_id,t_id")
                    except Exception:
                        cur.execute(f"SELECT f_id,t_id,demand FROM {od_table} ORDER BY f_id,t_id")
                    rows = cur.fetchall()
                    cols = [d[0] for d in cur.description]
                with open(od_csv, 'w', newline='', encoding='utf-8-sig') as f:
                    w = csv.writer(f)
                    w.writerow(cols)
                    w.writerows(rows)
                print(f"[CSV] saved: {od_csv}")
            finally:
                conn.close()
        except Exception as _:
            pass
    except Exception as _:
        pass

    # 校验数据库输出
    ok, msgs, counts = verify_db_outputs(cfg, pa_table, od_table)

    _print_kv("回调触发统计", {
        "all_callbacks": len(all_cbs),
    })
    print("\n=== 数据读取/写入检查 ===")
    for m in msgs:
        print(m)

    print("\n=== 结果汇总（简） ===")
    print("ALL rc:", all_rc, "; DB OK:", ok)
    all_status = 0
    try:
        all_status = int(all_ret.get("status", 0))
    except Exception:
        all_status = 0
    if all_status != 1 or not ok:
        sys.exit(2)


if __name__ == "__main__":
    main()
