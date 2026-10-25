import os
import sys
import json
import psycopg2

# 获取项目根目录
PROJECT_ROOT = os.path.dirname(os.path.dirname(__file__))
CFG_JSON_PATH = os.path.join(PROJECT_ROOT, "tools", "cfg.json")

def load_config():
    with open(CFG_JSON_PATH, "r", encoding="utf-8") as f:
        return json.load(f)

def connect_db(cfg):
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

def get_table_names(cfg):
    db = cfg.get("db_conn_str", {})
    role = db.get("role", "user_project")
    prefix = db.get("scenario_prefix", "")

    sep = "" if prefix.endswith("_") else "_"

    od_table = f"{role}.{prefix}{sep}other_od"
    hist_od_table = f"{role}.{prefix}{sep}other_trip_distribution_hist"
    return od_table, hist_od_table

def check_table_exists(conn, table_name):
    schema, table = table_name.split(".")
    with conn.cursor() as cur:
        cur.execute("""
            SELECT EXISTS (
                SELECT FROM information_schema.tables
                WHERE table_schema = %s AND table_name = %s
            )
        """, (schema, table))
        return cur.fetchone()[0]

def check_table_has_rows(conn, table_name):
    try:
        with conn.cursor() as cur:
            cur.execute(f"SELECT COUNT(*) FROM {table_name}")
            count = cur.fetchone()[0]
            return True, count
    except Exception as e:
        print(f"Error checking table {table_name}: {e}")
        return False, 0

def copy_od_to_hist(conn, od_table, hist_od_table):
    try:
        with conn.cursor() as cur:
            # 创建历史OD表（如果不存在）
            cur.execute(f"""
            CREATE TABLE IF NOT EXISTS {hist_od_table} (
              f_id INTEGER NOT NULL,
              t_id INTEGER NOT NULL,
              demand DOUBLE PRECISION NOT NULL
            )
            """)

            # 清空历史OD表
            cur.execute(f"DELETE FROM {hist_od_table}")

            # 从OD表复制数据到历史OD表
            cur.execute(
                f"INSERT INTO {hist_od_table}(f_id, t_id, demand) "
                f"SELECT f_id, t_id, demand FROM {od_table}"
            )

            # 检查结果
            ok, count = check_table_has_rows(conn, hist_od_table)
            if ok:
                print(f"成功复制数据到历史OD表: {hist_od_table}, 行数: {count}")
            else:
                print(f"复制完成但历史OD表为空: {hist_od_table}")

            return ok, count
    except Exception as e:
        print(f"复制数据到历史OD表失败: {e}")
        return False, 0

def main():
    # 加载配置
    cfg = load_config()

    # 连接数据库
    conn = connect_db(cfg)

    try:
        # 获取表名
        od_table, hist_od_table = get_table_names(cfg)

        print(f"OD表: {od_table}")
        print(f"历史OD表: {hist_od_table}")

        # 检查OD表是否存在并有数据
        od_exists = check_table_exists(conn, od_table)
        if not od_exists:
            print(f"错误: OD表不存在: {od_table}")
            return 1

        od_ok, od_count = check_table_has_rows(conn, od_table)
        if not od_ok or od_count == 0:
            print(f"错误: OD表为空或无法访问: {od_table}")
            return 1

        print(f"OD表行数: {od_count}")

        # 复制OD表数据到历史OD表
        hist_ok, hist_count = copy_od_to_hist(conn, od_table, hist_od_table)

        if hist_ok:
            print("操作成功完成!")
            return 0
        else:
            print("操作失败!")
            return 1
    finally:
        conn.close()

if __name__ == "__main__":
    sys.exit(main())
