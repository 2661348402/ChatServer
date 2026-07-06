# ChatClient — 终端客户端

## 源文件

- 头文件：[include/client/Client.hpp](../include/client/Client.hpp)
- 实现：[src/client/Client.cpp](../src/client/Client.cpp)

## 职责

- 通过 TCP Socket 连接聊天服务器
- 实现与服务端一致的帧协议（4 字节长度前缀 + JSON Body）
- 提供命令行交互界面：主菜单（登录/注册/退出）→ 聊天界面（命令驱动）
- 独立线程接收服务端推送的消息并实时显示

## 依赖

- POSIX Socket API：`socket()`, `connect()`, `send()`, `recv()`
- nlohmann/json：JSON 序列化/反序列化
- `public.hpp`：消息类型枚举
- `User.hpp`：本地用户信息

## 核心接口

### 连接/生命周期

| 方法 | 说明 |
|------|------|
| `ChatClient()` | 构造函数，初始化 socket 和命令处理器 |
| `~ChatClient()` | 析构函数，关闭连接 |
| `bool connectServer(ip, port)` | 建立 TCP 连接到服务器 |
| `void closeClient()` | 关闭连接，停止接收线程 |
| `void mainMenu()` | 主菜单循环（登录/注册/退出） |

### 业务操作

| 方法 | 说明 |
|------|------|
| `bool login(id, pwd)` | 登录：发送 LOGIN_MSG，解析响应（离线消息/好友/群组） |
| `bool registerUser(name, pwd)` | 注册：发送 REG_MSG，获取分配的 ID |
| `void chatMain()` | 进入聊天界面，启动接收线程，循环读取用户命令 |

### 聊天命令

| 命令 | 格式 | 对应方法 |
|------|------|---------|
| `help` | — | `help()`：显示命令列表 |
| `addfriend:id` | `addfriend:5` | `addFriend()`：发送 ADD_FRIEND_MSG |
| `chat:id:message` | `chat:5:你好` | `oneToOneChat()`：发送 ONE_CHAT_MSG |
| `creategroup:name:desc` | `creategroup:技术:C++群` | `createGroup()`：发送 CREATE_GROUP_MSG |
| `addgroup:id` | `addgroup:1` | `addGroup()`：发送 ADD_GROUP_MSG |
| `groupchat:id:message` | `groupchat:1:大家好` | `groupChat()`：发送 GROUP_CHAT_MSG |
| `loginout` | — | `loginOut()`：发送 LOGIN_OUT_MSG，返回主菜单 |

### 网络 I/O

| 函数 | 说明 |
|------|------|
| `void sendMsg(const string& buf)` | 发送帧（4 字节长度头 + JSON） |
| `void recvMsgThread()` | 接收线程：循环读取帧，解析 JSON，根据 msgId 显示消息内容 |
| `static bool recvFramedMessage(fd, out)` | 工具函数：从 socket 精确读取一帧 |
| `static void sendFramedMessage(fd, buf)` | 工具函数：向 socket 发送一帧 |
| `static bool recvAll(fd, buf, len)` | 工具函数：循环 `recv()` 直到读满 len 字节 |

## 帧协议（客户端实现）

与服务端完全一致的实现：

**发送**：`htonl(size)` + JSON 字符串，循环 `send()` 确保全部发出

**接收**：
1. `recvAll(4)` 读长度头
2. `ntohl()` 转主机字节序
3. 校验长度 ≤ 1MB
4. `recvAll(msgLen)` 读 Body
5. `json::parse()` 解析

## 线程模型

```
主线程                          接收线程
  │                               │
  │ mainMenu() / chatMain()       │ recvMsgThread()
  │ - 等待用户输入                 │ - 阻塞 recvFramedMessage()
  │ - 发送请求消息                 │ - 收到消息 → 解析 → 显示
  │                               │ - 连接断开 → _running=false
  │                               │
  └───────────────┬───────────────┘
                  │
            共享 _running 标志位 (atomic<bool>)
```

## 使用方式

```bash
./bin/ChatClient <server_ip> <port>
```

```
===== Main Menu =====
1. Login
2. Register
3. Exit
Select: 1
ID: 4
Password: 123456

===== Login Success! =====
[Friends]:
  ID:5  Name:张三  State:online
[Groups]:
  dashabi - this a group of sb

Enter chat, type help for commands
[cmd] >> chat:5:hello
message sent
[cmd] >> loginout
logged out, returning to menu...
```
