# Cluster Chat Server — 项目迁移文档

> 导出日期：2026-07-06

---

## 一句话定位

基于 Muduo 网络库的分布式集群聊天服务器，支持多节点部署、跨服务器消息路由与横向扩展。配套终端客户端和 Qt GUI 客户端。

---

## 版本与进度

| 版本 | 状态 | 内容 |
|------|------|------|
| v1.0.0 | ✅ 已完成 | 注册/登录/一对一聊天/好友/群组/离线消息 |
| v1.1.0 | ✅ 已提交 | TCP帧协议 + SHA256 + SQL防注入 + 连接池 + 配置系统 |
| **v1.2.0** | 🔴 待开始 | Phase 3 紧急修复（锁粒度 + 心跳 + 性能优化） |

**已提交的最近两次 commit：**
```
5b42eac v1.1.0: TCP帧协议 + SHA256 + SQL防注入 + 连接池 + 配置系统
83462ef first commit
```

**工作区有未提交改动（+82/-5 行，6 个文件）：**
- `CMakeLists.txt`、`src/CMakeLists.txt`、`README.md`、`conf/server.conf`、`include/public.hpp`
- `src/server/ChatService.cpp`（改动最大，+74 行）
- 未跟踪目录：`ai_context/`、`sql/`、`src/qt-client/`

---

## 已完成功能清单 ✅

| 类别 | 功能 |
|------|------|
| **核心业务** | 注册、登录/登出、一对一聊天（三级投递：本地→Redis→离线）、群聊、添加好友、创建/加入群组、离线消息 |
| **基础设施** | TCP 4字节大端帧协议、MySQL 连接池（生产者-消费者+RAII）、SHA-256 哈希、SQL 参数化防注入、INI 配置系统、Redis Pub/Sub 双连接、Muduo Reactor 多线程 |
| **部署** | Nginx TCP Stream 负载均衡（轮询/least_conn） |
| **客户端** | POSIX 终端客户端 + Qt Widgets GUI（亮/暗主题、CJK 字体） |

---

## 技术栈

| 层 | 技术 |
|----|------|
| 语言 | C++11 |
| 网络 | Muduo（Reactor 多线程） |
| 数据库 | MySQL 8.0 (InnoDB) |
| 缓存/消息 | Redis + hiredis (Pub/Sub) |
| 序列化 | nlohmann/json |
| 负载均衡 | Nginx TCP Stream |
| 密码哈希 | OpenSSL SHA-256 |
| 构建 | CMake ≥ 3.16 |
| Qt 客户端 | Qt 5/6 Widgets |

---

## 架构图

```
Client (Terminal / Qt GUI)
        │
        ▼
   Nginx (TCP Stream 负载均衡)
        │
   ┌────┼────┐
   ▼    ▼    ▼
ChatServer-1  ChatServer-2  ChatServer-N
   │    │    │
   └────┼────┘
        ▼
   MySQL (持久化)  +  Redis (Pub/Sub 跨服路由)
```

---

## 部署构建

### 依赖安装

```bash
# MySQL 8.0, Redis, hiredis, Muduo, Nginx (with --with-stream), Boost
# Qt 5/6 (仅 Qt 客户端需要)
```

### 构建

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
# 产物在 bin/ 目录
```

### 启动

```bash
# 服务端（可启动多个实例，不同端口，前面挂 Nginx）
./bin/ChatServer conf/server.conf

# 终端客户端
./bin/ChatClient <server_ip> <port>

# Qt 客户端
./bin/ChatClientQt -H <server_ip> -P <port>
```

---

## 目录结构

```
ChatServer/
├── CMakeLists.txt              # 顶层构建
├── conf/server.conf            # 服务器配置 (INI)
├── sql/chatdb.sql              # 数据库表结构
├── include/                    # 头文件
│   ├── public.hpp              # 消息类型枚举 (EnMsgType, 22种)
│   ├── server/                 # ChatServer, ChatService, db, model, redis, config, util
│   └── client/Client.hpp       # 终端客户端
├── src/
│   ├── server/                 # 服务端源码（main, ChatServer, ChatService, db, model, redis, config, util）
│   ├── client/                 # 终端客户端源码
│   └── qt-client/              # Qt GUI 客户端源码
├── tests/                      # 测试
├── third_party/                # json.hpp, muduo, nginx, hiredis
├── ai_context/                 # AI 上下文（架构评审、进度、模块文档）
└── bin/                        # 编译产物
```

---

## 消息协议

### 帧格式

```
+----------------+------------------+
| 4 bytes (BE)   | N bytes          |
|  Body Length   | JSON Body (UTF-8)|
+----------------+------------------+
```

最大消息体：1MB

### 消息类型枚举 (EnMsgType)

| 枚举值 | 消息 ID | 说明 |
|--------|---------|------|
| LOGIN_MSG | 1 | 登录 |
| REG_MSG | 2 | 注册 |
| LOGIN_ACK | 3 | 登录应答 |
| REG_ACK | 4 | 注册应答 |
| ONE_CHAT_MSG | 5 | 一对一聊天 |
| ADD_FRIEND_MSG | 6 | 添加好友 |
| CREATE_GROUP_MSG | 7 | 创建群组 |
| ADD_GROUP_MSG | 8 | 加入群组 |
| GROUP_CHAT_MSG | 9 | 群聊 |
| LOGIN_OUT_MSG | 10 | 登出 |
| PING_MSG | 14 | 心跳 Ping |
| PONG_MSG | 15 | 心跳 Pong |
| MSG_ACK | 16 | 消息已读确认 |
| USER_SEARCH_MSG | 17 | 用户搜索 |
| USER_SEARCH_ACK | 18 | 搜索应答 |
| FRIEND_REQUEST_MSG | 19 | 好友申请 |
| FRIEND_ACCEPT_MSG | 20 | 接受好友 |
| FRIEND_REJECT_MSG | 21 | 拒绝好友 |
| FRIEND_NOTIFY_MSG | 22 | 好友通知 |

---

## 数据库表结构

参见 [sql/chatdb.sql](sql/chatdb.sql)：

- **User** — 用户表（id, name, password, state, lastLoginTime）
- **Friend** — 好友关系表（userId, friendId, addTime）
- **AllGroup** — 群组表（id, groupname, groupdesc）
- **GroupUser** — 群成员表（groupId, userId, grouprole）
- **OfflineMessage** — 离线消息表（id, userId, message, sendTime）
- **FriendRequest**（待建）— 好友申请表

---

## 待修复的已知瓶颈（来自 6/18 架构评审）

| 优先级 | 问题 | 位置 | 影响 |
|--------|------|------|------|
| 🔴 严重 | `groupChat()` 持锁期间执行 DB/Redis I/O | ChatService.cpp | 群聊阻塞所有并发操作 |
| 🔴 严重 | 登录 6 次独立 DB 往返 | ChatService.cpp | 高并发登录延迟高 |
| 🔴 严重 | `GroupModel::queryGroups()` N+1 查询 | GroupModel.cpp | 登录拖慢 |
| 🔴 严重 | 无心跳保活机制 | — | 半开连接 2h 才检测到 |
| 🟡 中等 | Redis publish 同步阻塞 | redis.cpp | worker 线程被阻塞 |
| 🟡 中等 | `server.threads` 硬编码为 4 | ChatServer.cpp | 无法按机器规格调优 |
| 🟡 中等 | SHA-256 无盐 | — | 安全基线问题 |
| 🟡 中等 | 无 Redis 自动重连/重订阅 | redis.cpp | Redis 断连无法恢复 |
| 🟢 低 | 无过载保护（连接/消息速率限制） | — | 恶意客户端可压垮服务端 |
| 🟢 低 | 消息不持久化（仅存离线消息） | — | 聊天记录不可追溯 |

---

## Phase 3 实施路线

| 阶段 | 内容 | 预计 |
|------|------|------|
| **3a** 紧急修复 | 锁粒度修复 + 心跳保活 + 线程数配置化 | 2-3天 |
| **3b** 性能优化 | 批量查询（N+1→JOIN）、Redis Pipeline、登录批量化 | 3-4天 |
| **3c** 新功能 | 好友申请流程、用户搜索、MSG_ACK 已读确认 | 3-4天 |

### Phase 3a 具体任务

1. **Fix-1: `groupChat()` 锁粒度修复**（优先级最高）
   - 将 `connMutex` 锁范围缩小到仅保护 `_userConnMap` 查找
   - DB 查询和 Redis 操作移出锁外
   - 改动 ~15 行，收益最高

2. **Fix-2: 心跳保活 (PING/PONG)**
   - 服务端：记录连接时间，定时器扫描超时连接
   - 客户端：心跳线程，超时未收到 Pong 则重连
   - 配置项：`heartbeat.interval=30` / `heartbeat.timeout=90`

3. **Fix-3: `server.threads` 配置化**
   - 去掉 ChatServer 中硬编码的 4
   - 从配置文件读取并传入

---

## 关键文件速查

| 文件 | 作用 |
|------|------|
| [include/public.hpp](include/public.hpp) | 22 种消息类型枚举 |
| [src/server/ChatService.cpp](src/server/ChatService.cpp) | 业务核心，msgId 路由 + 所有业务处理 |
| [src/server/ChatServer.cpp](src/server/ChatServer.cpp) | 网络层，TCP 连接管理 + 帧解包 |
| [src/server/main.cpp](src/server/main.cpp) | 入口：初始化配置、连接池、启动 Server |
| [include/server/db/ConnectionPool.hpp](include/server/db/ConnectionPool.hpp) | MySQL 连接池 |
| [src/server/model/GroupModel.cpp](src/server/model/GroupModel.cpp) | 群组 DAO（含 N+1 问题） |
| [src/server/redis/redis.cpp](src/server/redis/redis.cpp) | Redis Pub/Sub 客户端 |
| [include/server/config/Config.hpp](include/server/config/Config.hpp) | INI 配置文件解析 |
| [include/server/util/SHA256.hpp](include/server/util/SHA256.hpp) | SHA-256 哈希工具 |
| [ai_context/architecture_review.md](ai_context/architecture_review.md) | 完整架构评审 + 优化方案 |
| [ai_context/progress.md](ai_context/progress.md) | 进度跟踪文档 |
| [ai_context/project_overview.md](ai_context/project_overview.md) | 项目总览 |
| [sql/chatdb.sql](sql/chatdb.sql) | 数据库建表 SQL |
| [conf/server.conf](conf/server.conf) | 服务器配置示例 |
