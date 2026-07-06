# 数据库模块 — MySQL 封装与连接池

## 源文件

| 组件 | 头文件 | 实现目录 |
|------|-------|---------|
| MySQL 封装 | [include/server/db/db.h](../include/server/db/db.h) | `src/server/db/` |
| 连接池 | [include/server/db/ConnectionPool.hpp](../include/server/db/ConnectionPool.hpp) | `src/server/db/` |

## 职责

- **MySQL 类**：封装 MySQL C API，提供连接、查询、更新的底层接口
- **ConnectionPool 类**：管理 MySQL 连接池，提供连接获取/归还的生产者-消费者模型

## MySQL 类

### 核心接口

| 方法 | 说明 |
|------|------|
| `bool connect()` | 使用默认参数连接（依赖 Config 单例） |
| `bool connect(host, port, user, password, dbname)` | 指定参数连接 |
| `bool update(const string& sql)` | 执行 UPDATE/INSERT/DELETE，返回是否成功 |
| `MYSQL_RES* query(const string& sql)` | 执行 SELECT，返回结果集（调用方负责 `mysql_free_result`） |
| `MYSQL* getConnection()` | 获取底层 `MYSQL*` 句柄 |
| `string escape(const string& str)` | 调用 `mysql_real_escape_string` 转义字符串，防 SQL 注入 |

### 安全设计

`escape()` 方法封装了 `mysql_real_escape_string()`，所有 Model 层在构建带用户输入的 SQL 时使用该方法：

```cpp
std::string escaped = conn->escape(userInput);
```

## ConnectionPool — 连接池

### 设计模式

**单例模式** + **生产者-消费者模型**：

```
连接池 (_pool: queue<MySQL*>)
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ C1 │ C2 │ C3 │ C4 │ C5 │ C6 │ C7 │ C8 │  (默认 poolSize=8)
└────┴────┴────┴────┴────┴────┴────┴────┘
         ▲                        │
         │  getConnection()       │  returnConnection()
         │  (池空则 wait)          │  (唤醒等待者)
         │                        ▼
    ┌─────────┐              ┌─────────┐
    │ Model A │              │ Model B │
    └─────────┘              └─────────┘
```

### 核心接口

| 方法 | 说明 |
|------|------|
| `static ConnectionPool& instance()` | 获取单例 |
| `void init(host, port, user, password, dbname, poolSize=8)` | 初始化连接池，创建 poolSize 个连接 |
| `shared_ptr<MySQL> getConnection()` | 获取连接。池空时阻塞等待（条件变量 `_cv`） |
| `void returnConnection(MySQL* conn)` | 归还连接到池，唤醒等待线程 |

### RAII 连接回收

`getConnection()` 返回 `shared_ptr<MySQL>`，使用自定义 deleter：

```cpp
shared_ptr<MySQL> getConnection() {
    unique_lock<mutex> lock(_mutex);
    while (_pool.empty()) {
        _cv.wait(lock);  // 条件变量阻塞等待
    }
    MySQL* conn = _pool.front();
    _pool.pop();
    // shared_ptr 析构时自动调用 returnConnection，而非 delete
    return shared_ptr<MySQL>(conn, [this](MySQL* c) { this->returnConnection(c); });
}
```

调用方无需手动归还连接，`shared_ptr` 离开作用域时自动归还。

### 生命周期

1. `main()` 中调用 `ConnectionPool::instance().init(...)` 初始化
2. 服务运行期间，各 Model 通过 `getConnection()` 获取连接
3. 连接池析构时关闭并释放所有连接

## 线程安全

- `_mutex` + `_cv` 条件变量保护连接队列的并发访问
- 池空时获取方自动阻塞，连接归还时自动唤醒
- 每个 Model 操作获取独立连接，操作期间无锁竞争
