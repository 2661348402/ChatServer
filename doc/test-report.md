# Test Report

## 测试环境

- CPU：
- 内存：
- 操作系统：
- MySQL：
- Redis：
- Muduo：
- server.threads：
- db.poolsize：

## 功能测试

| 用例 | 操作 | 预期结果 | 实际结果 | 状态 |
| --- | --- | --- | --- | --- |
| 用户注册 | 新用户注册 | 返回用户 id |  | 正常 |
| 用户登录 | 正确账号密码登录 | 登录成功，返回好友/群组/离线消息 |  | 正常 |
| 私聊 | 在线用户互发消息 | 对方实时收到 |  | 正常 |
| 群聊 | 群成员发送消息 | 其他群成员收到 |  | 正常 |
| 离线消息 | 给离线用户发消息 | 用户下次登录收到 |  | 正常 |

### redis测试
#### 操作
1. 启动 Redis
2. 启动 server1:6001
3. 启动 server2:6000
4. 客户端 A 连接 6001，登录用户 101
5. 客户端 B 连接 6000，登录用户 102
6. 不要退出任意客户端
7. A 执行 chat:102:hello
8. B 应该显示消息
#### 结果
正常显示消息



###  并发测试: 群聊锁粒度优化

#### 对比表

QPS	版本	实际本地接收	缺失消息	吞吐量	平均延迟	P95	P99	错误数
10	改进前	35400	0	590.00 msg/s	6.25 ms	11.37 ms	51.38 ms	0
10	改进后	35400	0	590.00 msg/s	4.98 ms	7.75 ms	8.73 ms	0
30	改进前	38685	67515	644.75 msg/s	40014.35 ms	42135.12 ms	460748.11 ms	67515
30	改进后	57879	48321	964.65 msg/s	15162.07 ms	28828.79 ms	30053.85 ms	48321
100	改进前	63189	290811	1053.15 msg/s	26268.35 ms	49801.99 ms	51813.42 ms	290811
100	改进后	68499	285501	1141.65 msg/s	26597.27 ms	48906.59 ms	50905.15 ms	285501

#### 测试结论
- 优化 groupChat() 锁粒度后，服务端在 10 QPS 场景下保持零消息缺失，平均延迟从 6.25ms 降至 4.98ms，P99 从 51.38ms 降至 8.73ms。
- 在 30 QPS 场景下，实际本地投递数从 38685 提升到 57879，吞吐量从 644.75 msg/s 提升到 964.65 msg/s，平均延迟从 40.01s 降至 15.16s，说明缩短 connMutex 持有时间后，中等压力下的消息积压明显缓解。
- 在 100 QPS 场景下，系统仍出现严重积压，说明后续瓶颈主要转移到同步 MySQL 查询、离线消息写入和 Redis publish 调用，需要继续优化数据访问和跨节点转发路径。

### 并发测试: 业务线程池优化

#### 测试目的
- 验证 `ChatServer::onMessage()` 将业务处理投递到业务线程池后，原有群聊功能是否正常。
- 验证多连接并发发送时，业务线程池是否能提升处理吞吐。
- 观察高压场景下系统瓶颈是否仍集中在 `groupChat()` 的 MySQL 查询、离线消息写入和 Redis publish 路径。

#### 测试命令

单发送者，100 QPS：

```bash
python3 scripts/group_chat_benchmark.py \
  --server-host 127.0.0.1 \
  --server-port 6001 \
  --db-user root \
  --db-password 123456 \
  --duration 60 \
  --qps 100
```

8 个发送者，100 QPS，等待 30 秒处理积压消息：

```bash
python3 scripts/group_chat_benchmark.py \
  --server-host 127.0.0.1 \
  --server-port 6001 \
  --db-user root \
  --db-password 123456 \
  --duration 60 \
  --qps 100 \
  --senders 8 \
  --drain-time 30
```

#### 测试结果

QPS	发送者数量	实际本地接收	期望本地接收	缺失消息	吞吐量	平均延迟	P95	P99	错误数
10	1	35400	35400	0	590.00 msg/s	5.80 ms	11.77 ms	25.80 ms	0
100	1	59885	354000	294115	998.08 msg/s	27008.55 ms	50197.99 ms	52314.53 ms	294115
100	8	88205	312000	223795	1470.07 msg/s	38839.59 ms	71094.71 ms	74365.02 ms	223795

#### 测试结论
- 10 QPS 场景下消息完整投递，说明业务线程池改造后基础群聊功能正常。
- 100 QPS 单发送者场景下吞吐约 `998.08 msg/s`，缺失消息较多，说明单连接热点任务固定落到同一个 worker，不能充分利用全部业务线程。
- 100 QPS、8 发送者场景下吞吐提升到 `1470.07 msg/s`，本地接收消息数从 `59885` 提升到 `88205`，说明多连接场景下业务线程池能够并行处理任务。
- 8 发送者场景延迟更高，是因为 `--drain-time` 从 3 秒增加到 30 秒，脚本统计到了更多排队很久才到达的消息，长尾延迟被放大。
- 高压下仍然存在明显积压，说明当前主要瓶颈已经从 IO 线程转移到 `groupChat()` 业务路径。
- 下一步应优先优化群聊数据访问：
  - 避免每条群聊消息都重复查询群成员。
  - 去掉对非本地用户逐个 `_userModel.queryById()` 的 MySQL 查询。
  - 离线消息改为批量插入。
  - Redis publish 考虑异步队列化。


### 群组查询mysql优化测试
#### 结果

  rows returned per login: 5100
  avg time: 3781.33 ms
  best time: 3541.02 ms
  worst time: 3966.59 ms
Optimized JOIN query:
  SQL requests per login: 1
  rows returned per login: 5000
  avg time: 65.85 ms
  best time: 50.00 ms
  worst time: 111.62 ms

Comparison:
  SQL requests reduced: 101 -> 1 (-100)
  average speedup: 57.42x

EXPLAIN for optimized JOIN:
id      select_type     table   partitions      type    possible_keys   key     key_len ref     rows    filtered        Extra
1       SIMPLE  gm      NULL    ALL     PRIMARY,idx_group_userid        NULL    NULL    NULL    5101    100.00  Using temporary; Using filesort
1       SIMPLE  self    NULL    eq_ref  PRIMARY,idx_group_userid        PRIMARY 8       chat.gm.groupid,const   1       100.00  Using index
1       SIMPLE  g       NULL    eq_ref  PRIMARY PRIMARY 4       chat.gm.groupid 1       100.00  NULL
1       SIMPLE  u       NULL    eq_ref  PRIMARY PRIMARY 4       chat.gm.userid  1       100.00  NULL

##### 测试结论
旧方案：101 次 SQL，平均 3781.33 ms
新方案：1 次 SQL，平均 65.85 ms
SQL 次数：101 -> 1
平均提速：57.42x

### Redis 容错与消息降级增强测试
#### 步骤
1. 启动MySQL、Redis、两个 ChatServer节点。
2. 用户A登录节点1，用户B登录节点2。
3. A 给 B 发消息，确认 Redis 正常时能跨节点收到。
4. 关闭 Redis。
5. A 再给 B 发消息，服务端不能崩溃。
6. 查询offlineMessage 表，确认消息进入离线消息。
7. 重启 Redis。
8. 观察日志，确认订阅线程重连并重新订阅。
9. 再发跨节点消息，确认 Redis 恢复后可以继续发布/订阅。
#### 结果
正常


### 心跳包测试
1. 正常心跳测试测试步骤：
启动服务端。
启动 Qt 客户端。
使用用户 101 登录。
保持客户端在线 2 分钟以上。
预期结果：
服务端登录成功后订阅 Redis 用户频道。
Qt 客户端每 30 秒发送一次 PING_MSG。
服务端收到心跳后打印心跳日志。
用户不会被误判为超时离线。
服务端日志示例：
添加到对话
do login thing
redis process command: SUBSCRIBE 101
heartbeat received, userid=101
heartbeat received, userid=101

测试结果：通过。
2. 停止心跳超时测试测试步骤：
临时注释 Qt 客户端登录成功后的 startHeartbeat()。
重新编译 Qt 客户端。
登录用户 101。
保持客户端连接但不发送心跳。
等待 90 到 100 秒。
预期结果：
服务端在超时时间后判定用户心跳超时。
服务端主动关闭连接。
用户状态被更新为 offline。

### 并发测试: 群聊热路径优化

#### 测试目的
- 验证群成员缓存、批量用户状态查询、批量离线消息插入和 Redis publish 队列化后，群聊高压场景是否还会严重积压。
- 对比优化前后在 100 人群聊、8 个发送者、100 QPS 场景下的吞吐、延迟和消息完整性。
- 验证修正后的压测脚本在多发送者场景下能正确计算期望本地消息数。

#### 测试场景

- `groupId`: 1
- 群总用户数：100
- 本机在线用户：60
- 远端在线用户：20
- 离线用户：20
- 发送者数量：8
- 压测时长：60 秒
- 目标发送速率：100 QPS
- 发送完成后等待处理积压：30 秒

#### 测试命令

```bash
python3 scripts/group_chat_benchmark.py \
  --server-host 127.0.0.1 \
  --server-port 6001 \
  --db-user root \
  --db-password 123456 \
  --duration 60 \
  --qps 100 \
  --senders 8 \
  --drain-time 30
```

#### 压测脚本修正

多发送者场景下，每条群消息只应该排除当前发送者本人，其他发送者仍然是群成员，也应该收到消息。

因此 `expected local messages` 的计算从：

```python
args.local_online - len(sender_ids)
```

修正为：

```python
args.local_online - 1
```

以 60 个本机在线用户、6000 条群消息为例，正确期望值为：

```text
6000 * 59 = 354000
```

#### 优化前结果

```text
sent messages: 5999
received local messages: 100536
expected local messages: 311948
throughput: 1675.33 msg/s
avg latency: 36748.88 ms
min latency: 1.97 ms
p95 latency: 69363.04 ms
p99 latency: 72231.43 ms
max latency: 72949.16 ms
parse errors: 0
disconnects: 0
messages without timestamp: 0
missing local messages: 211412
total errors: 211412
```

#### 优化后结果

单发送者，100 QPS：

```text
sent messages: 6000
received local messages: 354000
expected local messages: 354000
throughput: 5899.96 msg/s
avg latency: 4.24 ms
min latency: 0.28 ms
p95 latency: 6.62 ms
p99 latency: 8.85 ms
max latency: 113.78 ms
parse errors: 0
disconnects: 0
messages without timestamp: 0
missing local messages: 0
total errors: 0
```

8 个发送者，100 QPS：

```text
sent messages: 6000
received local messages: 354000
expected local messages: 354000
throughput: 5898.99 msg/s
avg latency: 11.41 ms
min latency: 0.28 ms
p95 latency: 33.23 ms
p99 latency: 217.68 ms
max latency: 399.76 ms
parse errors: 0
disconnects: 0
messages without timestamp: 0
missing local messages: 0
total errors: 0
```

#### 优化前后对比

| 场景 | 发送消息数 | 实际本地接收 | 期望本地接收 | 缺失消息 | 吞吐量 | 平均延迟 | P95 | P99 | 错误数 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 优化前，8 发送者，100 QPS | 5999 | 100536 | 311948 | 211412 | 1675.33 msg/s | 36748.88 ms | 69363.04 ms | 72231.43 ms | 211412 |
| 优化后，8 发送者，100 QPS | 6000 | 354000 | 354000 | 0 | 5898.99 msg/s | 11.41 ms | 33.23 ms | 217.68 ms | 0 |
| 优化后，1 发送者，100 QPS | 6000 | 354000 | 354000 | 0 | 5899.96 msg/s | 4.24 ms | 6.62 ms | 8.85 ms | 0 |

#### 测试结论
- 群聊热路径优化后，8 发送者、100 QPS 场景下，本机消息全部送达，缺失消息从 `211412` 降为 `0`。
- 吞吐量从 `1675.33 msg/s` 提升到 `5898.99 msg/s`，约提升 `3.5x`。
- 平均延迟从 `36748.88 ms` 降到 `11.41 ms`，长时间排队问题基本消除。
- P99 延迟从 `72231.43 ms` 降到 `217.68 ms`，长尾延迟明显改善。
- 单发送者 100 QPS 场景下，P99 为 `8.85 ms`，说明在没有多发送者竞争时群聊路径延迟更稳定。
- 8 发送者场景仍存在一定长尾抖动，后续可以继续关注本机连接发送循环、连接表锁竞争和 Redis 发布队列积压情况。

#### 优化原因分析
- 群成员缓存减少了 `GroupUser` 表重复查询。
- 批量用户状态查询将非本地用户的 N 次 MySQL 查询合并为 1 次。
- 批量离线消息插入将多次单行写入合并为一次多行插入。
- Redis publish 队列化避免群聊 worker 被跨节点转发网络 I/O 阻塞。
- 这些改动共同减少了 `groupChat()` 热路径中的同步阻塞操作，因此吞吐、平均延迟、长尾延迟和消息完整性都有明显改善。
