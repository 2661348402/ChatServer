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

### 并发测试: 可观测性指标与群聊瓶颈定位

#### 测试目的

- 验证新增服务端指标是否能正确反映连接数、在线用户数、消息解析数、群聊处理数、Redis 发布数和离线消息落库情况。
- 通过 `avg_group_us`、`avg_local_send_us`、`avg_redis_us`、`avg_offline_us`、`max_group_us`、`max_offline_us` 定位群聊长尾延迟来源。
- 对比 100 QPS、200 QPS、500 QPS 下系统吞吐、延迟、消息完整性和服务端处理能力。

#### 指标说明

| 指标 | 含义 |
| --- | --- |
| `conn` | 当前 TCP 连接数 |
| `users` | 当前本节点在线用户数 |
| `parsed` | 服务端成功解析的 JSON 消息数量 |
| `parse_errors` | JSON 解析失败数量 |
| `group_chat` | 已完成处理的群聊消息数量 |
| `redis_publish` | Redis 跨节点发布次数 |
| `redis_fail` | Redis 发布失败次数 |
| `offline_degrade` | Redis 发布失败后降级写入离线消息次数 |
| `avg_group_us` | 单条群聊消息服务端平均处理耗时 |
| `avg_local_send_us` | 本机连接发送循环平均耗时 |
| `avg_redis_us` | 单次 Redis publish 平均耗时 |
| `avg_offline_us` | 单批离线消息落库平均耗时 |
| `offline_store_batch` | 离线消息批量落库调用次数 |
| `offline_store_rows` | 实际写入离线消息行数 |
| `max_group_us` | 单条群聊消息最大处理耗时 |
| `max_offline_us` | 单批离线消息最大落库耗时 |

#### 测试场景

- `groupId`: 1
- 群总用户数：100
- 本机在线用户：60
- 远端在线用户：20
- 离线用户：20
- 发送者数量：8
- 压测时长：60 秒

#### 测试命令

100 QPS：

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

200 QPS：

```bash
python3 scripts/group_chat_benchmark.py \
  --server-host 127.0.0.1 \
  --server-port 6001 \
  --db-user root \
  --db-password 123456 \
  --duration 60 \
  --qps 200 \
  --senders 8 \
  --drain-time 30
```

500 QPS：

```bash
python3 scripts/group_chat_benchmark.py \
  --server-host 127.0.0.1 \
  --server-port 6001 \
  --db-user root \
  --db-password 123456 \
  --duration 60 \
  --qps 500 \
  --senders 8 \
  --drain-time 60
```

#### 客户端压测结果

| QPS | 发送消息数 | 实际本地接收 | 期望本地接收 | 缺失消息 | 吞吐量 | 平均延迟 | P95 | P99 | 最大延迟 | 解析错误 | 断连 | 总错误 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 100 | 6000 | 354000 | 354000 | 0 | 5899.95 msg/s | 5.39 ms | 14.74 ms | 33.66 ms | 55.38 ms | 0 | 60 | 60 |
| 200 | 12000 | 708000 | 708000 | 0 | 11799.73 msg/s | 9.70 ms | 31.00 ms | 58.33 ms | 202.08 ms | 0 | 0 | 0 |
| 500 | 30000 | 798757 | 1770000 | 971243 | 13312.52 msg/s | 7923.00 ms | 14688.60 ms | 15414.12 ms | 16194.30 ms | 0 | 60 | 971303 |

说明：100 QPS 和 500 QPS 中的 `disconnects=60` 主要来自压测结束时脚本主动关闭 60 个客户端连接，不能直接等同于服务端异常断连。500 QPS 的核心问题是 `missing local messages=971243`。

#### 服务端指标结果

| QPS | parsed | group_chat | redis_publish | redis_fail | offline_degrade | offline_store_batch | offline_store_rows | avg_group_us | avg_local_send_us | avg_redis_us | avg_offline_us | max_group_us | max_offline_us |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 100 | 6060 | 6000 | 120000 | 0 | 0 | 6000 | 120000 | 4620 | 154 | 79 | 3357 | 29038 | 27848 |
| 200 | 12060 | 12000 | 240000 | 0 | 0 | 12000 | 240000 | 3658 | 136 | 71 | 2734 | 88051 | 55474 |
| 500 | 30060 | 14078 | 281560 | 0 | 0 | 14078 | 281560 | 4190 | 154 | 85 | 3236 | 30558 | 29468 |

#### 结果分析

- 100 QPS 和 200 QPS 场景下，`group_chat` 分别等于发送消息数 `6000` 和 `12000`，说明服务端完成了全部群聊处理。
- 100 QPS 和 200 QPS 场景下，`redis_publish` 分别等于 `发送消息数 * 20`，`offline_store_rows` 也等于 `发送消息数 * 20`，说明远端在线用户转发和离线用户落库数量符合预期。
- Redis 发布平均耗时稳定在 `71us` 到 `85us`，且 `redis_fail=0`，说明当前测试下 Redis 不是主要瓶颈。
- 本机连接发送平均耗时稳定在 `136us` 到 `154us`，说明本机发送循环不是主要瓶颈。
- 离线消息批量落库平均耗时为 `2.7ms` 到 `3.3ms`，明显高于本机发送和 Redis publish，是 100/200 QPS 场景下群聊热路径中的主要耗时来源。
- 200 QPS 场景下 `max_group_us=88051`、`max_offline_us=55474`，说明长尾延迟主要来自离线消息落库，但也包含业务线程调度和排队开销。
- 500 QPS 场景下，`parsed=30060` 说明 IO 线程已经成功收到了 60 条登录消息和 30000 条群聊消息，但 `group_chat=14078`，说明只有约一半群聊任务在压测统计前完成处理。
- 500 QPS 场景下 `redis_publish=281560` 和 `offline_store_rows=281560`，刚好等于 `14078 * 20`，进一步证明瓶颈不是消息解析，而是业务任务积压后未能全部执行完。

#### 测试结论

- 当前系统在 100 QPS 和 200 QPS、8 个发送者、100 人群聊场景下可以保证本机消息完整投递，且 P99 延迟分别为 `33.66 ms` 和 `58.33 ms`。
- 500 QPS 已超过当前系统稳定处理能力，出现大量消息未及时送达，平均延迟和 P99 延迟上升到秒级。
- 可观测性指标能够定位瓶颈位置：IO 线程收包正常，Redis publish 正常，本机发送正常，主要瓶颈集中在业务线程处理能力和离线消息同步落库。
- 下一步应优先增加业务线程池队列长度、当前积压任务数、最大积压任务数等指标，确认是否存在业务队列堆积或任务丢弃。
- 性能优化方向应优先考虑将离线消息落库从 `groupChat()` 热路径中拆出，改为异步离线落库队列，由后台线程批量写入 MySQL。
