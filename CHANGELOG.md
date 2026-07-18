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

验证：
- 补充群聊并发压测。
- 对比优化前后的平均延迟、P95 延迟和吞吐量。

面试：
```text
我在群聊压测时发现，groupChat() 原来的实现把整个群聊转发流程都放在 connMutex 里面执行。这个锁本来只是用来保护在线连接表 _userConnMap，但原代码在持锁期间还做了 MySQL 查询、Redis publish 和离线消息写入。

这些操作都属于阻塞 I/O，耗时不可控。如果它们发生在全局连接锁里面，就会影响其他线程访问 _userConnMap，比如登录、退出、私聊转发和断线清理。高并发下，这会导致连接管理路径被群聊业务拖慢，形成全局锁竞争。

我的优化方式是缩短锁的保护范围。锁只负责读取 _userConnMap，判断目标用户是否在当前节点在线；如果在线，就拿到对应连接并发送。对于不在当前节点的用户，先把用户 ID 收集到一个临时列表，然后释放锁。锁释放后，再去查询用户状态、Redis 发布或者写离线消息。

这样 connMutex 只保护共享内存结构，不再覆盖 MySQL、Redis 这些阻塞 I/O。优化后锁持有时间明显缩短，群聊转发不会长时间阻塞登录、退出和断线清理。

```
### 稳定性问题：Redis 订阅连接并发访问导致崩溃

现象:

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
```

原因分析：断线清理流程会调用：

```cpp
ChatService::clientConnectException()
    -> Redis::unsubscribe()
```
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

面试：
```text
我在做群聊并发压测时遇到过一个稳定性问题：压测结束阶段大量客户端同时断开，服务端偶发崩溃，错误是 free(): invalid pointer。日志里能看到多个用户几乎同时触发 disconnected 清理。

我沿着断线流程排查，发现 clientConnectException() 会调用 Redis::unsubscribe()。而 Redis 模块里有两个 hiredis context：一个用于 publish，一个用于 subscribe。问题出在 _subscribe_context：订阅线程正在 redisGetReply(_subscribe_context, ...) 阻塞读取消息，同时业务 IO 线程在用户登录/退出时也会调用 redisAppendCommand(_subscribe_context, "SUBSCRIBE ...") 或 "UNSUBSCRIBE ..."。

hiredis 的 redisContext 不是线程安全对象，不能多个线程同时读写。同一个 _subscribe_context 被订阅线程和业务线程并发访问后，内部输入输出缓冲区可能被破坏，所以最终出现 free(): invalid pointer 这种内存错误。
```

## 2026-07-16
### 登录链路与群组查询优化

- 优化 `GroupModel::queryGroups()`，将原来的 N+1 查询改为单条 JOIN 查询。
- 使用 `GroupUser` 自连接：
  - `self` 用于定位当前用户加入的群。
  - `gm` 用于展开这些群的全部成员。
- 服务端按 `groupId` 对 JOIN 结果做内存聚合，保持原有 `Group -> GroupUser` 返回结构不变。
- 确认关键查询路径已有索引覆盖：
  - `Friend.userid` 使用 `PRIMARY(userid, friendid)`。
  - `GroupUser.groupid` 使用 `PRIMARY(groupid, userid)`。
  - `GroupUser.userid` 使用 `idx_group_userid(userid)`。
  - `OfflineMessage.userid` 使用 `idx_offline_userid(userid)`。
- 新增群组查询基准脚本，对比旧 N+1 查询和新 JOIN 查询的性能差异。
- 在 100 个群、每群 50 个成员的测试场景下：
  - SQL 请求数从 `101` 次降到 `1` 次。
  - 平均耗时从 `3781.33 ms` 降到 `65.85 ms`。
  - 平均提速约 `57.42x`。

  面试：
```text
我在优化登录链路时发现，登录成功后服务端会加载用户的好友、群组和离线消息，其中群组查询存在 N+1 查询问题。

旧实现是先查询当前用户加入了哪些群，然后对每个群再单独查询一次群成员。如果用户加入 N 个群，就需要 1 + N 次 SQL。群数量变多时，登录阶段数据库往返次数会线性增长，延迟也会明显增加。

我把这部分改成了一条 JOIN 查询。具体做法是让 GroupUser 表做自连接：一份别名 self 用来根据当前登录用户 ID 找出他加入的群，另一份别名 gm 用来展开这些群里的全部成员，再 JOIN user 表拿到成员昵称和在线状态。查询结果是一张扁平表，服务端再按 groupId 聚合回原来的 Group -> GroupUser 结构，所以对上层登录响应格式没有影响。

同时我检查了相关索引：GroupUser.groupid 由联合主键覆盖，GroupUser.userid 有独立索引，Friend.userid 和 OfflineMessage.userid 也都有对应索引，保证登录路径上的查询可以走索引。

我还写了基准脚本验证效果。在 100 个群、每群 50 个成员的场景下，旧方案需要 101 次 SQL，平均耗时约 3781 ms；新方案只需要 1 次 SQL，平均耗时约 66 ms，SQL 往返次数减少 100 次，平均提速约 57 倍。
```
## 2026-07-18

### Redis 容错与消息降级增强

#### 修改内容

- 为 Redis 发布连接增加失败重连机制：
  - `publish()` 发送失败后，先释放旧连接。
  - 尝试重新连接 Redis。
  - 重连成功后重试一次 `PUBLISH`。
  - 重试仍失败则返回 `false`，交给业务层处理降级。

- 为 Redis 订阅连接增加自动恢复机制：
  - 订阅线程检测到 `redisGetReply()` 失败后，不再无限刷错误日志。
  - 连接异常时释放 `_subscribe_context` 并置空。
  - 等待一段时间后尝试重新连接 Redis。
  - Redis 恢复后重新建立订阅连接。

- 增加订阅频道状态保存：
  - 用户登录订阅频道时，将频道记录到 `_subscribedChannels`。
  - 用户退出或断开连接时，从 `_subscribedChannels` 移除频道。
  - Redis 订阅连接重建后，通过 `resubscribeAll()` 恢复所有仍在线用户的频道订阅。


- 增加跨节点消息降级逻辑：
  - 私聊和群聊中，目标用户状态为 `online` 但 Redis 发布失败时，不再直接丢弃消息。
  - Redis 发布失败后，将消息写入 `OfflineMessage` 表。
  - 保证 Redis 不可用时跨节点消息仍可通过离线消息机制保留。

- 补充关键日志：
  - Redis 发布失败
  - Redis 发布重连
  - Redis 发布重试失败
  - Redis 订阅连接异常
  - Redis 订阅重连
  - Redis 频道重新订阅
  - Redis 不可用时降级写入离线消息

#### 修改原因

Redis Pub/Sub 只是跨服务器实时消息通道，不具备消息持久化能力。  
当 Redis 异常或重启时，原有发布和订阅连接都会失效：

- 如果 `publish()` 失败后不处理，跨节点消息会直接丢失。
- 如果订阅线程异常退出，Redis 恢复后服务端也无法继续接收跨节点消息。
- 如果重连后不重新订阅用户频道，新的 Redis 连接仍然收不到消息。

因此需要将 Redis 故障处理分为两层：

- Redis 层负责连接重建、订阅线程恢复、频道重新订阅。
- 业务层负责消息可靠性兜底，发布失败时写入离线消息表。

#### 验收标准
- Redis 关闭时，服务端不崩溃。
- Redis 关闭后发送跨节点私聊或群聊，消息能写入 `OfflineMessage`。
- Redis 恢复后，服务端能重新建立发布连接。
- Redis 恢复后，订阅线程能重新订阅在线用户频道。
- 日志中能看到 Redis 故障、重连、重新订阅、降级写离线消息的完整过程。

#### 面试
```test
我在项目里对 Redis Pub/Sub 做了容错增强。原来 Redis 只是作为跨服务器消息转发通道使用，如果 Redis 挂掉，publish() 失败后消息可能直接丢失；订阅线程如果因为连接异常退出，Redis 恢复后服务端也无法继续接收跨节点消息。

我的优化分两层做。

第一层是 Redis 封装层。发布消息时，如果 PUBLISH 失败，我会释放旧的 publish 连接，尝试重新连接 Redis，然后重试一次发布。如果重试仍然失败，就返回 false，不在 Redis 层死循环重试，避免阻塞业务线程。

第二层是订阅恢复。订阅线程里会检测 redisGetReply() 的返回值。一旦发现订阅连接异常，就释放旧的 subscribe 连接并置空，等待一小段时间后重连 Redis。因为 Redis Pub/Sub 的订阅关系是绑定在连接上的，所以重连后还要重新订阅当前在线用户的频道。为此我维护了一个 _subscribedChannels 集合，用户登录时加入，退出时删除，Redis 恢复后遍历这个集合重新 SUBSCRIBE。

另外，为了避免多线程竞争 hiredis 的订阅连接，我没有让业务线程直接操作 _subscribe_context。登录和退出时只是把订阅或取消订阅请求放进队列，真正的 SUBSCRIBE / UNSUBSCRIBE 由订阅线程统一执行。这样订阅连接始终只被一个线程操作。

业务层也做了兜底。私聊和群聊时，如果目标用户状态是 online，说明他可能连接在其他服务器节点上，需要通过 Redis 转发。如果 Redis 发布失败，我不会直接丢弃消息，而是降级写入 OfflineMessage 表。这样 Redis 不可用时，跨节点消息虽然不能实时送达，但不会丢，用户后续登录或拉取离线消息时还能收到。

可以总结成一句话：

Redis 在我的系统里只是实时转发通道，不是可靠存储。Redis 故障时，连接层负责自动恢复，业务层负责消息降级落库，保证服务不崩、消息不丢、Redis 恢复后跨节点通信可以继续工作。
```