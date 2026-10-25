# 接口契约

- [当前二期 Swagger](../../docs/swagger-interfaces-current.json) 和 [当前二期 API 文档](../../docs/api.md) 是本项目联调入口。
- `legacy-apifox-openapi.json`、`legacy-postman-collection.json` 来自原开发包的一期客户端目录，保留用于接口演变核对。
- 历史契约中的 `localhost:8080` 是示例环境，不能据此替换当前服务器。历史接口与二期不一致时，优先核对 `src/httpapis/` 和二期契约。
- 这些文件描述请求和响应，不包含业务 HTTP 服务端实现。
