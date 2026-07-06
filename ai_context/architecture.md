# 架构设计

## 拓扑结构

```
┌──────────────────────┐
│  ChatClient (Term)   │  TCP Socket, 自定义帧协议
│  ChatClientQt (GUI)  │  Qt Widgets + ProtocolClient
└────────┬─────────────┘
         │
         ▼
┌──────────────────────┐
│  Nginx TCP Stream    │  listen :8000
│  负载均衡 (轮询/least_conn)│  upstream → ChatServer×N
│  心跳检测 + 故障转移   │  max_fails=3 fail_timeout=30s
└────────┬─────────────┘
         │ (分发到各节点)
    ┌────┼────┐
    ▼    ▼    ▼
┌──────┐ ┌──────┐ ┌──────┐
│Chat  │ │Chat  │ │Chat  │  Muduo Reactor, 4 worker threads
│Server│ │Server│ │Server│
│:6000 │ │:6002 │ │:600N │
└──┬───┘ └──┬───┘ └──┬───┘
   │        │        │
   └────────┼────────┘
            ▼
   ┌──────────────────┐
   │     MySQL 8.0    │  持久化：User/Friend/Group/OfflineMsg
   └──────────────────┘
   ┌──────────────────┐
   │  Redis Pub/Sub   │  跨服务器实时消息路由
   └──────────────────┘
```

### 关键说明

- **客户端不是浏览器/Web**，而是两个独立的 C++ 客户端程序
- Nginx 对外暴露 **单一端口 8000**，客户端只需连接这个端口
- Nginx 支持最少连接调度算法 (`least_conn`) 和默认轮询
- 各 ChatServer 实例独立，之间不直接通信，通过 Redis Pub/Sub 间接协作
- 服务端端口和线程数通过 `conf/server.conf` 配置

## 线程模型

```
┌──────────────────┐
│  主线程           │  EventLoop (base loop)
│  - accept 新连接  │
│  - 分发给 worker  │
└──────────────────┘
         │
    ┌────┼────┬────┐
    ▼    ▼    ▼    ▼
┌──────┐┌──────┐┌──────┐┌──────┐
│Worker││Worker││Worker││Worker│  线程池 (setThreadNum(4))
│  0   ││  1   ││  2   ││  3   │
│I/O事件││I/O事件││I/O事件││I/O事件│
│帧解包 ││帧解包 ││帧解包 ││帧解包 │
│业务分发││业务分发││业务分发││业务分发│
└──────┘└──────┘└──────┘└──────┘
         +
┌──────────────────┐
│ Redis 订阅线程    │  独立线程
│ - 阻塞式监听     │  redisGetReply (阻塞)
│ - 收到消息后回调 │  → notify_handler
│   ChatService    │  → redisSubscribeMessage()
└──────────────────┘
```

### 线程安全说明

- `_userConnMap`（用户连接表）受 `connMutex` 互斥锁保护
- MySQL 连接池通过 `_mutex` + `_cv` 条件变量实现线程安全
- Redis `subscribe` 和 `publish` 使用**两个独立连接**：
  - `_publish_context`：发送消息（各 worker 线程均可调用）
  - `_subscribe_context`：阻塞监听（独立线程持有）

## 数据流

### 完整请求-响应链路

```
Client                            Server
  │                                 │
  │  ┌────────────────- TCP ───┐   │
  │  │ [4B len][JSON payload]  │   │
  │  └─────────────────────────┘   │
  │                                 ▼
  │                          ChatServer::onMessage()
  │                          ├─ 循环读 buffer
  │                          ├─ 解析 4 字节长度头 (ntohl)
  │                          ├─ 校验长度 (<= 1MB)
  │                          ├─ 裁剪完整帧
  │                          └─ JSON::parse(msg)
  │                                 │
  │                                 ▼
  │                          ChatService::getHandler(msgId)
  │                          ├─ 查 msgHandlerMap
  │                          └─ 调用对应 handler
  │                                 │
  │                    ┌────────────┼────────────┐
  │                    ▼            ▼            ▼
  │              MySQL (持久化)  Redis (跨服)  直接回复
  │                                 │
  │  ◄──────────────────────────────┘
  │                          sendFramed(conn, msg)
  │                          ├─ htonl(msg.size())
  │                          └─ [4B len][JSON]
```

### 一对一消息路由决策树

```
ChatService::oneChat(toid, msg)
  │
  ├─ toid 在本地 _userConnMap ？
  │   YES → sendFramed(to_conn) ─── 直接送达
  │
  ├─ toid 状态 == "online"（不在本机）？
  │   YES → _redis.publish(toid, msg) ─── 发布到 Redis 频道
  │         → 目标服务器订阅线程收到 → redisSubscribeMessage()
  │         → sendFramed(to_conn) ─── 跨服送达
  │
  └─ toid 离线
      → _offlineMsgModel.insert(toid, msg) ─── 存入 MySQL 离线表
      → 用户下次登录时拉取并清除
```

## 关键设计

### 1. Reactor 非阻塞 I/O

- Muduo 提供事件循环 (EventLoop) 和 Reactor 模式
- 主线程 accept，4 个 worker 线程处理 I/O 读写和业务分发
- 无阻塞，高吞吐

### 2. TCP 自定义帧协议

- 4 字节网络字节序（大端）长度前缀 + JSON Payload
- 最大帧长 1MB
- 循环解包：buffer 不足时 break 等待下次数据到达，正确处理 TCP 粘包/半包
- 实现见 [ChatServer.cpp:34-63](../src/server/ChatServer.cpp)

### 3. 单例模式

三个核心组件使用单例（懒汉式，线程安全）：

| 单例 | 作用 |
|------|------|
| `ChatService::instance()` | 全局唯一的业务处理入口 |
| `Config::instance()` | 全局配置访问 |
| `ConnectionPool::instance()` | 全局数据库连接池 |

### 4. Redis Pub/Sub 跨服路由

- 每个用户 ID 作为 Redis 频道名（int → string channel）
- 用户登录时 `subscribe(userid)`，登出/断线时 `unsubscribe(userid)`
- 本机找不到目标用户连接 → `publish(toid, msg)` 广播到 Redis
- 目标用户所在服务器的 Redis 订阅线程收到消息 → 投递到对应连接

### 5. MySQL 连接池

- 生产者-消费者模型：初始化时创建 poolSize 个连接
- 外部通过 `getConnection()` 获取连接（返回 `shared_ptr<MySQL>`，自定义 deleter 实现 RAII 自动归还）
- 池空时阻塞等待（条件变量 `_cv`）

### 6. 离线消息三级投递

```
消息投递优先级:
  本地在线 (sendFramed) → Redis 跨服 (publish) → MySQL 离线存储 (insert)
```

- 本地判定：`_userConnMap` 查找
- 跨服判定：查数据库状态为 "online"（在别的节点），发 Redis
- 离线兜底：都不命中，存 MySQL `OfflineMessage` 表
- 登录时批量拉取 → 推送 → 清除

### 7. 安全设计

- 密码使用 SHA-256 哈希存储和比较，不传不存明文
- MySQL 查询使用参数化，通过 `mysql_real_escape_string` 防 SQL 注入

## 模块依赖图

```
ChatServer (网络层)
  │
  ▼
ChatService (业务层，单例)
  ├──► UserModel ──────► ConnectionPool ──► MySQL
  ├──► FriendModel ────► ConnectionPool ──► MySQL
  ├──► GroupModel ─────► ConnectionPool ──► MySQL
  ├──► OfflineMsgModel ► ConnectionPool ──► MySQL
  ├──► Redis (Pub/Sub)
  └──► _userConnMap (本地连接表)

Config (单例) ◄── main() 初始化时加载
```

各 Model 类持有对 ConnectionPool 的依赖，每次数据库操作从池中获取连接，操作完成后自动归还。
