# 接口协议

## 传输层帧格式（TCP 自定义帧）

所有 C/S 通信在 TCP 流上使用统一的二进制帧格式：

```
┌─────────────────────┬──────────────────────────────┐
│   字节 0-3 (4 bytes) │   字节 4 - N                  │
│   Payload 长度       │   JSON Payload (UTF-8)        │
│   uint32_t, 大端序   │                               │
└─────────────────────┴──────────────────────────────┘
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `length` | `uint32_t` (大端序/网络字节序) | JSON Body 的字节数，不包含自身 4 字节 |
| `payload` | `string` (UTF-8) | JSON 格式的业务消息 |

### 编码规则

- **发送**：`htonl(msg.size())` 将主机字节序转网络字节序，拼接在 JSON 字符串前
- **接收**：先读 4 字节 → `ntohl()` 转回主机字节序 → 按长度读取 JSON Body
- **最大帧长**：1MB (`1024 * 1024`)，超过直接 `conn->shutdown()` 断开连接
- **拆包逻辑**：循环读取 muduo Buffer，不足 `4 + msgLen` 时 break，等待下次数据到达（正确处理 TCP 粘包/半包）

### 实现引用

- 服务端发送：[ChatService.cpp:17-22](../src/server/ChatService.cpp) `sendFramed()`
- 服务端接收：[ChatServer.cpp:34-63](../src/server/ChatServer.cpp) `onMessage()`
- 客户端发送：[Client.cpp:66-79](../src/client/Client.cpp) `sendFramedMessage()`
- 客户端接收：[Client.cpp:56-63](../src/client/Client.cpp) `recvFramedMessage()`

---

## 消息通用格式

所有消息均为 JSON 对象，`msgId` 字段为**必填项**：

```json
{
  "msgId": <int>,     // 消息类型，对应 EnMsgType 枚举值（必填）
  "id": <int>,        // 发送方用户 ID（大部分请求携带）
  // ... 各消息类型的扩展字段
}
```

响应消息使用对应的 `*_MSG_ACK` 类型，统一携带 `errno` 表示结果：

```json
{
  "msgId": <int>,     // *_MSG_ACK
  "errno": <int>,     // 0 = 成功，非 0 = 失败
  "errMessage": "..." // 错误描述（失败时）
}
```

---

## 消息类型枚举 (EnMsgType)

定义文件：[include/public.hpp](../include/public.hpp)

### 账号相关

| msgId | 枚举名 | 方向 | 说明 |
|-------|--------|------|------|
| 1 | `LOGIN_MSG` | C→S | 登录请求 |
| 2 | `REG_MSG` | C→S | 注册请求 |
| 3 | `REG_MSG_ACK` | S→C | 注册响应 |
| 4 | `LOG_MSG_ACK` | S→C | 登录响应 |
| 10 | `LOGIN_OUT_MSG` | C→S | 登出请求 |

### 聊天相关

| msgId | 枚举名 | 方向 | 说明 |
|-------|--------|------|------|
| 5 | `ONE_CHAT_MSG` | C→S→C | 一对一聊天消息 |
| 9 | `GROUP_CHAT_MSG` | C→S→C | 群聊消息 |

### 好友相关

| msgId | 枚举名 | 方向 | 说明 |
|-------|--------|------|------|
| 6 | `ADD_FRIEND_MSG` | C→S | 添加好友请求 |
| 11 | `ADD_FRIEND_MSG_ACK` | S→C | 添加好友响应 |

### 群组相关

| msgId | 枚举名 | 方向 | 说明 |
|-------|--------|------|------|
| 7 | `CREATE_GROUP_MSG` | C→S | 创建群组请求 |
| 8 | `ADD_GROUP_MSG` | C→S | 加入群组请求 |
| 12 | `CREATE_GROUP_MSG_ACK` | S→C | 创建群组响应 |
| 13 | `ADD_GROUP_MSG_ACK` | S→C | 加入群组响应 |

### Phase 3 扩展（已定义枚举，功能待实现）

| msgId | 枚举名 | 方向 | 说明 |
|-------|--------|------|------|
| 14 | `PING_MSG` | C→S | 心跳 Ping |
| 15 | `PONG_MSG` | S→C | 心跳 Pong |
| 16 | `MSG_ACK` | C↔S | 消息已读确认 |
| 17 | `USER_SEARCH_MSG` | C→S | 用户搜索请求 |
| 18 | `USER_SEARCH_ACK` | S→C | 用户搜索响应 |
| 19 | `FRIEND_REQUEST_MSG` | C→S | 好友申请 |
| 20 | `FRIEND_ACCEPT_MSG` | C→S | 接受好友申请 |
| 21 | `FRIEND_REJECT_MSG` | C→S | 拒绝好友申请 |
| 22 | `FRIEND_NOTIFY_MSG` | S→C | 好友申请通知 |

---

## 各消息详细格式

### 1. 登录

**请求 LOGIN_MSG (1)：**
```json
{
  "msgId": 1,
  "id": 4,
  "password": "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92"
}
```
- `password`：SHA-256 哈希后的 64 字符十六进制字符串

**响应 LOG_MSG_ACK (4)：**

成功：
```json
{
  "msgId": 4,
  "errno": 0,
  "id": 4,
  "name": "ggw",
  "offlineMsg": ["{\"msgId\":5,\"from\":\"张三\",\"message\":\"hello\",\"sendtime\":\"...\"}", ...],
  "friends": ["{\"id\":5,\"name\":\"张三\",\"state\":\"online\"}", ...],
  "groups": ["{\"id\":1,\"name\":\"dashabi\",\"desc\":\"...\",\"members\":[...]}", ...]
}
```

失败：
```json
{"msgId": 4, "errno": 1, "errMessage": "user not exist"}
{"msgId": 4, "errno": 2, "errMessage": "password error"}
{"msgId": 4, "errno": 3, "errMessage": "already online"}
```

| errno | 含义 |
|-------|------|
| 0 | 登录成功 |
| 1 | 用户不存在 |
| 2 | 密码错误 |
| 3 | 用户已在线（重复登录） |

- `offlineMsg`：JSON 字符串数组，每条是一条离线消息的 JSON
- `friends`：JSON 字符串数组，每条是一个好友对象 `{id, name, state}`
- `groups`：JSON 字符串数组，每条是一个群组对象 `{id, name, desc, members[]}`

### 2. 注册

**请求 REG_MSG (2)：**
```json
{
  "msgId": 2,
  "name": "donk",
  "password": "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92"
}
```

**响应 REG_MSG_ACK (3)：**

成功：
```json
{"msgId": 3, "errno": 0, "id": 8}
```
失败：
```json
{"msgId": 3, "errno": 1}
```

- 成功时 `id` 是服务端分配的用户 ID（MySQL AUTO_INCREMENT）

### 3. 一对一聊天

**请求/转发 ONE_CHAT_MSG (5)：**
```json
{
  "msgId": 5,
  "id": 4,
  "from": "ggw",
  "toid": 5,
  "message": "你好！",
  "sendtime": "2026-06-18 14:30:00.123456"
}
```

- `sendtime`：服务端收到消息时打的时间戳 (`Timestamp::toFormattedString()`)
- 服务端收到后原样转发给目标用户

### 4. 添加好友

**请求 ADD_FRIEND_MSG (6)：**
```json
{
  "msgId": 6,
  "id": 4,
  "friendId": 5
}
```

**响应 ADD_FRIEND_MSG_ACK (11)：**

成功：
```json
{"msgId": 11, "errno": 0, "friendId": 5, "friendName": "张三", "friendState": "online"}
```
失败：
```json
{"msgId": 11, "errno": 1, "errMessage": "failed to add friend"}
```

### 5. 创建群组

**请求 CREATE_GROUP_MSG (7)：**
```json
{
  "msgId": 7,
  "id": 4,
  "groupname": "技术交流",
  "groupdesc": "C++ 学习群"
}
```

**响应 CREATE_GROUP_MSG_ACK (12)：**

成功：
```json
{
  "msgId": 12,
  "errno": 0,
  "groupId": 4,
  "groupName": "技术交流",
  "groupDesc": "C++ 学习群",
  "members": [{"id": 4, "name": "ggw", "role": "creator", "state": "online"}]
}
```
失败：
```json
{"msgId": 12, "errno": 1, "errMessage": "failed to create group"}
```

### 6. 加入群组

**请求 ADD_GROUP_MSG (8)：**
```json
{
  "msgId": 8,
  "id": 5,
  "groupId": 1
}
```

**响应 ADD_GROUP_MSG_ACK (13)：**
```json
{
  "msgId": 13,
  "errno": 0,
  "groupId": 1,
  "groupName": "dashabi",
  "groupDesc": "this a group of sb",
  "members": [
    {"id": 4, "name": "ggw", "role": "creator", "state": "online"},
    {"id": 5, "name": "张三", "role": "normal", "state": "online"}
  ]
}
```

### 7. 群聊

**请求/转发 GROUP_CHAT_MSG (9)：**
```json
{
  "msgId": 9,
  "id": 4,
  "from": "ggw",
  "groupId": 1,
  "message": "大家好啊！"
}
```

- 服务端查数据库获取群内所有成员 ID，排除发送者自己
- 对每个成员执行三级投递（本地 → Redis → 离线）

### 8. 登出

**请求 LOGIN_OUT_MSG (10)：**
```json
{
  "msgId": 10,
  "id": 4
}
```

- 服务端处理：从 `_userConnMap` 移除 → Redis `unsubscribe` → 更新用户状态为 "offline"
- 无响应消息

---

## 客户端命令映射

终端客户端命令与服务端消息的对应关系：

| 客户端命令 | 格式 | msgId |
|-----------|------|-------|
| 登录 | 菜单选项 1，输入 ID + 密码 | `LOGIN_MSG(1)` |
| 注册 | 菜单选项 2，输入名称 + 密码 | `REG_MSG(2)` |
| `chat:id:message` | 一对一聊天 | `ONE_CHAT_MSG(5)` |
| `addfriend:id` | 添加好友 | `ADD_FRIEND_MSG(6)` |
| `creategroup:name:desc` | 创建群组 | `CREATE_GROUP_MSG(7)` |
| `addgroup:id` | 加入群组 | `ADD_GROUP_MSG(8)` |
| `groupchat:id:message` | 群聊 | `GROUP_CHAT_MSG(9)` |
| `loginout` | 登出 | `LOGIN_OUT_MSG(10)` |
