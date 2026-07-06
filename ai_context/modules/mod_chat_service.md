# ChatService — 业务层

## 源文件

- 头文件：[include/server/ChatService.hpp](../include/server/ChatService.hpp)
- 实现：[src/server/ChatService.cpp](../src/server/ChatService.cpp)

## 职责

- **消息分发**：维护 `msgId → handler` 映射表，将网络层解析的消息路由到对应处理函数
- **用户状态管理**：维护在线用户连接表 `_userConnMap`，处理登录/登出/断线
- **业务逻辑**：注册、登录、一对一聊天、群聊、好友管理、群组管理
- **跨服务器消息路由**：调用 Redis Pub/Sub 将消息投递到其他服务器节点上的在线用户
- **离线消息管理**：用户离线时存储消息，登录时推送并清除

## 依赖

- UserModel / FriendModel / GroupModel / OfflineMsgModel：数据访问层
- Redis：跨服务器消息发布/订阅
- ConnectionPool → MySQL：数据库连接
- nlohmann/json：JSON 序列化

## 设计模式

**单例模式**（懒汉式，C++11 静态局部变量保证线程安全）：

```cpp
static ChatService* instance() {
    static ChatService service;
    return &service;
}
```

## 核心接口

### 消息处理函数

| 方法 | 对应 msgId | 功能 |
|------|-----------|------|
| `login()` | `LOGIN_MSG(1)` | 用户登录：验证凭据、推送离线消息/好友/群组 |
| `reg()` | `REG_MSG(2)` | 用户注册：创建账号 |
| `oneChat()` | `ONE_CHAT_MSG(5)` | 一对一聊天：三级投递（本地→Redis→离线） |
| `addFriend()` | `ADD_FRIEND_MSG(6)` | 添加好友：写入好友关系表 |
| `createGroup()` | `CREATE_GROUP_MSG(7)` | 创建群组：建群 + 创建者自动加入 |
| `addGroup()` | `ADD_GROUP_MSG(8)` | 加入群组 |
| `groupChat()` | `GROUP_CHAT_MSG(9)` | 群聊：查询群成员 → 逐个投递 |
| `loginout()` | `LOGIN_OUT_MSG(10)` | 登出：清理连接、取消订阅、更新状态 |

### `MsgHandler getHandler(int msgId)`

根据 msgId 返回对应的处理函数。未注册的 msgId 返回空处理函数（打日志）。

### `static void sendFramed(const TcpConnectionPtr& conn, const string& msg)`

帧协议编码发送：`htonl(size)` + JSON 字符串，通过 muduo `conn->send()` 发出。

### `void clientConnectException(const TcpConnectionPtr& conn)`

连接异常断开处理：
- 遍历 `_userConnMap` 找到断开的用户 ID
- 从连接表移除
- Redis 取消订阅
- 更新用户状态为 "offline"

### `void redisSubscribeMessage(int userid, string message)`

Redis 订阅回调：
- 收到跨服务器消息后，在 `_userConnMap` 中查找目标用户
- 找到 → `sendFramed()` 直接发送
- 未找到（用户刚好下线）→ 存入离线消息表

### `bool reset()`

重置所有用户状态为 "offline"（服务器重启/退出时调用）。

## 消息分发机制

构造函数注册 `msgId → handler` 映射：

```cpp
ChatService::ChatService() {
    msgHandlerMap.insert({LOGIN_MSG,      bind(&ChatService::login,      this, _1, _2, _3)});
    msgHandlerMap.insert({REG_MSG,        bind(&ChatService::reg,        this, _1, _2, _3)});
    msgHandlerMap.insert({ONE_CHAT_MSG,   bind(&ChatService::oneChat,    this, _1, _2, _3)});
    // ... 其他消息类型
    // 同时初始化 Redis 连接和订阅回调
    if (_redis.connect()) {
        _redis.init_notify_handler(bind(&ChatService::redisSubscribeMessage, this, _1, _2));
    }
}
```

## 线程安全

- `_userConnMap` 操作受 `connMutex` (`std::mutex`) 保护
- 各 Model 类通过 ConnectionPool（内部条件变量同步）访问数据库
- Redis publish 可多线程调用，subscribe 在独立线程中阻塞运行

## 关键业务逻辑

### 登录流程

1. 查询用户是否存在（`_userModel.queryById`）
2. 验证密码（`SHA256::hash(pwd) == user.getPwd()`）
3. 检查是否已在线（`user.getState() == "online"`）
4. 成功：加入 `_userConnMap` → Redis `subscribe` → 更新状态 online
5. 推送离线消息（`_offlineMsgModel.query` + `remove`）
6. 推送好友列表（`_friendModel.query`）
7. 推送群组列表（`_groupModel.queryGroups`）

### 一对一消息路由决策

```
oneChat(toid)
├─ toid 在本机连接表？ → sendFramed() 直接发送
├─ toid 在数据库为 online？ → redis.publish() 跨服投递
└─ toid 离线？ → offlineMsgModel.insert() 存储
```

### 群聊消息

- 查询群内所有成员（排除发送者）
- 对每个成员执行上述三级投递
