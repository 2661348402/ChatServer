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