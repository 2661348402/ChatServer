# Changelog

## 2026-07-15

### 并发优化

#### 群聊锁粒度优化

修改位置：
- `src/server/ChatService.cpp`

修改内容：
- 原来 `groupChat()` 在持有 `connMutex` 时执行数据库查询、Redis 发布、离线消息写入。
- 修改后锁只保护 `_userConnMap` 查询。
- MySQL、Redis、离线消息写入移动到锁外执行。

修改原因：
- `connMutex` 是在线连接表的全局锁。
- 在锁内执行阻塞 I/O 会导致其他登录、退出、私聊、断线清理操作被阻塞。
- 高并发场景下容易形成全局锁竞争。

优化效果：
- 缩短锁持有时间。
- 降低业务 I/O 对连接管理的影响。
- 提升群聊场景下的并发稳定性。

后续验证：
- 补充群聊并发压测。
- 对比优化前后的平均延迟、P95 延迟和吞吐量。


### 稳定性问题：Redis 订阅连接并发访问导致崩溃

#### 现象

在群聊压测结束阶段，大量客户端连接同时断开，服务端出现崩溃：

```text
free(): invalid pointer
Aborted (core dumped)

服务端日志显示多个用户几乎同时断开：
添加到对话
id: 43 disconnected
id: 38 disconnected
id: 47 disconnected
id: 41 disconnected
id: 48 disconnected

原因分析断线清理流程会调用：
cpp
ChatService::clientConnectException()
    -> Redis::unsubscribe()

当前 Redis 模块中，_subscribe_context 同时被多个线程访问：
Redis 订阅线程调用 redisGetReply(_subscribe_context, ...)
业务 IO 线程调用 redisAppendCommand(_subscribe_context, "UNSUBSCRIBE ...")
登录流程也会调用 redisAppendCommand(_subscribe_context, "SUBSCRIBE ...")
hiredis 的 redisContext 不是线程安全对象，不能被多个线程同时读写。同一个 _subscribe_context 被并发访问后，可能造成内部缓冲区状态破坏，最终触发 free(): invalid pointer。
修复方向:

给 Redis publish() 路径增加互斥保护，避免多个业务线程并发操作 _publish_context。
不再让业务线程直接操作 _subscribe_context。
将 subscribe() / unsubscribe() 请求提交到线程安全队列。
由 Redis 订阅线程串行处理订阅变更，并负责调用 redisGetReply() 接收消息。
保证 _subscribe_context 只被一个线程访问。

验证方式:
重复执行群聊压测。
压测结束时同时关闭 60 个在线连接。
观察服务端是否仍出现 free(): invalid pointer。
统计断连清理是否完整执行，用户状态是否正确更新为 offline。






