import json
import argparse
import os

import numpy as np
import pandas as pd
import psycopg2

from pandas import DataFrame


__all__ = ["fs_run_trip_distribution"]



def _connect(db: dict) -> "psycopg2.extensions.connection":
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


def _resolve_tables(db: dict, taz_table_name: str, trip_generation_table_name: str, hist_od_table_name: str = None) -> tuple[str, str, str, str]:
    role = db.get("role", "user_project")
    prefix = db.get("scenario_prefix", "")
    # Handle prefix with or without trailing underscore
    sep = "" if prefix.endswith("_") else "_"

    taz = taz_table_name or f"{role}.{prefix}{sep}other_taz_socioeconomic"
    pa = trip_generation_table_name or f"{role}.{prefix}{sep}other_trip_production_attraction"

    # 仅当显式传入非空 hist_od_table_name 时才使用历史OD；否则返回空串以便上层跳过标定
    if hist_od_table_name is not None and str(hist_od_table_name).strip() != "":
        hist_od = hist_od_table_name
    else:
        hist_od = ""
    out_distribution_table = f"{role}.{prefix}{sep}other_od"
    return taz, pa, hist_od, out_distribution_table


def _read_taz(conn, taz_table: str) -> DataFrame:
    with conn.cursor() as cur:
        sql = f"SELECT id,longitude,latitude FROM {taz_table}"
        cur.execute(sql)
        rows = cur.fetchall()
        cols = [desc[0] for desc in cur.description]
    df = pd.DataFrame(rows, columns=cols)
    for col in df.columns:
        df[col] = pd.to_numeric(df[col])
    df = df.sort_values("id").reset_index(drop=True)
    return df


def _read_pa(conn, pa_table: str) -> DataFrame:
    with conn.cursor() as cur:
        sql = f"SELECT zone_id,production,attraction FROM {pa_table}"
        cur.execute(sql)
        rows = cur.fetchall()
        cols = [desc[0] for desc in cur.description]
    df = pd.DataFrame(rows, columns=cols)
    for col in df.columns:
        df[col] = pd.to_numeric(df[col])
    df = df.sort_values("zone_id").reset_index(drop=True)
    return df


def _gravity_doubly_constrained(production: np.ndarray, attraction: np.ndarray, C: np.ndarray,
    param_lambda: float, max_iter: int, tol: float) -> np.ndarray:
    n = len(production)
    F = np.exp(-param_lambda * C)
    F[F < 1e-12] = 1e-12
    alpha = np.ones(n, dtype=float)
    beta = np.ones(n, dtype=float)
    for _ in range(max_iter):
        denom_alpha = (beta * attraction)[None, :] * F
        denom_alpha = denom_alpha.sum(axis=1)
        alpha_new = np.where(denom_alpha > 0, 1.0 / denom_alpha, 0.0)
        denom_beta = (alpha_new * production)[:, None] * F
        denom_beta = denom_beta.sum(axis=0)
        beta_new = np.where(denom_beta > 0, 1.0 / denom_beta, 0.0)
        diff_alpha = np.max(np.abs(alpha_new - alpha))
        diff_beta = np.max(np.abs(beta_new - beta))
        alpha, beta = alpha_new, beta_new
        if max(diff_alpha, diff_beta) < tol:
            break
    T = (alpha[:, None] * production[:, None]) * (beta[None, :] * attraction[None, :]) * F
    return T


# doubly-constrained gravity model
def _trip_distribution(pa_df: DataFrame, taz_df: DataFrame, alg: dict, on_progress, hist_od_df: DataFrame | None = None) -> list[list[float]]:
    param_lambda = alg.get("param_lambda", 0.1)
    diag_eps = alg.get("diag_eps", 0.1)
    max_iter = alg.get("max_iter", 20)
    tol = alg.get("tol", 1e-6)
    production = pa_df["production"].to_numpy().astype(float)
    attraction = pa_df["attraction"].to_numpy().astype(float)
    C = _obtain_taz_cost_from_coor(taz_df, diag_eps=diag_eps)
    if hist_od_df is not None and not hist_od_df.empty:
        lambda_calib = _calibrate_lambda_with_hist_od(taz_df, hist_od_df, C, alg)
        if lambda_calib is not None:
            param_lambda = lambda_calib
    on_progress(6, 60)
    T = _gravity_doubly_constrained(production, attraction, C, param_lambda, max_iter, tol)
    on_progress(7, 70)
    return T


def _obtain_taz_cost_from_coor(taz_df: DataFrame, diag_eps: float) -> np.ndarray:
    R_earth_km = 6371.0
    latitude = np.radians(taz_df["latitude"].to_numpy(dtype=float))
    longitude = np.radians(taz_df["longitude"].to_numpy(dtype=float))
    latitude_diff = latitude[:, None] - latitude[None, :]
    longitude_diff = longitude[:, None] - longitude[None, :]
    a = np.sin(latitude_diff / 2.0)**2 + np.cos(latitude[:, None]) * np.cos(latitude[None, :]) * np.sin(longitude_diff / 2.0)**2
    c = 2 * np.arcsin(np.sqrt(a))
    C_km = R_earth_km * c  # km
    if diag_eps is not None and diag_eps > 0:
        np.fill_diagonal(C_km, diag_eps)
    return C_km


def _read_hist_od(conn, hist_od_table: str) -> DataFrame:
    with conn.cursor() as cur:
        sql = f"SELECT f_id, t_id, demand FROM {hist_od_table}"
        cur.execute(sql)
        rows = cur.fetchall()
        if not rows:
            return pd.DataFrame(columns=["f_id", "t_id", "demand"])
        cols = [desc[0] for desc in cur.description]
    df = pd.DataFrame(rows, columns=cols)
    for col in df.columns:
        df[col] = pd.to_numeric(df[col])
    return df


def _table_has_rows(conn, table_name: str) -> tuple[bool, int]:
    """检查指定表是否存在且有数据，返回 (是否有数据, 行数)。

    若表不存在或查询出错，则视为无数据，返回 (False, 0)。
    """
    if not table_name:
        return False, 0
    try:
        with conn.cursor() as cur:
            cur.execute(f"SELECT count(*) FROM {table_name}")
            cnt = cur.fetchone()[0]
        try:
            cnt_int = int(cnt)
        except Exception:
            cnt_int = 0
        return cnt_int > 0, cnt_int
    except Exception:
        return False, 0


def _hist_od_to_matrix(taz_df: DataFrame, hist_od_df: DataFrame, f_col: str = "f_id", t_col: str = "t_id", val_col: str = "demand") -> np.ndarray:
    zone_ids = taz_df["id"].to_numpy()
    id_to_idx = {int(z): idx for idx, z in enumerate(zone_ids)}
    n = len(zone_ids)
    T0 = np.zeros((n, n), dtype=float)
    for _, row in hist_od_df.iterrows():
        fid = int(row[f_col]); tid = int(row[t_col]); val = float(row[val_col])
        i = id_to_idx.get(fid); j = id_to_idx.get(tid)
        if i is None or j is None:
            continue
        T0[i, j] = val
    return T0


def _calibrate_lambda_with_hist_od(taz_df: DataFrame, hist_od_df: DataFrame, C: np.ndarray, alg: dict) -> float | None:
    if hist_od_df is None or hist_od_df.empty:
        return None
    T_hist = _hist_od_to_matrix(taz_df, hist_od_df)
    total_hist = T_hist.sum()
    if total_hist <= 0:
        return None
    P0 = T_hist.sum(axis=1)
    A0 = T_hist.sum(axis=0)
    c_obs = float((T_hist * C).sum() / total_hist)
    lambda_min = alg.get("lambda_min", 0.01)
    lambda_max = alg.get("lambda_max", 1.0)
    lambda_steps = alg.get("lambda_steps", 10)
    max_iter_calib = alg.get("max_iter_calib", alg.get("max_iter", 50))
    tol_calib = alg.get("tol_calib", alg.get("tol", 1e-5))
    best_lambda = None
    best_err = float("inf")
    for lam in np.linspace(lambda_min, lambda_max, lambda_steps):
        T_model = _gravity_doubly_constrained(P0, A0, C, lam, max_iter_calib, tol_calib)
        total_model = T_model.sum()
        if total_model <= 0:
            continue
        c_model = float((T_model * C).sum() / total_model)
        err = abs(c_model - c_obs)
        if err < best_err:
            best_err = err
            best_lambda = lam
    return best_lambda


def _distribution_matrix_to_table(distribution: list[list[float]], taz_df: DataFrame, id_col: str ="id") -> DataFrame:
    zone_ids = taz_df[id_col].to_numpy()
    if isinstance(distribution, pd.DataFrame):
        mat = distribution.to_numpy()
    else:
        mat = np.asarray(distribution)
    n = len(zone_ids)
    assert mat.shape == (n, n)
    f_ids = np.repeat(zone_ids, n)
    t_ids = np.tile(zone_ids, n)
    demand = mat.reshape(-1)
    distribution_df = pd.DataFrame({
        "f_id": f_ids,
        "t_id": t_ids,
        "demand": demand,
        "m_special": "N"   # default N
    })
    return distribution_df


def _write_trip_distribution_df(conn, out_distribution_table_name: str, distribution_df: pd.DataFrame):
    with conn.cursor() as cur:
        cur.execute("SET lock_timeout = '2s'")
        cur.execute(f"""
        CREATE TABLE IF NOT EXISTS {out_distribution_table_name} (
          f_id INTEGER NOT NULL,
          t_id INTEGER NOT NULL,
          demand NUMERIC,
          m_special VARCHAR,
          created_at TIMESTAMPTZ NOT NULL DEFAULT now()
        )
        """)
        cur.execute(f"DELETE FROM {out_distribution_table_name}")
        cols = ["f_id", "t_id", "demand", "m_special"]
        rows = list(distribution_df[cols].itertuples(index=False, name=None))
        cur.executemany(
            f"INSERT INTO {out_distribution_table_name}(f_id, t_id, demand, m_special) VALUES (%s,%s,%s,%s)",
            rows,
        )
    conn.commit()
    return len(rows)


def progress(iteration: int, percent: float):
    print(iteration, percent)
    # return iteration, percent


def fs_run_trip_distribution(on_progress, cfg: dict, taz_table_name: str, trip_generation_table_name, hist_od_table_name: str = None):
    logs: list[str] = []
    db, alg = cfg.get("db_conn_str", {}), cfg.get("algorithm_params", {})
    out_distribution_table_name = ""
    try:
        # 1) connect
        on_progress(1, 10)
        conn = _connect(db)
        logs.append(
            f"db.connect ok: host={db.get('host','localhost')} port={db.get('port',5432)} dbname={db.get('dbname')} user={db.get('user')}"
        )
        on_progress(2, 20)

        # 2) resolve & read
        taz, pa, hist_od_table_name, out_distribution_table_name = _resolve_tables(db, taz_table_name, trip_generation_table_name, hist_od_table_name)
        logs.append(f"tables: taz={taz}, pa={pa}, hist_pd={hist_od_table_name}, out_od={out_distribution_table_name}")
        on_progress(3, 30)

        taz_df = _read_taz(conn, taz)
        logs.append(f"read.taz rows={len(taz_df)}")
        on_progress(4, 40)

        pa_df = _read_pa(conn, pa)
        logs.append(f"read.pa rows={len(pa_df)} sumP={float(pa_df['production'].sum()) if len(pa_df) else 0.0} sumA={float(pa_df['attraction'].sum()) if len(pa_df) else 0.0}")
        on_progress(5, 50)

        # 历史OD处理：当调用方希望启用历史OD（传入非空表名）时，
        # 依次尝试：hist_od_table_given -> 默认表 role.scenario_prefix+"trip_distribution_hist" -> role.scenario_prefix+"other_od" 拷贝到默认表。
        hist_od_df = None
        hist_rows = 0
        role = db.get("role", "user_project")
        prefix = db.get("scenario_prefix", "")

        # Handle prefix for other_od table name
        sep = "" if prefix.endswith("_") else "_"
        other_od_table = f"{role}.{prefix}{sep}other_od"

        default_hist_od_table = f"{role}.{prefix}{sep}other_trip_distribution_hist"

        # 原始调用给定的历史OD表名（可能为空）
        hist_od_table_given = hist_od_table_name if hist_od_table_name is not None else ""
        hist_od_table_given = str(hist_od_table_given).strip()

        # 历史OD探测：不要求调用方必须显式提供表名，也会尝试默认历史OD表与 other_od 回退
        logs.append(f"hist.od check: given={hist_od_table_given}, default_hist={default_hist_od_table}, other_od={other_od_table}")

        # 1) 优先检查调用方给定的表（若提供）
        use_hist_table = ""
        if hist_od_table_given:
            ok_given, cnt_given = _table_has_rows(conn, hist_od_table_given)
            if ok_given:
                use_hist_table = hist_od_table_given
                hist_od_table_name = use_hist_table
                logs.append(f"hist.od use given table: {hist_od_table_given}, rows={cnt_given}")

        # 2) 若给定表不可用，则检查默认历史OD表
        if not use_hist_table:
            ok_def, cnt_def = _table_has_rows(conn, default_hist_od_table)
            if ok_def:
                use_hist_table = default_hist_od_table
                hist_od_table_name = use_hist_table
                logs.append(f"hist.od fallback to default table: {default_hist_od_table}, rows={cnt_def}")
            else:
                # 3) 若二者都不存在或无数据，则尝试从 other_od 拷贝到默认历史OD表
                ok_other, cnt_other = _table_has_rows(conn, other_od_table)
                if ok_other:
                    try:
                        with conn.cursor() as cur:
                            # 回退场景仅用于“运行前兜底提供历史OD”：尽量不破坏既有表结构。
                            # 强制覆盖的动作放到算法运行结束、other_od写入之后再做。
                            cur.execute(f"""
                            CREATE TABLE IF NOT EXISTS {default_hist_od_table} (
                              f_id INTEGER NOT NULL,
                              t_id INTEGER NOT NULL,
                              demand NUMERIC
                            )
                            """)
                            cur.execute(f"DELETE FROM {default_hist_od_table}")
                            cur.execute(
                                f"INSERT INTO {default_hist_od_table}(f_id, t_id, demand) "
                                f"SELECT f_id, t_id, demand FROM {other_od_table}"
                            )
                        ok_def2, cnt_def2 = _table_has_rows(conn, default_hist_od_table)
                        if ok_def2:
                            use_hist_table = default_hist_od_table
                            hist_od_table_name = use_hist_table
                            logs.append(f"hist.od copied from {other_od_table} to {default_hist_od_table}, rows={cnt_def2}")
                        else:
                            logs.append(f"hist.od copy from {other_od_table} to {default_hist_od_table} finished but no rows, treat as no hist")
                    except Exception as e:
                        logs.append(f"hist.od copy from other_od failed: {e.__class__.__name__}: {e}")
                else:
                    logs.append(f"hist.od other_od table not available or empty: {other_od_table}")

        # 4) 若最终确认有可用的历史OD表，则读取；否则回退为不使用历史OD，并记录中文警告
        if hist_od_table_name is not None and str(hist_od_table_name).strip() != "":
            try:
                hist_od_df = _read_hist_od(conn, hist_od_table_name)
                hist_rows = len(hist_od_df) if hist_od_df is not None else 0
            except Exception as e:
                hist_od_df = None
                hist_rows = 0
                logs.append(f"hist.od read.error: {e.__class__.__name__}: {e}")
        if hist_rows <= 0:
            hist_od_df = None
            hist_rows = 0
            warning_msg = "没有可用的历史OD数据，请检查数据表"
            logs.append(f"hist.od disabled: {warning_msg}")

        use_hist_flag = bool(hist_rows > 0)
        logs.append(f"hist.od detected use_calib={use_hist_flag} rows={hist_rows}")

        # 3) distribution compute
        distribution_matrix = _trip_distribution(pa_df, taz_df, alg, on_progress, hist_od_df=hist_od_df)
        logs.append(f"compute.distribution n={len(taz_df)}")
        distribution_df = _distribution_matrix_to_table(distribution_matrix, taz_df)
        logs.append(f"to_table rows={len(distribution_df)}")

        # 限制 OD 规模：剔除内部OD(f_id==t_id)后，全局仅保留需求(demand)最大的前K条 OD 记录
        # 说明：这样可避免全量 n*n 写入导致 OD 表过大，同时去掉小区内部流量。
        try:
            top_k = int(alg.get("od_top_k", 500) or 500)
        except Exception:
            top_k = 500
        if top_k > 0:
            before_rows = len(distribution_df)

            # OD 需求缩放：允许整体放大/缩小需求量（默认 1.0）
            try:
                od_scale = float(alg.get("od_scale", cfg.get("od_scale", 1.0)))
            except Exception:
                od_scale = 1.0
            if od_scale != 1.0:
                try:
                    distribution_df["demand"] = distribution_df["demand"].astype(float) * od_scale
                except Exception:
                    distribution_df["demand"] = distribution_df["demand"] * od_scale
                logs.append(f"od.scale applied: scale={od_scale}")

            # 先剔除小区内部流量（f_id == t_id）
            try:
                distribution_df = distribution_df.loc[distribution_df["f_id"] != distribution_df["t_id"]]
            except Exception:
                pass
            distribution_df = (
                distribution_df.sort_values(["demand"], ascending=[False])
                .head(top_k)
                .reset_index(drop=True)
            )
            logs.append(f"od.topk applied(global,no-internal): k={top_k}, rows {before_rows}->{len(distribution_df)}")
        on_progress(8, 80)

        # 4) write
        inserted = _write_trip_distribution_df(conn, out_distribution_table_name, distribution_df)
        logs.append(f"write.od inserted={inserted}")

        # 运行结束后，用最新 other_od 结果强制覆盖默认历史OD表 trip_distribution_hist
        try:
            with conn.cursor() as cur:
                cur.execute("SET lock_timeout = '2s'")
                cur.execute(
                    f"CREATE TABLE IF NOT EXISTS {default_hist_od_table} (LIKE {out_distribution_table_name} INCLUDING ALL)"
                )
                cur.execute(f"TRUNCATE TABLE {default_hist_od_table}")
                cur.execute(f"INSERT INTO {default_hist_od_table} SELECT * FROM {out_distribution_table_name}")
            ok_hist_end, cnt_hist_end = _table_has_rows(conn, default_hist_od_table)
            logs.append(f"hist.od overwritten after run: table={default_hist_od_table}, ok={int(ok_hist_end)}, rows={cnt_hist_end}")
        except Exception as e:
            logs.append(f"hist.od overwrite after run failed: {e.__class__.__name__}: {e}")
        on_progress(9, 90)

        ret = {
            "status": 1,
            "message": "Success",
            "data": {
                "tables": {
                    "trip_distribution_table": out_distribution_table_name,
                },
                "counts": {"od_rows": inserted},
            },
            "summary": {
                "stage": "trip_distribution",
                "logs": logs,
                "hist_od_used": int(use_hist_flag),
                "hist_od_rows": int(hist_rows),
            },
        }
        on_progress(10, 100)
        return json.dumps(ret, ensure_ascii=False)

    except Exception as e:
        err = f"trip_distribution error: {e.__class__.__name__}: {e}"
        logs.append(err)
        # determine hist rows if available
        try:
            _rows = len(hist_od_df) if hist_od_df is not None else 0
        except Exception:
            _rows = 0
        ret = {
            "status": 0,
            "message": err,
            "data": {
                "tables": {
                    "trip_distribution_table": out_distribution_table_name,
                }
            },
            "summary": {
                "stage": "trip_distribution",
                "logs": logs,
                "hist_od_used": int(_rows > 0),
                "hist_od_rows": int(_rows),
            },
        }
        try:
            on_progress(10, 100)
        except Exception:
            pass
        return json.dumps(ret, ensure_ascii=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Trip distribution runner')
    parser.add_argument('--cfg-json', type=str, required=True, help='Path to cfg.json')
    parser.add_argument('--taz-table-name', type=str, required=True, help='Fully qualified TAZ table name or relies on search_path')
    parser.add_argument('--trip-generation-table-name', type=str, required=True, help='Fully qualified PA table name produced by generation')
    parser.add_argument('--out-csv', type=str, default=None, help='Path to save OD distribution table as CSV (default: dist/other_od.csv)')
    args = parser.parse_args()

    with open(args.cfg_json, 'r', encoding='utf-8') as f:
        cfg = json.load(f)

    def progress_cb(iteration: int, percent: float):
        print(iteration, percent)

    data = fs_run_trip_distribution(
        on_progress=progress_cb,
        cfg=cfg,
        taz_table_name=args.taz_table_name,
        trip_generation_table_name=args.trip_generation_table_name
    )
    print(data)

    # 保存分布结果为 CSV（默认 dist/other_od.csv）
    parent_dir = os.path.dirname(os.path.dirname(__file__))
    dist_dir = os.path.join(parent_dir, 'dist')
    os.makedirs(dist_dir, exist_ok=True)
    if not args.out_csv:
        args.out_csv = os.path.join(dist_dir, 'other_od.csv')

    if args.out_csv:
        try:
            ret = json.loads(data)
            od_table = ret.get('tables', {}).get('trip_distribution_table')
            if od_table:
                conn = _connect(cfg.get('db_conn_str', {}))
                try:
                    with conn.cursor() as cur:
                        cur.execute(f"SELECT f_id,t_id,demand,m_special FROM {od_table} ORDER BY f_id, t_id")
                        rows = cur.fetchall()
                        cols = [d[0] for d in cur.description]
                    df = pd.DataFrame(rows, columns=cols)
                    df.to_csv(args.out_csv, index=False, encoding='utf-8-sig')
                finally:
                    conn.close()
        except Exception:
            pass
