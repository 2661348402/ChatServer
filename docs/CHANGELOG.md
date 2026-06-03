# Changelog


## v1.0.0 — 基础功能原型 (2026-06-02)

### 概述

基于 Muduo 网络库实现的分布式集群聊天服务器初始版本，具备完整的聊天系统基本能力。

### 技术栈

| 组件 | 技术 |
|------|------|
| 网络库 | Muduo（Reactor 模型） |
| 数据库 | MySQL 8.0 |
| 缓存 | Redis（Pub/Sub 跨服务器通信） |
| 序列化 | nlohmann/json |
| 负载均衡 | Nginx TCP Stream |
| 构建系统 | CMake |

### 功能

| 功能 | 说明 |
|------|------|
| 用户注册 | 用户名 + 密码注册，分配唯一 ID |
| 用户登录 | ID + 密码验证，返回好友列表、群组列表、离线消息 |
| 私聊（一对一） | 在线即时送达，离线存入 MySQL |
| 群聊 | 创建群组、加入群组、群内广播消息 |
| 离线消息 | 接收方离线时消息存入 `OfflineMessage` 表，登录时推送 |
| 好友管理 | 添加好友（单向） |
| 退出登录 | 断开连接，状态切换 |

### 架构

```
┌──────────────┐    TCP     ┌─────────────────────────────┐
│  CLI Client  │ ─────────→ │  Nginx (TCP Load Balancer)  │
│  (C++)       │            └─────────────────────────────┘
└──────────────┘                      │
                              ┌───────┼───────┐
                              ▼       ▼       ▼
                         ┌──────────────────────────────┐
                         │     ChatServer (Muduo)       │
                         │  ┌────────────────────────┐  │
                         │  │   ChatService           │  │
                         │  │   (消息分发 & 业务逻辑)  │  │
                         │  ├────────────────────────┤  │
                         │  │   Model Layer           │  │
                         │  │   UserModel · GroupModel │  │
                         │  │   FriendModel            │  │
                         │  │   OfflineMessageModel    │  │
                         │  ├────────────────────────┤  │
                         │  │   Data Layer             │  │
                         │  │   MySQL · Redis          │  │
                         │  └────────────────────────┘  │
                         └──────────────────────────────┘
```

### 消息协议

基于 JSON 文本协议，所有消息包含 `msgId` 字段：

| msgId | 类型 | 说明 |
|-------|------|------|
| 1 | `LOGIN_MSG` | 登录请求 |
| 2 | `LOGIN_ACK` | 登录响应 |
| 3 | `REG_MSG` | 注册请求 |
| 4 | `REG_ACK` | 注册响应 |
| 5 | `ONE_CHAT_MSG` | 私聊消息 |
| 6 | `ADD_FRIEND_MSG` | 添加好友 |
| 7 | `CREATE_GROUP_MSG` | 创建群组 |
| 8 | `ADD_GROUP_MSG` | 加入群组 |
| 9 | `GROUP_CHAT_MSG` | 群聊消息 |
| 10 | `LOGIN_OUT_MSG` | 退出登录 |

### 数据库表

- **user** — id, name, password, state
- **friend** — userid, friendid
- **offlinemessage** — userid, message
- **allgroup** — id, groupname, groupdesc
- **groupuser** — groupid, userid, grouprole

### 已知问题（v1.0 中未解决）

| 问题 | 影响 | 在 v1.1 修复 |
|------|------|-------------|
| TCP 粘包导致 JSON 解析失败 | 消息错乱、连接断开 | ✅ 帧协议 |
| 密码明文存储 | 数据库泄露风险 | ✅ SHA256 |
| SQL 注入 | 恶意输入可破坏数据库 | ✅ escape() |
| 频繁创建/销毁 MySQL 连接 | 性能瓶颈 | ✅ 连接池 |
| 好友单向可见 | B 看不到 A | ✅ 双向插入 |
| 配置硬编码 | 修改需重新编译 | ✅ 配置系统 |
| send() 部分发送 | 帧协议损坏 | ✅ 循环发送 |

---

## 版本路线图

| 版本 | 主题 | 内容 |
|------|------|------|
| **v1.0** ✅ | 功能原型 | 注册/登录、私聊、群聊、离线消息、Redis Pub/Sub |
| **v1.1** ✅ | 工程质量 | 帧协议、SHA256、防注入、连接池、配置系统 |
| v1.2 | Qt 客户端 | 图形化界面（暂定） |
| v1.3 | 功能扩展 | 心跳、消息可靠性、好友请求系统 |
| v1.4 | 性能优化 | 业务线程池、性能压测、数据采集 |


## v1.1.0 — 工程质量升级 (2026-06-03)

### 概述

v1.1 是项目从"功能原型"迈向"工程系统"的第一阶段升级，核心聚焦**网络可靠性、安全性、数据访问效率**三大方向。

---

### 新增功能

#### 🔌 TCP 帧协议（粘包处理）

- **问题**：TCP 是字节流协议，`send()` 次数 ≠ `recv()` 次数，多个 JSON 消息会粘在一起，导致解析失败
- **方案**：4 字节大端长度前缀 + 循环接收
  - `sendFramedMessage()` — 发送时先发 4 字节长度（`htonl`），再发 JSON 体，循环 `send()` 直到全部发出
  - `recvFramedMessage()` — 先收 4 字节长度（`ntohl`），再循环 `recv()` 收完整的 JSON 体
- **文件**：[`src/client/Client.cpp`](src/client/Client.cpp) · [`src/server/ChatService.cpp`](src/server/ChatService.cpp)
- **测试**：[`tests/framing/`](tests/framing/) — 对比测试（旧版无帧 vs 新版有帧）

#### 🔐 SHA256 密码哈希

- **问题**：密码明文存储，数据库泄露 = 所有账号密码泄露；同一密码撞库攻击
- **方案**：自实现 SHA256（RFC 6234），~80 行，不依赖 OpenSSL 等外部库
  - 注册时 `SHA256::hash(pwd)` → 存哈希
  - 登录时 `SHA256::hash(input)` → 与数据库哈希比对
- **文件**：[`include/server/util/SHA256.hpp`](include/server/util/SHA256.hpp) · [`src/server/util/SHA256.cpp`](src/server/util/SHA256.cpp)

#### 🛡️ SQL 注入防护

- **问题**：`sprintf` 拼接 SQL，用户输入 `'; DROP TABLE user; --` 可注入恶意 SQL
- **方案**：封装 `MySQL::escape()`，调用 `mysql_real_escape_string` 转义危险字符（`'`、`\`、null 字节等），自动加单引号包裹
  - RAII 内存管理（`std::vector<char>` 替代裸 `new char[]`）
  - 连接有效性检查 + 返回值错误检查
- **文件**：[`include/server/db/db.h`](include/server/db/db.h) · [`src/server/db/db.cpp`](src/server/db/db.cpp)

#### 🔗 MySQL 连接池

- **问题**：每次数据库操作都 `new MySQL` → `connect()` → 3 次 TCP 握手 → 用完 `delete`，频繁创建销毁开销大
- **方案**：单例连接池，RAII 归还
  - 预创建 N 个连接（可配，默认 8）
  - `getConnection()` 返回 `shared_ptr<MySQL>`，自定义 deleter 自动归还
  - `condition_variable` 实现连接耗尽时阻塞等待
- **文件**：[`include/server/db/ConnectionPool.hpp`](include/server/db/ConnectionPool.hpp) · [`src/server/db/ConnectionPool.cpp`](src/server/db/ConnectionPool.cpp)

#### ⚙️ 配置系统

- **问题**：IP、端口、数据库账号等硬编码在代码中，修改需重新编译
- **方案**：简单 `key=value` 格式解析器，不引入 JSON/INI 依赖
  - 单例 `Config`，`get()` / `getInt()` 接口
  - 服务启动时从 `conf/server.conf` 加载
- **文件**：[`include/server/config/Config.hpp`](include/server/config/Config.hpp) · [`src/server/config/Config.cpp`](src/server/config/Config.cpp) · [`conf/server.conf`](conf/server.conf)

---

### Bug 修复

#### 🐛 sendFramedMessage 部分发送

- **问题**：`send()` 一次调用不一定发送全部字节（socket 缓冲区满时），剩余字节丢失导致帧协议损坏
- **修复**：循环 `send()` 直到所有字节发出（[`Client.cpp:71-78`](src/client/Client.cpp#L71-L78)）

#### 🐛 好友双向插入

- **问题**：`FriendModel::insert()` 只插入 `(userid, friendid)` 一条记录，A 能看到 B 但 B 看不到 A
- **修复**：同时插入 `(userid, friendid)` 和 `(friendid, userid)`（[`FriendModel.cpp`](src/server/model/FriendModel.cpp)）

#### 🐛 mainMenu 注册不生效

- **问题**：`mainMenu()` 的 `switch` 中 `case 2`（注册）分支缺失 `registerUser()` 调用
- **修复**：补充 `registerUser(name, pwd)` 调用（[`Client.cpp:342`](src/client/Client.cpp#L342)）

#### 🐛 _running 线程安全隐患

- **问题**：`_running` 是 `bool` 类型，一个线程写、另一个线程读，存在数据竞争（UB）
- **修复**：改为 `std::atomic<bool>`（[`Client.hpp`](include/client/Client.hpp)）

#### 🐛 密码字段长度不足

- **问题**：数据库 `user.password` 列可能是 `VARCHAR(32)`，SHA256 哈希 64 字符导致 `Data too long`
- **修复**：`ALTER TABLE user MODIFY password CHAR(64);`

---

### 代码质量改进

- 移除所有头文件中的 `using namespace std;`（9 个头文件）
- 修复 `FriendModel.hpp` 头文件保护宏拼写错误（`FFIEND` → `FRIEND`）
- 统一使用 Muduo 日志宏替代 `std::cerr` / `std::cout`
- 服务端添加 `SIGINT` 信号处理，Ctrl+C 优雅退出
- 预定义未来消息类型（心跳、ACK、好友请求等）

---

### 测试

- [`tests/framing/`](tests/framing/) — TCP 帧协议对比测试（旧版粘包 vs 新版正确分帧）
- [`docs/study-notes/study-notes.md`](docs/study-notes/study-notes.md) — TCP 粘包原理详解（含时序图和代码分析）

---

