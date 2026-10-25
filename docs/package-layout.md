# 发布包内容清单（Layout A）

相对 `out/package/TransNetPro2_*` 的建议内容。启动器与主程序路径约定见 [build-and-env.md](build-and-env.md)。

## 必需

| 路径 | 说明 |
|------|------|
| `bin/tnp2_launcher.exe` | 启动器 |
| `bin/tnp2.env` | 由 `src/launcher/tnp2.env.in` 生成 |
| `bin/tnp2_app.exe` | 主程序 |
| `bin/qt.conf` | `Prefix=..` / `Plugins=plugins`（包根 Qt 插件） |
| `bin/depends/` | Qt/QGIS/GDAL 等运行时 DLL |
| `bin/plugins/` | QGIS providers（`provider_*.dll` 等） |
| `bin/resources/`、`bin/svg/`、`bin/i18n/` | QGIS 包数据（`QGIS_DATA_SUBDIR=.`） |
| `plugins/`（包根） | 仅 Qt 插件（`platforms` 等） |
| `gdalplugins/` | GDAL 驱动 |
| `share/proj`、`share/gdal` | PROJ/GDAL 数据 |

## 建议补齐

| 路径 | 说明 |
|------|------|
| `bin/WebView2/` | 固定 WebView2 运行时（无系统安装时内嵌页可用） |
| `bin/knowledge/` | AI 知识库 markdown |
| `bin/data/` | 业务数据（如人口栅格、`layerstyles.db`） |
| `bin/log4qt.properties` | 日志配置 |

## 勿打入正式包

| 路径 | 原因 |
|------|------|
| `bin/recent_projects.json` | 本机项目痕迹 |
| `bin/data/community_gen_*` | 生成交通小区调试中间 shp |

## 可选瘦身

- `share/proj` 仅保留常用网格
- 大型人口栅格外置或首次下载
- 核对 `depends` 中未链接的重复 GDAL DLL
- 按需裁剪 `bin/i18n` 语言包
