# ChatServer — 网络层

## 源文件

- 头文件：[include/server/ChatServer.hpp](../include/server/ChatServer.hpp)
- 实现：[src/server/ChatServer.cpp](../src/server/ChatServer.cpp)

## 职责

- TCP 连接管理（接受连接 / 断开处理）
- 自定义帧协议解包（4 字节长度头 + JSON Payload）
- 消息合法性校验（长度上限 1MB）
- 将解析后的 JSON 消息分发给 ChatService 的对应 handler

## 依赖

- Muduo 网络库：`TcpServer`, `EventLoop`, `Buffer`, `Timestamp`
- ChatService（单例）：消息分发
- nlohmann/json：JSON 解析

## 核心接口

### `ChatServer(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg)`

构造函数，注册回调并设置线程数：

```cpp
ChatServer(muduo::net::EventLoop* loop,
           const muduo::net::InetAddress& listenAddr,
           const std::string& nameArg);
```

- 绑定 `onConnection` 回调（连接建立/断开事件）
- 绑定 `onMessage` 回调（数据到达事件）
- 调用 `_server.setThreadNum(4)` 设置 4 个 I/O worker 线程

### `void start()`

启动 TCP 服务器，开始监听端口。

### `void onConnection(const TcpConnectionPtr& conn)`

连接事件回调：
- 对端断开 (`!conn->connected()`) 时，调用 `ChatService::clientConnectException()` 清理用户状态
- 然后 `conn->shutdown()` 关闭连接

### `void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)`

消息到达回调，实现帧协议解包：

```cpp
while (buf->readableBytes() >= 4) {       // 至少有 4 字节长度头
    uint32_t beLen = 0;
    memcpy(&beLen, buf->peek(), 4);         // 读长度头（不消费）
    uint32_t msgLen = ntohl(beLen);         // 网络字节序 → 主机字节序

    if (msgLen > 1024 * 1024) {            // > 1MB 拒绝
        conn->shutdown(); return;
    }
    if (buf->readableBytes() < 4 + msgLen) {
        break;                              // 帧不完整，等下次数据
    }

    buf->retrieve(4);                       // 消费长度头
    std::string msg = buf->retrieveAsString(msgLen); // 消费 Payload

    auto js = nlohmann::json::parse(msg);
    auto msgHandler = ChatService::instance()->getHandler(js["msgId"]);
    msgHandler(conn, js, receiveTime);      // 分发到业务层
}
```

## 生命周期

- 构造 → `start()` → 进入 `loop.loop()` 事件循环
- 程序结束时 `SIGINT` 信号触发 `serverHandler()` → `reset()` 重置在线状态 → `exit(0)`
