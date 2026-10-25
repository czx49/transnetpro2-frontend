BEGIN;

-- 你的schema/前缀
DO $$
DECLARE
  sch text := 'user_project';
  pref text := 'project63_user1_case41';
  sep  text := CASE WHEN pref = '' OR right(pref, 1) = '_' THEN '' ELSE '_' END;
BEGIN
  -- 生成阶段输出：PA
  EXECUTE format('DROP TABLE IF EXISTS %I.%I CASCADE', sch, pref || sep || 'other_trip_production_attraction');

  -- 历史OD（如果你也想删掉，让算法重新生成/覆盖）
  EXECUTE format('DROP TABLE IF EXISTS %I.%I CASCADE', sch, pref || sep || 'other_trip_distribution_hist');

  -- 你若还想清理TAZ输入（谨慎！一般不建议删输入表）
  -- EXECUTE format('DROP TABLE IF EXISTS %I.%I CASCADE', sch, pref || sep || 'other_taz_socioeconomic');

END $$;

COMMIT;