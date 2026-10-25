#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
批量导出 PostgreSQL 表为 CSV（包含表头），用于导出 OD 估计三张结果表：
- <schema>.<prefix>od_estimation_results
- <schema>.<prefix>link_flow_results
- <schema>.<prefix>iteration_record

可选导出：网络/OD/观测输入表（传入完整表名）。

依赖：
  pip install psycopg2-binary

示例：
  python export_results.py \
    --host 127.0.0.1 --port 5432 --dbname postgres \
    --user uproject --password uproject753159 \
    --schema user_project --prefix project59_user1_ \
    --output-dir exports \
    --network-table user_project.project59_user1_road_way \
    --od-table user_project.project59_user1_other_od \
    --observed-table user_project.project59_user1_other_observation

运行后将在输出目录生成：
  user_project.project59_user1_od_estimation_results.csv
  user_project.project59_user1_link_flow_results.csv
  user_project.project59_user1_iteration_record.csv
  （以及可选的 network/od/observed 表 CSV）
"""
import argparse
import os
import sys
import time
from datetime import datetime

try:
    import psycopg2
    from psycopg2 import sql
except ImportError:
    print("需要安装依赖: pip install psycopg2-binary", file=sys.stderr)
    raise


def parse_fq_table(name: str):
    """解析 'schema.table' -> (schema, table)。允许带引号/空白。"""
    s = name.strip().strip('"')
    if '.' not in s:
        raise ValueError(f"表名需要是 schema.table 形式: {name}")
    schema, table = s.split('.', 1)
    return schema.strip('"'), table.strip('"')


def export_table(conn, schema: str, table: str, out_path: str):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with conn.cursor() as cur, open(out_path, 'w', encoding='utf-8', newline='') as f:
        # 采用 COPY TO STDOUT WITH CSV HEADER
        composed = sql.SQL("COPY {} TO STDOUT WITH (FORMAT csv, HEADER true)").format(
            sql.Identifier(schema, table)
        )
        cur.copy_expert(composed.as_string(conn), f)


def main():
    ap = argparse.ArgumentParser(description="导出 PostgreSQL 结果表为 CSV")
    ap.add_argument('--host', default='127.0.0.1')
    ap.add_argument('--port', type=int, default=5432)
    ap.add_argument('--dbname', required=True)
    ap.add_argument('--user', required=True)
    ap.add_argument('--password', required=True)
    ap.add_argument('--schema', required=True, help='schema，例如 user_project')
    ap.add_argument('--prefix', required=True, help='结果表前缀，例如 project59_user1_')
    ap.add_argument('--output-dir', default='exports', help='输出目录')
    # 可选：输入数据表
    ap.add_argument('--network-table')
    ap.add_argument('--od-table')
    ap.add_argument('--observed-table')
    # 额外自由列表（逗号分隔的 schema.table 列表）
    ap.add_argument('--extra-tables', help='额外导出的表，逗号分隔，例如: foo.bar,foo.baz')

    args = ap.parse_args()

    dsn = f"host={args.host} port={args.port} dbname={args.dbname} user={args.user} password={args.password}"

    # 结果表集合
    results_suffix = [
        'od_estimation_results',
        'link_flow_results',
        'iteration_record',
    ]

    targets = []
    for suf in results_suffix:
        targets.append((args.schema, f"{args.prefix}{suf}"))

    # 附加可选的输入表
    for opt_name in ('network_table', 'od_table', 'observed_table'):
        val = getattr(args, opt_name)
        if val:
            sch, tbl = parse_fq_table(val)
            targets.append((sch, tbl))

    # 附加 extra-tables
    if args.extra_tables:
        for itm in args.extra_tables.split(','):
            sch, tbl = parse_fq_table(itm)
            targets.append((sch, tbl))

    # 去重
    seen = set()
    final_targets = []
    for sch, tbl in targets:
        key = (sch, tbl)
        if key in seen:
            continue
        seen.add(key)
        final_targets.append((sch, tbl))

    os.makedirs(args.output_dir, exist_ok=True)

    t0 = time.time()
    with psycopg2.connect(dsn) as conn:
        with conn.cursor() as cur:
            cur.execute("SET client_encoding TO 'UTF8'")
        for sch, tbl in final_targets:
            out_name = f"{sch}.{tbl}.csv"
            out_path = os.path.join(args.output_dir, out_name)
            print(f"导出 {sch}.{tbl} -> {out_path}")
            export_table(conn, sch, tbl, out_path)
    dt = time.time() - t0
    print(f"完成导出 {len(final_targets)} 张表，用时 {dt:.2f}s，输出目录: {args.output_dir}")


if __name__ == '__main__':
    main()
