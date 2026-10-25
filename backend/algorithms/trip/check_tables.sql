BEGIN;

-- 检查四阶段法运行结束后：PA表 / other_od表 / 历史OD表 是否成功生成（存在、行数、created_at最大值）
DO $$
DECLARE
  sch text := 'user_project';
  pref text := 'project72_user1';
  sep  text := CASE WHEN pref = '' OR right(pref, 1) = '_' THEN '' ELSE '_' END;

  suffixes text[] := ARRAY[
    'other_trip_production_attraction',
    'other_od',
    'other_trip_distribution_hist'
  ];

  sfx text;
  tbl_name text;
  fq_tbl text;
  reg regclass;
  cnt bigint;
  has_created_at boolean;
  max_created timestamptz;
BEGIN
  CREATE TEMP TABLE IF NOT EXISTS tmp_check_results (
    table_name text,
    exists_in_db boolean,
    row_count bigint,
    has_created_at boolean,
    max_created_at timestamptz
  ) ON COMMIT DROP;

  TRUNCATE TABLE tmp_check_results;

  FOREACH sfx IN ARRAY suffixes LOOP
    tbl_name := pref || sep || sfx;
    fq_tbl := sch || '.' || tbl_name;
    reg := to_regclass(fq_tbl);

    IF reg IS NULL THEN
      INSERT INTO tmp_check_results(table_name, exists_in_db, row_count, has_created_at, max_created_at)
      VALUES (fq_tbl, false, NULL, NULL, NULL);
    ELSE
      EXECUTE format('SELECT count(*) FROM %s', fq_tbl) INTO cnt;

      SELECT EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = sch
          AND table_name = tbl_name
          AND column_name = 'created_at'
      ) INTO has_created_at;

      IF has_created_at THEN
        EXECUTE format('SELECT max(created_at) FROM %s', fq_tbl) INTO max_created;
      ELSE
        max_created := NULL;
      END IF;

      INSERT INTO tmp_check_results(table_name, exists_in_db, row_count, has_created_at, max_created_at)
      VALUES (fq_tbl, true, cnt, has_created_at, max_created);
    END IF;
  END LOOP;
END $$;

SELECT *
FROM tmp_check_results
ORDER BY table_name;

COMMIT;


SELECT * FROM user_project.project72_user1_other_trip_production_attraction