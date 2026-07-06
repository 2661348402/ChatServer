# Redis 模块 — 跨服务器消息路由

## 源文件

- 头文件：[include/server/redis/redis.hpp](../include/server/redis/redis.hpp)
- 实现：`src/server/redis/`

## 职责

- 封装 hiredis C 库，提供 Redis 连接和操作接口
- **跨服务器消息路由**：通过 Redis Pub/Sub 实现多 ChatServer 节点间的实时消息投递
- 用户上下线时管理 Redis 频道的订阅/取消订阅

## 核心设计：双连接模型

Redis 类持有两个独立的 Redis 连接：

```
┌─────────────────────────────┐
│          Redis 类            │
│                              │
│  _publish_context ─────────► Redis Server
│    (发布消息，各 worker 线程    │      │
│     均可调用，非阻塞)           │      │
│                              │      │
│  _subscribe_context ────────►  频道: userid (int)
│    (订阅消息，独立线程持有，     │   subscribe / unsubscribe
│     阻塞式 redisGetReply)     │   publish(msg)
└─────────────────────────────┘
```

### 为什么需要两个连接？

- `subscribe` 后连接进入订阅模式，**不能再执行其他命令**（包括 `publish`）
- `redisGetReply` 是阻塞调用，必须由独立线程持有
- 因此 publish 和 subscribe 必须使用**不同连接**

## 频道设计

- **频道名**：用户 ID（整数转字符串）
- **订阅时机**：用户登录成功后 `subscribe(userid)`
- **取消时机**：用户登出或异常断线时 `unsubscribe(userid)`
- **发布时机**：本机找不到目标用户，但数据库显示其在线（在别的节点）

## 核心接口

| 方法 | 说明 |
|------|------|
| `bool connect()` | 使用默认参数连接（依赖 Config 单例） |
| `bool connect(host, port)` | 指定参数连接，同时创建 publish 和 subscribe 两个连接 |
| `bool publish(int channel, const string& message)` | 发布消息到指定频道（用户 ID），JSON 字符串 |
| `bool subscribe(int channel)` | 订阅指定频道 |
| `bool unsubscribe(int channel)` | 取消订阅指定频道 |
| `void observer_channel_message()` | 独立线程运行的阻塞监听循环 |
| `void init_notify_handler(function<void(int,string)> fn)` | 注册回调，收到消息时调用 |

## 消息流

```
服务器 A                            Redis Server                  服务器 B
   │                                    │                           │
   │ oneChat(toid=5, msg)               │                           │
   │ ├─ 本地 _userConnMap 未找到 user 5  │                           │
   │ ├─ DB 查询 user 5 状态 = "online"   │                           │
   │ └─ _redis.publish(5, msg) ────────►│                           │
   │                                    │  PUBLISH channel:5 msg    │
   │                                    │ ─────────────────────────►│
   │                                    │                           │ _subscribe_context
   │                                    │                           │ redisGetReply()
   │                                    │                           │ → notify_handler
   │                                    │                           │ → ChatService::
   │                                    │                           │   redisSubscribeMessage()
   │                                    │                           │ → sendFramed(to_conn)
```

## 初始化流程

```cpp
// ChatService 构造函数中
if (_redis.connect()) {
    _redis.init_notify_handler(
        bind(&ChatService::redisSubscribeMessage, this, _1, _2));
}
// connect() 成功后会启动独立线程运行 observer_channel_message()
```

## 线程安全

- `_subscribe_context` 由独立线程独占（阻塞在 `redisGetReply`）
- `_publish_context` 可被多个 worker 线程调用 `publish()`
- hiredis 本身对单连接的并发操作**不是线程安全的**，双连接设计避免了这个问题
- 回调函数 `redisSubscribeMessage()` 内部访问 `_userConnMap` 时使用 `connMutex` 保护

## 依赖

- hiredis C 库（`third_party/hiredis-master.zip`）
- Config 单例：获取 Redis 连接参数
- 编译链接：`target_link_libraries(... hiredis pthread)`
