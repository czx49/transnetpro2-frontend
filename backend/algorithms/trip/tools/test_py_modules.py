import os
import sys
import json
import traceback

PROJECT_ROOT = os.path.dirname(os.path.dirname(__file__))
CFG_JSON_PATH = os.path.join(PROJECT_ROOT, "tools", "cfg.json")

# 确保项目根在 sys.path 首位，避免包查找问题
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)


def _print_title(t):
    print("\n===" , t , "===")


def _load_cfg_text(path: str) -> str:
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()


def _resolve_defaults(cfg: dict):
    db = cfg.get("db_conn_str", {})
    role = db.get("role", "user_project")
    prefix = db.get("scenario_prefix", "")
    taz = f"{role}.{prefix}other_taz_socioeconomic"
    pa = f"{role}.{prefix}other_trip_production_attraction"
    od = f"{role}.{prefix}other_od"
    return taz, pa, od


def main():
    report = {"gen":{}, "dist":{}}
    try:
        cfg_text = _load_cfg_text(CFG_JSON_PATH)
        cfg = json.loads(cfg_text)
        taz, pa_default, od_default = _resolve_defaults(cfg)

        _print_title("Import trip_generation")
        try:
            import trip_generation.trip_generation as tgen
            print("import trip_generation OK")
        except Exception as e:
            print("import trip_generation FAILED:", repr(e))
            print(traceback.format_exc())
            report["gen"] = {"import_ok": False, "error": repr(e)}
            raise

        _print_title("Run fs_run_trip_generation")
        try:
            def cb(i,p):
                print("[GEN-CB]", i, p)
            gen_json = tgen.fs_run_trip_generation(cb, cfg, taz)
            print("[GEN-JSON]", gen_json)
            try:
                gen_ret = json.loads(gen_json)
            except Exception:
                gen_ret = {}
            report["gen"] = {"import_ok": True, "json": gen_ret}
            pa_table = gen_ret.get("tables",{}).get("trip_generation_table", pa_default)
        except Exception as e:
            print("run generation FAILED:", repr(e))
            print(traceback.format_exc())
            report["gen"].update({"run_ok": False, "error": repr(e)})
            # 不中断，让分布继续使用默认表名
            pa_table = pa_default

        _print_title("Import trip_distribution")
        try:
            import trip_distribution.trip_distribution as tdist
            print("import trip_distribution OK")
        except Exception as e:
            print("import trip_distribution FAILED:", repr(e))
            print(traceback.format_exc())
            report["dist"] = {"import_ok": False, "error": repr(e)}
            raise

        _print_title("Run fs_run_trip_distribution")
        try:
            def cb2(i,p):
                print("[DIST-CB]", i, p)
            dist_json = tdist.fs_run_trip_distribution(cb2, cfg, taz, pa_table)
            print("[DIST-JSON]", dist_json)
            try:
                dist_ret = json.loads(dist_json)
            except Exception:
                dist_ret = {}
            report["dist"].update({"import_ok": True, "json": dist_ret})
        except Exception as e:
            print("run distribution FAILED:", repr(e))
            print(traceback.format_exc())
            report["dist"].update({"run_ok": False, "error": repr(e)})

    finally:
        # 保存报告
        try:
            dist_dir = os.path.join(PROJECT_ROOT, 'dist')
            os.makedirs(dist_dir, exist_ok=True)
            out_path = os.path.join(dist_dir, 'py_modules_test_result.json')
            with open(out_path, 'w', encoding='utf-8') as f:
                json.dump(report, f, ensure_ascii=False, indent=2)
            print("[FINAL] saved:", out_path)
        except Exception:
            pass


if __name__ == '__main__':
    main()
