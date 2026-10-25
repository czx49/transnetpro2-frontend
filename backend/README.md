# 后端联调与算法资产

团队负责前端优化，同时需要保留真实后端的数据读取、算法调用、导入导出与结果展示链路。本目录保存开发包中已有的算法源码、调用示例和接口契约，供前后端协作使用。

## 服务与接口

| 内容 | 位置 |
|------|------|
| 当前业务 API | `http://gisapi.jiheshop.com`，由 `config/httpserver.ini.example` 提供默认地址 |
| WFS 地图与属性数据 | 同一服务器的 `/wfs` |
| 当前二期 API 说明 | [docs/api.md](../docs/api.md) |
| 当前二期 Swagger | [docs/swagger-interfaces-current.json](../docs/swagger-interfaces-current.json) |
| C++ HTTP、登录与 SSE 客户端 | [src/httpapis](../src/httpapis/) |
| 响应模型及字段解析 | [src/jsonreader](../src/jsonreader/) |
| 一期接口契约与 Postman 参考 | [contracts](contracts/)；以二期接口和客户端实现为准 |

登录、项目、方案、WFS、导入导出和算法请求继续使用真实后端。无需为前端开发另建空数据库。

## 开发包中已有的算法内容

| 目录 | 已纳入的内容 | 运行依赖 |
|------|--------------|----------|
| `algorithms/trip/` | 出行生成、出行分布 Python 实现，嵌入式 Python C++ 封装，测试工具、SQL 辅助脚本和说明 | Python、数据分析/数据库依赖、匹配的数据库结构；DLL 封装另需原生构建环境 |
| `algorithms/ctap/` | 交通分配 DLL 调用示例、回调和结果字段文档 | 开发包中的匹配 DLL |
| `algorithms/greedy/` | 算法驱动调用示例、配置样例 | 开发包中的匹配 DLL |
| `algorithms/odtna/` | OD 估计与容量分析 DLL 调用、配置及导出示例 | 开发包中的匹配 DLL |

导入来源为同级 `tnpsource_developmentKit/transnetproCode/out/bin/algorithm/`。来源文件未被修改；仓库副本已把发现的密码等凭据常量替换为 `<CONFIGURE_LOCALLY>`，配置 JSON 以 `.json.example` 后缀保存。使用前复制为本机配置并填写开发环境参数。

这些算法工具中有写入、清理、恢复数据库表的操作。联调时使用专门的开发数据库和测试项目，按各模块原有说明配置，不直接运行全部脚本作为验收测试。

完整导入文件及本地原生程序清单见 [source-inventory.json](source-inventory.json)。原生 DLL/EXE、嵌入式 Python 运行库、生成的业务结果和真实数据库配置继续留在开发包中。

Python 源码所需的外部依赖见 [algorithms/requirements.txt](algorithms/requirements.txt)；集成 DLL 时仍需与原生模块绑定的 Python 运行时匹配。当前只完成源码语法检查，没有执行会写入数据库的算法。

## 仍需开发方提供的资料

现有开发包包含客户端、部分算法源码和原生运行库，没有找到业务 HTTP 服务端的完整实现，也没有完整的业务数据库初始化/迁移。因此本目录不能单独部署一个完整业务后端。

如需独立部署或修改服务端，需要补充业务服务端源码/部署包、完整数据库结构与初始化资料、与当前 DLL 对应的原生算法源码及服务部署配置。不要把测试 HTTP 服务器或已停止的离线实现当成业务服务端。

## 数据读取验收

验收应依次覆盖：登录 → 项目/方案详情 → WFS 服务目录 → 实际图层和属性 → 算法结果。公开 API 可访问只说明第一部分网络链路正常。

`tests/networkproxy` 包含 Qt 与 QGIS 两条链路的代理回归测试。给定有效的测试图层名后，可以检查真实 WFS provider 的有效性、字段及首条要素读取；不会运行写入型算法。

2026-10-25 已通过真实节点、方案路段和基础小区图层读取检查，分别识别 7、25、16 个字段，并读到首条要素；应用本身的 WFS 服务目录预热也已验证。此结果覆盖数据读取链路，不替代全部业务和算法验收。
