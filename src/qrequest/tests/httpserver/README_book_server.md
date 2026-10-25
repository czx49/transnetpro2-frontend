# 图书管理HTTP测试服务器

## 概述

这是一个基于Python的HTTP测试服务器，专门为测试qrequest库的各种HTTP方法而设计。服务器模拟了一个图书管理系统，提供完整的RESTful API接口。

## 功能特性

- ✅ 支持所有主要HTTP方法 (GET, POST, PUT, DELETE, OPTIONS)
- ✅ 完整的图书管理CRUD操作
- ✅ JSON格式的请求和响应
- ✅ 错误处理和状态码返回
- ✅ 跨域支持 (CORS)
- ✅ 线程安全的数据管理
- ✅ 图书搜索功能
- ✅ 健康检查接口

## 快速开始

### 1. 启动服务器

#### Windows用户：
```bash
# 双击运行
start_book_server.bat

# 或在命令行中运行
start_book_server.bat
```

#### 其他系统：
```bash
python book_server.py
```

### 2. 服务器信息
- **默认地址**: http://localhost:8888
- **编码**: UTF-8
- **数据格式**: JSON

## API接口文档

### 基础响应格式
所有API响应都采用统一格式：
```json
{
  "status": 200,
  "message": "成功",
  "timestamp": "2024-01-01T12:00:00.000000",
  "data": {...}
}
```

### 1. 获取所有图书
**GET** `/books`

**响应示例：**
```json
{
  "status": 200,
  "message": "获取到 5 本图书",
  "timestamp": "2024-01-01T12:00:00.000000",
  "data": [
    {
      "id": "uuid-string",
      "title": "Python编程：从入门到实践",
      "author": "埃里克·马瑟斯",
      "isbn": "978-7-115-42802-8",
      "publish_date": "2016-07-01",
      "created_at": "2024-01-01T12:00:00.000000",
      "updated_at": "2024-01-01T12:00:00.000000"
    }
  ]
}
```

### 2. 搜索图书
**GET** `/books?search=关键词`

**查询参数：**
- `search`: 搜索关键词（在标题、作者、ISBN中搜索）

### 3. 获取指定图书
**GET** `/books/{id}`

**路径参数：**
- `id`: 图书ID

### 4. 创建新图书
**POST** `/books`

**请求体：**
```json
{
  "title": "新图书标题",
  "author": "作者姓名",
  "isbn": "978-x-xxx-xxxxx-x",
  "publish_date": "2024-01-01"  // 可选，默认为当前日期
}
```

### 5. 更新图书
**PUT** `/books/{id}`

**请求体：**
```json
{
  "title": "更新后的标题",
  "author": "更新后的作者",
  "isbn": "更新后的ISBN",
  "publish_date": "2024-01-01"
}
```

### 6. 删除图书
**DELETE** `/books/{id}`

### 7. 健康检查
**GET** `/health`

**响应示例：**
```json
{
  "status": 200,
  "message": "服务器运行正常",
  "data": {
    "status": "healthy",
    "server": "Book Management Server",
    "version": "1.0.0",
    "uptime": 1234567890
  }
}
```

## 测试示例

### 使用curl测试

#### 1. 获取所有图书
```bash
curl -X GET http://localhost:8888/books
```

#### 2. 搜索图书
```bash
curl -X GET "http://localhost:8888/books?search=Python"
```

#### 3. 创建新图书
```bash
curl -X POST http://localhost:8888/books \
  -H "Content-Type: application/json" \
  -d "{\"title\":\"新图书\",\"author\":\"测试作者\",\"isbn\":\"978-1-234-56789-0\"}"
```

#### 4. 更新图书
```bash
curl -X PUT http://localhost:8888/books/{book_id} \
  -H "Content-Type: application/json" \
  -d "{\"title\":\"更新的标题\"}"
```

#### 5. 删除图书
```bash
curl -X DELETE http://localhost:8888/books/{book_id}
```

### 使用qrequest测试

这个服务器专门为测试qrequest库而设计，你可以在C++代码中这样使用：

```cpp
#include "qrequest2.h"

// 获取所有图书
QRequest request("http://localhost:8888/books");
request.get([](QNetworkReply* reply) {
    qDebug() << reply->readAll();
});

// 创建新图书
QJsonObject bookData;
bookData["title"] = "新图书";
bookData["author"] = "测试作者";
bookData["isbn"] = "978-1-234-56789-0";

QRequest createRequest("http://localhost:8888/books");
createRequest.post(bookData, [](QNetworkReply* reply) {
    qDebug() << "创建结果:" << reply->readAll();
});
```

## 状态码说明

| 状态码 | 说明 |
|--------|------|
| 200 | 成功 |
| 201 | 创建成功 |
| 400 | 请求参数错误 |
| 404 | 资源未找到 |
| 405 | 方法不允许 |
| 500 | 服务器内部错误 |

## 初始测试数据

服务器启动时会自动创建以下测试图书：

1. Python编程：从入门到实践 - 埃里克·马瑟斯
2. 深入理解计算机系统 - 兰德尔·E.布莱恩特
3. 算法导论 - 托马斯·H.科尔门
4. 设计模式 - Erich Gamma
5. 重构：改善既有代码的设计 - 马丁·福勒

## 注意事项

1. **数据持久性**: 服务器重启后数据会重置为初始状态
2. **并发安全**: 服务器支持多线程并发访问
3. **编码**: 所有输入输出都使用UTF-8编码
4. **跨域**: 已启用CORS支持，可从浏览器直接访问

## 故障排除

### 端口占用
如果8888端口被占用，可以修改`book_server.py`中的`PORT`变量：
```python
PORT = 9999  # 改为其他端口
```

### Python版本要求
- 最低要求：Python 3.6+
- 推荐版本：Python 3.8+

### 字符编码问题
如果在Windows下遇到字符编码问题，请确保：
1. 终端支持UTF-8编码
2. 使用`start_book_server.bat`启动（已设置编码）

## 开发信息

- **版本**: 1.0.0
- **作者**: qrequest测试团队
- **许可**: MIT License
- **依赖**: 仅使用Python标准库，无需额外安装包

---

**快速测试命令：**
```bash
# 启动服务器
python book_server.py

# 在另一个终端测试
curl http://localhost:8888/health
```