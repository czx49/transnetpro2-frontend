import argparse
import json
import random
import math
import psycopg2
import os
import pandas as pd
import numpy as np


def connect_db(db: dict):
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


def ensure_schema(conn, schema: str):
    with conn.cursor() as cur:
        cur.execute(f"CREATE SCHEMA IF NOT EXISTS {schema}")


def create_taz_table(conn, fq_table: str):
    with conn.cursor() as cur:
        cur.execute(f"""
        CREATE TABLE IF NOT EXISTS {fq_table} (
          id INTEGER PRIMARY KEY,
          pop NUMERIC,
          cars NUMERIC,
          eind NUMERIC,
          eret NUMERIC,
          eoth NUMERIC,
          etot NUMERIC,
          du NUMERIC,
          hinc NUMERIC,
          area NUMERIC,
          longitude NUMERIC,
          latitude NUMERIC,
          geometry geometry(Point,4326),
          created_at TIMESTAMPTZ NOT NULL DEFAULT now()
        )
        """)
        # 兼容已存在但缺少列的表（增加 PostGIS geometry 列）
        try:
            cur.execute(f"ALTER TABLE {fq_table} ADD COLUMN IF NOT EXISTS geometry geometry(Point,4326)")
        except Exception:
            pass
        cur.execute(f"DELETE FROM {fq_table}")


def seed_taz_rows(n: int, lon0: float, lat0: float, dlon: float = 0.02, dlat: float = 0.02):
    rows = []
    side = int(math.ceil(math.sqrt(n)))
    idx = 0
    for i in range(side):
        for j in range(side):
            if idx >= n:
                break
            pop = random.randint(500, 5000)
            cars = int(pop * random.uniform(0.2, 0.8))
            eind = random.randint(100, 5000)
            eret = random.randint(50, 2000)
            eoth = random.randint(50, 2000)
            etot = eind + eret + eoth
            du = random.randint(200, 4000)
            hinc = random.uniform(3000, 12000)
            area = random.uniform(0.5, 10.0)
            longitude = lon0 + (j - side/2) * dlon + random.uniform(-0.002, 0.002)
            latitude = lat0 + (i - side/2) * dlat + random.uniform(-0.002, 0.002)
            rows.append((idx + 1, pop, cars, eind, eret, eoth, etot, du, hinc, area, longitude, latitude))
            idx += 1
        if idx >= n:
            break
    return rows


def insert_taz(conn, fq_table: str, rows):
    with conn.cursor() as cur:
        rows_with_geom = [r + (r[-2], r[-1]) for r in rows]  # 追加经纬度以构造 geometry(Point,4326)
        cur.executemany(
            f"INSERT INTO {fq_table}(id,pop,cars,eind,eret,eoth,etot,du,hinc,area,longitude,latitude,geometry) "
            f"VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s, ST_SetSRID(ST_MakePoint(%s,%s),4326))",
            rows_with_geom,
        )


def create_hist_od_table(conn, fq_table: str):
    with conn.cursor() as cur:
        cur.execute(f"""
        CREATE TABLE IF NOT EXISTS {fq_table} (
          f_id INTEGER NOT NULL,
          t_id INTEGER NOT NULL,
          demand NUMERIC
        )
        """)
        cur.execute(f"DELETE FROM {fq_table}")


def taz_cost_from_df(df: pd.DataFrame, diag_eps: float) -> np.ndarray:
    R = 6371.0
    lat = np.radians(df["latitude"].to_numpy(dtype=float))
    lon = np.radians(df["longitude"].to_numpy(dtype=float))
    dlat = lat[:, None] - lat[None, :]
    dlon = lon[:, None] - lon[None, :]
    a = np.sin(dlat / 2.0) ** 2 + np.cos(lat[:, None]) * np.cos(lat[None, :]) * np.sin(dlon / 2.0) ** 2
    c = 2 * np.arcsin(np.sqrt(a))
    C = R * c
    if diag_eps is not None and diag_eps > 0:
        np.fill_diagonal(C, diag_eps)
    return C


def gravity_dc(P: np.ndarray, A: np.ndarray, C: np.ndarray, lam: float, max_iter: int, tol: float) -> np.ndarray:
    n = len(P)
    F = np.exp(-lam * C)
    F[F < 1e-12] = 1e-12
    alpha = np.ones(n, dtype=float)
    beta = np.ones(n, dtype=float)
    for _ in range(max_iter):
        denom_alpha = (beta * A)[None, :] * F
        denom_alpha = denom_alpha.sum(axis=1)
        alpha_new = np.where(denom_alpha > 0, 1.0 / denom_alpha, 0.0)
        denom_beta = (alpha_new * P)[:, None] * F
        denom_beta = denom_beta.sum(axis=0)
        beta_new = np.where(denom_beta > 0, 1.0 / denom_beta, 0.0)
        if max(np.max(np.abs(alpha_new - alpha)), np.max(np.abs(beta_new - beta))) < tol:
            alpha = alpha_new
            beta = beta_new
            break
        alpha = alpha_new
        beta = beta_new
    T = (alpha[:, None] * P[:, None]) * (beta[None, :] * A[None, :]) * F
    return T


def insert_hist_od(conn, fq_table: str, T: np.ndarray, zone_ids: np.ndarray):
    n = T.shape[0]
    f_ids = np.repeat(zone_ids, n)
    t_ids = np.tile(zone_ids, n)
    demand = T.reshape(-1)
    with conn.cursor() as cur:
        rows = list(zip(f_ids.tolist(), t_ids.tolist(), demand.astype(float).tolist()))
        cur.executemany(
            f"INSERT INTO {fq_table}(f_id,t_id,demand) VALUES (%s,%s,%s)",
            rows,
        )


def main():
    parser = argparse.ArgumentParser(description="Generate test TAZ data and write to PostgreSQL")
    parser.add_argument("--cfg-json", required=True, help="Path to cfg.json containing db_conn_str")
    parser.add_argument("--schema", default=None, help="Schema name, default from cfg db_conn_str.role")
    parser.add_argument("--table", default="taz_socioeconomic", help="TAZ base table name (without schema)")
    parser.add_argument("--n", type=int, default=16, help="Number of zones to generate")
    parser.add_argument("--lon0", type=float, default=104.06, help="Center longitude")
    parser.add_argument("--lat0", type=float, default=30.67, help="Center latitude")
    parser.add_argument("--out-csv", default=None, help="Path to save generated CSV (default: dist/<table>.csv)")
    parser.add_argument("--hist-od-table", default=None, help="Fully qualified hist OD table name; default: <role>.<prefix>trip_distribution_hist")
    parser.add_argument("--hist-lambda", type=float, default=0.1, help="Lambda for generating hist OD")
    parser.add_argument("--hist-diag-eps", type=float, default=0.1, help="Diagonal epsilon for cost matrix")
    parser.add_argument("--hist-max-iter", type=int, default=20, help="Max iterations for gravity")
    parser.add_argument("--hist-tol", type=float, default=1e-6, help="Tolerance for gravity")
    args = parser.parse_args()

    with open(args.cfg_json, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    db = cfg.get("db_conn_str", {})

    schema = args.schema or db.get("role", "user_project")
    fq_table = f"{schema}.{args.table}"

    conn = connect_db(db)
    try:
        try:
            ensure_schema(conn, schema)
        except Exception:
            pass
        create_taz_table(conn, fq_table)
        rows = seed_taz_rows(args.n, args.lon0, args.lat0)
        insert_taz(conn, fq_table, rows)
        parent_dir = os.path.dirname(os.path.dirname(__file__))
        dist_dir = os.path.join(parent_dir, 'dist')
        os.makedirs(dist_dir, exist_ok=True)
        out_path = args.out_csv or os.path.join(dist_dir, f"{args.table}.csv")
        cols = ["id","pop","cars","eind","eret","eoth","etot","du","hinc","area","longitude","latitude"]
        df = pd.DataFrame(rows, columns=cols)
        # 为 CSV 加一列 PostGIS geometry(Point,4326) 字面量，形如 "SRID=4326;POINT(lon lat)"
        df["geometry"] = df.apply(lambda r: f"SRID=4326;POINT({r['longitude']} {r['latitude']})", axis=1)
        df.to_csv(out_path, index=False, encoding='utf-8-sig')
        print(f"OK: inserted {len(rows)} rows into {fq_table}")

        role = db.get("role", schema)
        prefix = db.get("scenario_prefix", "")
        sep = "" if (not prefix or prefix.endswith("_")) else "_"
        hist_od_table = args.hist_od_table or f"{role}.{prefix}{sep}other_trip_distribution_hist"
        hist_schema = hist_od_table.split(".")[0] if "." in hist_od_table else role
        try:
            ensure_schema(conn, hist_schema)
        except Exception:
            pass
        create_hist_od_table(conn, hist_od_table)
        taz_df = pd.DataFrame(rows, columns=cols)
        taz_df = taz_df.sort_values("id").reset_index(drop=True)
        C = taz_cost_from_df(taz_df, diag_eps=args.hist_diag_eps)
        n = len(taz_df)
        P = np.ones(n, dtype=float)
        A = np.ones(n, dtype=float)
        T = gravity_dc(P, A, C, lam=args.hist_lambda, max_iter=args.hist_max_iter, tol=args.hist_tol)
        insert_hist_od(conn, hist_od_table, T, taz_df["id"].to_numpy())
        print(f"OK: inserted {n*n} rows into {hist_od_table}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
