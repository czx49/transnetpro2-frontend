CREATE SCHEMA IF NOT EXISTS tnp2_local;
CREATE SCHEMA IF NOT EXISTS tnp2_data;
CREATE TABLE IF NOT EXISTS tnp2_local.projects (
 id bigserial PRIMARY KEY,name text NOT NULL CHECK(length(trim(name))>0),description text NOT NULL DEFAULT '',
 province_id integer NOT NULL DEFAULT 0,province text NOT NULL DEFAULT '',city_id integer NOT NULL DEFAULT 0,city text NOT NULL DEFAULT '',
 created_at timestamptz NOT NULL DEFAULT now(),updated_at timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE IF NOT EXISTS tnp2_local.cases (
 id bigserial PRIMARY KEY,project_id bigint NOT NULL REFERENCES tnp2_local.projects(id) ON DELETE CASCADE,
 name text NOT NULL CHECK(length(trim(name))>0),case_type_id integer NOT NULL DEFAULT 1,isbase integer NOT NULL DEFAULT 0,
 status integer NOT NULL DEFAULT 0,road_types jsonb NOT NULL DEFAULT '[]',bus_types jsonb NOT NULL DEFAULT '[]',other_types jsonb NOT NULL DEFAULT '[]',
 result text NOT NULL DEFAULT '',flags jsonb NOT NULL DEFAULT '{}',created_at timestamptz NOT NULL DEFAULT now(),updated_at timestamptz NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS one_base_case_per_project ON tnp2_local.cases(project_id) WHERE isbase=1;
CREATE TABLE IF NOT EXISTS tnp2_local.data_tables (
 project_id bigint NOT NULL REFERENCES tnp2_local.projects(id) ON DELETE CASCADE,case_id bigint NOT NULL DEFAULT 0,
 logical_name text NOT NULL,physical_name text NOT NULL UNIQUE,display_name text NOT NULL,category text NOT NULL,
 PRIMARY KEY(project_id,case_id,logical_name)
);
CREATE TABLE IF NOT EXISTS tnp2_local.edit_log (
 id bigserial PRIMARY KEY,case_id bigint NOT NULL REFERENCES tnp2_local.cases(id) ON DELETE CASCADE,
 details jsonb NOT NULL DEFAULT '{}',created_at timestamptz NOT NULL DEFAULT now()
);
INSERT INTO tnp2_local.projects(name,description)
 SELECT '本地离线工程','二期离线工程，等待导入业务数据' WHERE NOT EXISTS(SELECT 1 FROM tnp2_local.projects);
