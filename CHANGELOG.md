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

## 2026-07-21

### 连接保活优化

#### 心跳机制与异常掉线检测

修改位置：
- `include/server/ChatService.hpp`
- `src/server/ChatService.cpp`
- `src/server/ChatServer.cpp`
- `src/qt-client/models/ChatData.h`
- `src/qt-client/network/ProtocolClient.h`
- `src/qt-client/network/ProtocolClient.cpp`

修改内容：
- 使用已有的 `PING_MSG` 和 `PONG_MSG` 作为心跳消息类型。
- Qt 客户端登录成功后启动心跳定时器，每 30 秒向服务端发送一次 `PING_MSG`。
- 服务端收到合法心跳后，更新 `_lastActiveMap` 中用户最近活跃时间。
- 服务端通过 Muduo `EventLoop::runEvery()` 每 10 秒扫描一次心跳超时用户。
- 如果用户超过 90 秒未发送心跳，服务端判定该连接失效。
- 心跳超时后，服务端会删除 `_userConnMap` 和 `_lastActiveMap` 中的用户记录，取消 Redis 订阅，并将 MySQL 中用户状态更新为 `offline`。
- 将用户离线清理逻辑抽取为 `setUserOffline()`，避免 `clientConnectException()` 和心跳超时处理重复维护 Redis 与数据库状态。

修改原因：
- 原项目主要依赖 TCP 连接断开回调处理用户离线。
- 客户端正常退出或进程关闭时，服务端可以通过 `clientConnectException()` 清理用户状态。
- 但在断网、网络链路异常、客户端卡死、TCP 半连接等场景下，服务端可能无法及时收到连接关闭事件。
- 这会导致用户长时间显示为 `online`，影响好友状态、重复登录判断和系统可靠性。

优化效果：
- 服务端可以主动发现长时间无响应的连接。
- 异常掉线用户能够被自动置为 `offline`。
- 避免用户因为 TCP 半连接长期保持假在线状态。
- 心跳检测逻辑与正常断线清理逻辑解耦，离线状态更新更加统一。
- 连接管理更加符合聊天系统对在线状态实时性的要求。

验证：
- 正常登录 Qt 客户端，保持在线 2 分钟以上。
- 服务端每 30 秒左右收到一次心跳日志：

```text
面试：
我给聊天服务器增加了心跳与连接保活机制。原来服务端主要依赖 TCP 断开回调来判断用户离线，这在客户端正常退出或者进程关闭时是有效的，因为 Muduo 会触发连接关闭回调，服务端可以清理连接表、取消 Redis 订阅并更新 MySQL 用户状态。

但是在断网、网络链路异常、客户端卡死或者 TCP 半连接场景下，服务端不一定能及时收到连接关闭事件。这样用户可能长时间显示为 online，造成假在线问题。

我的做法是让 Qt 客户端登录成功后定时发送 PING_MSG，服务端收到后更新用户最近活跃时间。服务端再通过 Muduo 的 runEvery() 注册一个定时任务，每 10 秒扫描一次在线用户。如果某个用户超过 90 秒没有心跳，就认为连接已经失效。

超时后，服务端会从 _userConnMap 删除连接记录，从 _lastActiveMap 删除心跳记录，取消 Redis 订阅，并把 MySQL 中用户状态更新为 offline。为了避免重复代码，我把 Redis 取消订阅和数据库状态更新抽成了 setUserOffline()，正常断线和心跳超时都复用这套逻辑。

这个优化解决的是 TCP 连接没有及时断开但业务上用户已经不可用的问题，提高了用户在线状态的准确性，也让系统在异常网络场景下更加可靠。
```

## 2026-07-21

### 业务线程池优化

#### IO 线程与业务线程解耦

修改位置：
- `include/server/ChatServer.hpp`
- `src/server/ChatServer.cpp`
- `include/server/util/ThreadPool.hpp`
- `src/server/util/ThreadPool.cpp`
- `CMakeLists.txt`
- `conf/server.conf`
- `conf/server2.conf`

修改内容：
- 新增 `ThreadPool` 公共工具模块，用于处理登录、注册、私聊、群聊、心跳等业务任务。
- `ChatServer::onMessage()` 保留网络层职责，只负责 TCP 拆包、JSON 解析、消息类型识别和任务投递。
- `ChatService` 中的业务处理逻辑从 Muduo IO 回调线程转移到业务线程池中执行。
- 线程池按连接对象地址计算分片 key，同一个 TCP 连接的消息固定进入同一个 worker 队列。
- 保证同一连接上的消息按顺序执行，避免登录、发消息、退出等操作乱序。
- 将 Muduo IO 线程数从硬编码 `4` 改为读取 `server.threads` 配置。
- 新增业务线程池配置：
  - `business.threads`
  - `business.queue_size`
- 项目 C++ 标准升级到 C++17，使用 lambda move capture 等更现代的写法，减少不必要的对象复制。

修改原因：
- 原实现中，`ChatServer::onMessage()` 解析出业务消息后直接调用 `ChatService` 对应处理函数。
- 登录、群聊、离线消息、Redis 发布、MySQL 查询等阻塞操作可能直接占用 Muduo IO 线程。
- IO 线程被业务逻辑阻塞后，会影响连接读写、消息拆包、断线回调等网络事件处理。
- 高并发场景下，网络层和业务层耦合过紧，不利于提升连接处理能力。

优化效果：
- Muduo IO 线程主要负责网络事件处理，不再直接执行复杂业务逻辑。
- 数据库访问、Redis 发布和群聊分发进入业务线程池执行。
- 多连接并发发送时，业务任务可以分散到多个 worker 并行处理。
- 单连接消息顺序得到保证，避免普通线程池随机调度造成业务乱序。
- 线程模型更加清晰，便于解释 Reactor 模型与业务线程池之间的职责分工。

验证：
- 通过 10 QPS 群聊压测，确认线程池改造后基础群聊功能和消息顺序正常。
- 对比 100 QPS 单发送者和 8 发送者压测，确认业务线程池在多连接场景下能够提升吞吐。
- 高压下仍然存在明显积压，说明后续瓶颈主要集中在 `groupChat()` 内部的数据访问和跨节点转发路径。
- 详细压测数据记录在 `doc/test-report.md`。

面试：
```text
我对服务器线程模型做了一次解耦优化。原来 Muduo 的 onMessage() 在 IO 线程中完成拆包和 JSON 解析后，会直接调用 ChatService 的业务函数。这样登录、群聊、数据库查询、Redis 发布和离线消息写入都有可能占用 IO 线程。

这种设计在低并发下问题不明显，但高并发时 IO 线程会被业务阻塞，导致网络事件处理变慢。我的优化方式是在网络层和业务层之间增加业务线程池。IO 线程只负责接收数据、按长度帧拆包、解析消息类型，然后把业务任务投递到线程池，具体的登录、私聊、群聊和心跳处理由业务线程执行。

线程池没有采用完全随机的任务分配，而是按连接对象地址做分片。同一个 TCP 连接的消息固定进入同一个 worker 队列，这样可以保证单连接内的消息顺序。例如登录、发送消息、退出登录不会因为并发调度而乱序执行；不同连接之间仍然可以并行处理。

压测结果显示，在 10 QPS 群聊场景下消息可以完整投递；在 100 QPS 下，单发送者场景吞吐约 998 msg/s，8 个发送者场景吞吐提升到 1470 msg/s，说明多连接情况下业务线程池确实生效。

不过高压下仍然存在明显积压，这说明当前瓶颈已经不在 IO 线程，而在 groupChat() 业务路径本身。每条群聊消息仍然会查询群成员、逐个查询非本地用户状态、逐条写离线消息并执行 Redis publish。下一步优化重点应该转向群成员缓存、批量用户状态查询、离线消息批量插入和 Redis 发布异步化。
```

## 2026-07-22

### 群聊热路径优化

#### 数据访问与跨节点转发优化

修改位置：
- `include/server/model/OfflineMessageModel.hpp`
- `src/server/model/OfflineMessageModel.cpp`
- `include/server/model/UserModel.hpp`
- `src/server/model/UserModel.cpp`
- `include/server/model/GroupModel.hpp`
- `src/server/model/GroupModel.cpp`
- `include/server/ChatService.hpp`
- `src/server/ChatService.cpp`
- `scripts/group_chat_benchmark.py`

修改内容：
- 为 `OfflineMsgModel` 增加批量离线消息写入接口，将群聊中的多次离线消息插入合并为一次批量 `INSERT`。
- 为 `UserModel` 增加批量用户状态查询接口，去掉 `groupChat()` 中对非本地用户逐个 `_userModel.queryById()` 的查询。
- 为群成员列表增加缓存，`groupChat()` 优先从内存读取群成员，缓存未命中时再查询 `GroupUser` 表。
- 在 `addGroup()` 等群成员变化路径中失效对应群的成员缓存，避免新成员无法收到后续群消息。
- 将 Redis publish 改为队列化处理，群聊业务线程只投递发布任务，后台线程负责执行 Redis 发布。
- Redis 发布失败时，后台线程将消息降级写入离线消息表，避免跨节点消息丢失。
- 修正 `scripts/group_chat_benchmark.py` 在多发送者场景下的期望本地消息数计算。每条消息只排除当前发送者本人，其他发送者也应收到群消息。

修改原因：
- 业务线程池改造后，网络 IO 已经与业务逻辑解耦，但高压群聊仍然存在严重积压。
- 原 `groupChat()` 每条消息都会查询群成员，增加 MySQL 访问频率。
- 对非本地用户逐个查询用户状态，会在大群场景形成 N 次 MySQL 查询。
- 离线用户较多时逐条写入 `OfflineMessage`，数据库写入次数随离线人数线性增长。
- Redis publish 是同步调用，Redis 网络抖动或跨节点消息较多时会阻塞群聊 worker。
- 这些问题叠加后，100 QPS 多发送者群聊场景会出现长时间排队和大量消息未及时送达。

优化效果：
- 群成员查询从每条消息访问 MySQL 优化为缓存命中读取。
- 非本地用户状态查询从 N 次 SQL 优化为 1 次批量 SQL。
- 离线消息写入从多次单行插入优化为一次批量插入。
- Redis 跨节点转发从同步阻塞调用优化为后台队列处理。
- 100 人群聊、60 个本机在线用户、20 个远端在线用户、20 个离线用户、8 个发送者、100 QPS 场景下：
  - 吞吐从 `1675.33 msg/s` 提升到 `5898.99 msg/s`，约 `3.5x`。
  - 平均延迟从 `36748.88 ms` 降到 `11.41 ms`。
  - P99 延迟从 `72231.43 ms` 降到 `217.68 ms`。
  - 缺失消息从 `211412` 降到 `0`。

验证：
- 使用原有 `scripts/group_chat_benchmark.py` 进行压测。
- 单发送者、100 QPS 场景下，本机消息全部送达，平均延迟 `4.24 ms`，P99 `8.85 ms`。
- 8 个发送者、100 QPS 场景下，本机消息全部送达，平均延迟 `11.41 ms`，P99 `217.68 ms`。
- 压测过程中无解析错误、无断连、无缺失消息。
- 详细压测数据记录在 `doc/test-report.md`。

面试：
```text
我在线程池优化之后继续压测群聊，发现瓶颈已经转移到 groupChat() 的业务热路径。原来的群聊处理每条消息都会查一次群成员，对非本地用户还会逐个 queryById() 查询状态；如果目标用户离线，又会逐条写 OfflineMessage；如果目标用户在线但在其他节点，还会同步调用 Redis publish。这个路径里同时存在重复 MySQL 查询、逐条数据库写入和同步 Redis 网络 I/O，所以在 100 QPS 多发送者场景下会出现严重积压。

我这次主要做了四个优化。

第一，给群成员列表加缓存。群成员在聊天过程中变化频率远低于消息发送频率，所以 groupChat() 不需要每条消息都查 GroupUser 表。缓存未命中时查一次数据库，后续直接读内存；当有人加群时删除对应 groupId 的缓存，下一次再加载最新成员。

第二，去掉循环里的用户状态单查。以前对非本地用户逐个 _userModel.queryById()，一个 100 人群里可能一条消息触发几十次 SQL。我增加了 queryStatesByIds()，一次性查询所有非本地用户状态，把 N 次 SQL 合并成 1 次。

第三，离线消息改成批量插入。群聊里如果有 20 个离线用户，原来会执行 20 次 insert。现在先收集离线用户 ID，最后用一条 insert values (...), (...), (...) 写入多条记录，减少数据库往返和事务开销。

第四，Redis publish 队列化。Redis 是跨节点实时转发通道，publish 属于网络 I/O，不应该阻塞群聊 worker。现在 groupChat() 只把发布任务放进队列，后台线程负责 publish；如果 publish 失败，再降级写离线消息，保证跨节点消息不会直接丢失。

优化前，在 100 人群、60 个本机在线、20 个远端在线、20 个离线、8 个发送者、100 QPS 的测试下，吞吐只有约 1675 msg/s，平均延迟约 36.7 秒，P99 超过 72 秒，并且有 21 万条本机消息未及时送达。优化后，同样场景下吞吐提升到约 5899 msg/s，平均延迟降到 11.41 ms，P99 降到 217.68 ms，缺失消息为 0。

这说明系统瓶颈从“每条消息大量同步 I/O”变成了“缓存读取、批量查询、批量落库和异步转发”，群聊热路径的吞吐和稳定性都有明显提升。
```

