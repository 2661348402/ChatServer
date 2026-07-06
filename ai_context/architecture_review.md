# 架构评审与下一阶段优化方案

> 评审日期：2026-06-18
> 当前版本：v1.1.0 → 目标 v1.2.0（Phase 3 启动）

---

## 1. 当前系统瓶颈分析

以下分析基于 [progress.md](progress.md) 的当前进度和源码审计，按严重程度排序：

### 🔴 严重瓶颈

#### B1. `groupChat()` 在持锁期间执行 DB 查询（锁粒度错误）

**位置：** [ChatService.cpp:329-351](../src/server/ChatService.cpp)

```cpp
void ChatService::groupChat(...) {
    std::vector<int> userVec = _groupModel.queryGroupUsers(userId, groupId);
    {
        std::lock_guard<std::mutex> lock(connMutex);  // 持有锁
        for (int& user_id : userVec) {
            auto iter = _userConnMap.find(user_id);
            if (iter != _userConnMap.end()) { ... continue; }
            User user = _userModel.queryById(user_id);    // DB 查询在锁内！
            if (user.getState() == "online") {
                _redis.publish(user_id, js.dump());        // 同步网络 I/O 在锁内！
            } else {
                _offlineMsgModel.insert(user_id, js.dump()); // DB 写入在锁内！
            }
        }
    }
}
```

**影响：**
- `connMutex` 保护的是 `_userConnMap`（本地连接表），但锁内却执行了 DB 查询、Redis 发布、离线消息写入
- 群成员数量 ×（DB 查询 + Redis I/O）全部在持锁期间完成
- 所有其他需要 `connMutex` 的操作（登录、登出、一对一聊天、异常断开处理）全部被阻塞
- 这是一个**线性阻塞放大器**：群越大，锁持有时间越长

#### B2. `login()` 串行 DB 往返过多

**位置：** [ChatService.cpp:25-111](../src/server/ChatService.cpp)

登录一条路径执行了多达 6 次独立的 DB 操作（每次从连接池获取/归还）：
1. `_userModel.queryById(id)` — 查用户
2. `_userModel.updateState(user)` — 更新在线状态
3. `_offlineMsgModel.query(id)` — 拉离线消息
4. `_offlineMsgModel.remove(id)` — 删离线消息
5. `_groupModel.queryGroups(id)` — 查群组（内部还有 N+1 查询）
6. `_friendModel.query(id)` — 查好友

**影响：** 高并发登录场景下，连接池争用严重，单个登录延迟 = 6 次 DB 往返延迟之和。

#### B3. `GroupModel::queryGroups()` N+1 查询问题

**位置：** [GroupModel.cpp:30-72](../src/server/model/GroupModel.cpp)

```cpp
// 第1次查询：查用户所属群组
select ... from AllGroup inner join GroupUser ... where userid = ?
// 对每个群组：再查成员（N 次额外查询）
for (Group& group : vec) {
    select ... from user inner join GroupUser ... where groupid = ?
}
```

**影响：** 用户加入 10 个群 → 11 次查询。登录时调用此方法，严重拖慢登录响应。

#### B4. `groupChat()` 对每个非本地成员逐条 DB 查询

**位置：** [ChatService.cpp:343](../src/server/ChatService.cpp)

```cpp
User user = _userModel.queryById(user_id);  // 每个非本地成员一次查询
```

**影响：** 群发消息到 100 人群，其中 80 人不在本机 → 80 次独立 DB 查询。

### 🟡 中等瓶颈

#### B5. Redis `publish()` 使用同步阻塞调用

**位置：** [redis.cpp:53-63](../src/server/redis/redis.cpp)

`redisCommand()` 是同步阻塞的——如果 Redis 服务响应慢或网络抖动，worker 线程被阻塞，该线程上的所有其他连接都受影响。

#### B6. 无心跳保活机制

**影响：**
- TCP 半开连接无法及时发现（依赖 TCP keepalive 默认 2 小时）
- 用户异常断线（如网络断开、客户端崩溃）后，`_userConnMap` 和 MySQL 中状态仍为 "online"
- 导致：该用户无法重新登录（"already online" 错误）、消息路由错误（以为在线实际不在）

#### B7. `config` 中的 `server.threads` 未被使用

**位置：** [ChatServer.cpp:20](../src/server/ChatServer.cpp)

```cpp
_server.setThreadNum(4);  // 硬编码为 4
```

配置文件定义了 `server.threads=4`，但 `main.cpp` 并未读取该配置并传递给 ChatServer。

#### B8. 密码安全：单次 SHA-256 无盐

已记录于 progress.md 但需提升优先级——这是生产环境的安全基线问题。

### 🟢 架构层面观察

#### B9. 单 Redis 实例是单点故障

Redis 同时承担 Pub/Sub 消息通道角色。如果 Redis 宕机：
- 跨服务器消息路由完全中断
- `subscribe()` / `unsubscribe()` 在登录/登出时失败但未重试
- 当前 `unsubscribe()` 失败不阻塞登出流程（静默失败），但 `subscribe()` 失败同样被忽略——用户登录成功但跨服消息收不到

#### B10. 缺乏过载保护

- 无连接速率限制
- 无消息速率限制
- 无发送缓冲区高水位回调
- 恶意或异常客户端可压垮服务端

---

## 2. 下一阶段优化方案（Phase 3 启动）

基于当前进度的**增量优化**，分 3 个子阶段交付。

### Phase 3a：紧急修复（锁粒度 + 心跳）— 预计 2-3 天

#### Fix-1: 修复 `groupChat()` 锁粒度

**修改文件：** [ChatService.cpp](../src/server/ChatService.cpp) `groupChat()` 方法

**方案：** 将锁范围缩小到仅保护 `_userConnMap` 查找，DB 查询和 Redis 操作移出锁外。

```cpp
void ChatService::groupChat(...) {
    std::vector<int> userVec = _groupModel.queryGroupUsers(userId, groupId);

    // Phase 1: 持锁期间仅收集本地连接
    std::vector<std::pair<int, std::string>> crossNodeUsers; // {user_id, msg}
    {
        std::lock_guard<std::mutex> lock(connMutex);
        for (int& user_id : userVec) {
            auto iter = _userConnMap.find(user_id);
            if (iter != _userConnMap.end()) {
                sendFramed(iter->second, js.dump());
            } else {
                crossNodeUsers.push_back({user_id, js.dump()});
            }
        }
    } // 锁在此释放

    // Phase 2: 无锁——批量查询非本地用户状态 + 路由
    for (auto& [user_id, msg] : crossNodeUsers) {
        User user = _userModel.queryById(user_id);
        if (user.getState() == "online") {
            _redis.publish(user_id, msg);
        } else {
            _offlineMsgModel.insert(user_id, msg);
        }
    }
}
```

**注意：** 锁外操作期间，用户状态可能变化（如刚下线）。这部分有天然的 race condition，但在聊天场景中可接受——最坏情况消息进离线表，下次登录拉取。

#### Fix-2: 心跳保活机制（PING/PONG）

**枚举已定义：** `PING_MSG(14)` / `PONG_MSG(15)`

**实现内容：**

| 层 | 工作 |
|----|------|
| 服务端 `ChatServer` | `onConnection` 建立时记录连接时间；每次收到消息刷新时间戳 |
| 服务端 `ChatServer` | 定时器（Muduo `EventLoop::runEvery`）扫描所有连接，超过 N 秒无数据则主动关闭 |
| 服务端 `ChatService` | 注册 `PING_MSG` handler：收到 Ping 回复 Pong |
| 客户端 `Client.cpp` | 心跳线程：每 N 秒发送 Ping；超时未收到 Pong 则重连 |
| Qt 客户端 | QTimer 驱动心跳，逻辑同上 |

**配置项：**
```ini
heartbeat.interval=30    # 心跳间隔（秒）
heartbeat.timeout=90     # 超时判定断线（秒）
```

**设计要点：**
- 服务端不主动发 Ping（由客户端发起），减少服务端开销
- 超时断线时执行与 `clientConnectException()` 相同的清理逻辑
- 定时器在 Muduo EventLoop 中注册，由主 loop 驱动，线程安全

#### Fix-3: 读取 `server.threads` 配置

**修改：**
- [ChatServer.hpp](../include/server/ChatServer.hpp)：构造函数增加 `int threadNum` 参数
- [ChatServer.cpp](../src/server/ChatServer.cpp)：使用参数而非硬编码
- [main.cpp](../src/server/main.cpp)：读取配置并传入

### Phase 3b：性能优化 — 预计 3-4 天

#### Opt-1: 批量查询优化（减少 DB 往返）

**`GroupModel::queryGroups()` — 一次 JOIN 替代 N+1；**

改为一趟查询：
```sql
SELECT a.id, a.groupname, a.groupdesc, c.id AS member_id, c.name, c.state, b.grouprole
FROM AllGroup a
INNER JOIN GroupUser b ON a.id = b.groupid
INNER JOIN User c ON b.userid = c.id
WHERE a.id IN (SELECT groupid FROM GroupUser WHERE userid = ?)
ORDER BY a.id;
```
在应用层按 `groupid` 分组组装对象。

**`groupChat()` — 批量查询非本地用户状态；**

收集所有非本地用户 ID，一次 `WHERE id IN (...)` 查询所有用户状态。

#### Opt-2: Redis Pipeline 化 publish

当群发消息时，连续 publish 给多个非本地用户。当前每次 `publish()` 都是一次同步 RTT。改为使用 hiredis 的 `redisAppendCommand()` 流水线批量发送。

或者更轻量的方案：使用 `redisCommandArgv` 一次发送多条 PUBLISH（Redis 协议本身支持 pipeline）。

#### Opt-3: 登录信息批量加载

将 login 中的 6 次 DB 操作合并为 2-3 次批量查询：
1. 用户查询 + 状态更新（原子操作：`UPDATE ... SET state='online' WHERE id=? AND state!='online'`，通过 affected_rows 判断是否已在线）
2. 离线消息 + 好友 + 群组（3 个独立查询并行或合并 JOIN）

### Phase 3c：好友申请流程 + 用户搜索 — 预计 3-4 天

**枚举已定义，待实现：**

| 消息 | 功能 | 说明 |
|------|------|------|
| `USER_SEARCH_MSG/ACK` | 用户搜索 | 按名称模糊搜索，分页返回 |
| `FRIEND_REQUEST_MSG` | 发送好友申请 | 写入好友申请表 |
| `FRIEND_ACCEPT_MSG` | 接受好友申请 | 双向写入 Friend 表 |
| `FRIEND_REJECT_MSG` | 拒绝好友申请 | 删除申请记录 |
| `FRIEND_NOTIFY_MSG` | 好友申请通知 | 服务端推送给被申请人 |
| `MSG_ACK` | 消息已读确认 | 发送方收到回执 |

需要新建数据表 `FriendRequest`：
```sql
CREATE TABLE FriendRequest (
    id INT AUTO_INCREMENT PRIMARY KEY,
    fromUserId INT NOT NULL,
    toUserId INT NOT NULL,
    status ENUM('pending', 'accepted', 'rejected') DEFAULT 'pending',
    createTime DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (fromUserId) REFERENCES User(id),
    FOREIGN KEY (toUserId) REFERENCES User(id),
    UNIQUE KEY uk_from_to (fromUserId, toUserId)
);
```

---

## 3. 影响评估

### 性能提升点

| 优化项 | 预估提升 | 测量指标 |
|--------|---------|---------|
| Fix-1 锁粒度修复 | **高** | 群聊 P99 延迟降低 50-80%；其他操作阻塞时间大幅减少 |
| Fix-2 心跳机制 | **中** | 减少 "already online" 错误；断线检测从 2h → 90s |
| Opt-1 批量查询 | **高** | 登录延迟降低 40-60%（减少 6→3 次 DB 往返） |
| Opt-1 群发批量查 | **中** | 群聊跨服路由延迟从 O(N) DB 查询 → 1 次 |
| Opt-2 Redis Pipeline | **中** | 群聊跨服 publish 延迟降低 30-50% |
| Fix-3 线程数可配置 | **低** | 运维灵活性提升，可按机器规格调优 |

### 风险与副作用

| 优化项 | 风险 | 缓解措施 |
|--------|------|---------|
| Fix-1 缩小锁范围 | 锁外操作时用户状态可能变化（TOCTOU） | 聊天场景容忍偶尔的消息进离线表；登录 handler 已处理 "already online" |
| Fix-2 心跳 | 增加网络流量和 CPU 开销 | 心跳间隔 30s，开销极小；Ping 消息体极小（`{"msgId":14}`） |
| Opt-1 批量 JOIN | 复杂 JOIN 可能增加 MySQL 负担 | 加索引（`GroupUser.userid`, `GroupUser.groupid`）；对 1000 人以下群组影响可忽略 |
| Phase 3c 好友申请 | 新增 FriendRequest 表 | 加唯一索引防重复申请 |
| Redis Pipeline | hiredis 异步 API 复杂度 | Phase 3b 中暂用同步 pipeline（`redisAppendCommand`），不引入异步 |

---

## 4. 进度更新（基于 progress.md）

以下是更新后的完整进度列表，建议直接覆盖 [progress.md](progress.md)：

```markdown
# 项目进度

> 最后更新：2026-06-18

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | — | 初始版本：单机聊天（注册/登录/聊天/好友/群组/离线消息） |
| v1.1.0 | — | TCP 帧协议 + SHA-256 + SQL 防注入 + 连接池 + 配置系统 |
| v1.2.0 | 目标 | Phase 3a 紧急修复：锁粒度修复 + 心跳保活 + 配置可用化 |

## 已完成 ✅

### 核心功能

| 功能 | 状态 | 说明 |
|------|------|------|
| 用户注册 | ✅ | REG_MSG(2)，SHA-256 哈希存储 |
| 用户登录 | ✅ | LOGIN_MSG(1)，密码验证 + 在线状态更新 |
| 一对一聊天 | ✅ | ONE_CHAT_MSG(5)，三级投递（本地→Redis→离线） |
| 群聊 | ✅ | GROUP_CHAT_MSG(9)，批量投递群成员 |
| 添加好友 | ✅ | ADD_FRIEND_MSG(6) |
| 创建群组 | ✅ | CREATE_GROUP_MSG(7)，创建者自动加入 |
| 加入群组 | ✅ | ADD_GROUP_MSG(8) |
| 登出 | ✅ | LOGIN_OUT_MSG(10) |
| 离线消息 | ✅ | MySQL 存储 + 登录拉取推送 + 清除 |
| 跨服务器消息 | ✅ | Redis Pub/Sub，双连接模型 |
| 负载均衡 | ✅ | Nginx TCP Stream，轮询/least_conn |

### 基础设施

| 组件 | 状态 | 说明 |
|------|------|------|
| TCP 帧协议 | ✅ | 4 字节大端长度头 + JSON Body，最大 1MB |
| MySQL 连接 | ✅ | 底层封装 (db.h) |
| 连接池 | ✅ | ConnectionPool，生产者-消费者 + 条件变量，RAII 回收 |
| SHA-256 哈希 | ✅ | OpenSSL，客户端+服务端双重哈希 |
| SQL 防注入 | ✅ | `mysql_real_escape_string` 参数化 |
| 配置系统 | ✅ | INI 格式，命令行可指定路径 |
| Redis 集成 | ✅ | hiredis，双连接 publish/subscribe |
| Muduo 网络 | ✅ | Reactor 多线程，4 worker |

### 客户端

| 客户端 | 状态 | 说明 |
|--------|------|------|
| 终端客户端 | ✅ | POSIX Socket + 帧协议，命令行交互 |
| Qt GUI 客户端 | ✅ | Qt Widgets，亮/暗主题，CJK 字体适配 |

---

## 进行中 🔄

| 任务 | 进度 | 说明 |
|------|------|------|
| Phase 3a: 锁粒度修复 | 🔴 待开始 | `groupChat()` connMutex 持锁期间执行 DB/Redis I/O |
| Phase 3a: 心跳保活 | 🔴 待开始 | PING_MSG(14)/PONG_MSG(15)，30s 间隔 + 90s 超时 |
| Phase 3a: 线程数可配置 | 🔴 待开始 | 读取 conf 中的 server.threads，去掉硬编码 4 |

---

## 计划中 📋 (Phase 3b — 性能优化)

| 功能 | 枚举已定义 | 说明 |
|------|-----------|------|
| 批量查询优化 | — | GroupModel N+1 → 1 次 JOIN；groupChat 批量查用户状态 |
| Redis Pipeline | — | 群发跨服消息 pipeline 化 publish |
| 登录批量加载 | — | 6 次 DB 往返 → 2-3 次批量查询 |

---

## 计划中 📋 (Phase 3c — 好友申请 + 杂项)

| 功能 | 枚举已定义 | 说明 |
|------|-----------|------|
| 心跳保活 | `PING_MSG(14)` / `PONG_MSG(15)` | 客户端-服务端连接探活 |
| 消息确认 | `MSG_ACK(16)` | 消息已读回执 |
| 用户搜索 | `USER_SEARCH_MSG(17)` / `USER_SEARCH_ACK(18)` | 按名称搜索用户 |
| 好友申请 | `FRIEND_REQUEST_MSG(19)` | 发送好友申请 |
| 接受好友 | `FRIEND_ACCEPT_MSG(20)` | 接受好友申请 |
| 拒绝好友 | `FRIEND_REJECT_MSG(21)` | 拒绝好友申请 |
| 好友通知 | `FRIEND_NOTIFY_MSG(22)` | 服务端推送好友申请通知 |

---

## 待优化 📝

| 项目 | 优先级 | 说明 |
|------|--------|------|
| 目录结构重构 | 高 | 大重构方案已出，待实施 |
| 密码加盐 | **高** ↑ | 当前单次 SHA-256 无盐，生产环境不可接受，建议 bcrypt/Argon2 |
| 单元测试 | 中 | tests/ 缺乏 CMake 集成 |
| 连接速率限制 | 中 | 无防 flooding 机制 |
| Redis 重连机制 | 中 | Redis 断连后无自动重连和重订阅 |
| 日志系统 | 低 | 当前使用 Muduo 内置 LOG，可考虑结构化日志 |
| 消息持久化 | 低 | 聊天记录目前不持久化（只存离线消息） |
| 过载保护 | 低 | 无发送缓冲区高水位回调，无消息速率限制 |
```

---

## 5. 下一步建议

### 立即启动：Phase 3a（本次迭代）

**优先级排序（建议按此顺序实施）：**

1. **Fix-1: `groupChat()` 锁粒度修复** — 最严重的并发瓶颈，修复成本低（改动 ~15 行），收益最高
2. **Fix-2: 心跳保活 + 超时断开** — 解决 "already online" 问题和连接泄漏，是分布式稳定性的基础
3. **Fix-3: `server.threads` 配置化** — 一行改动，配合 Nginx `least_conn` 在多规格机器上灵活部署

### 下一迭代：Phase 3b

4. **`GroupModel::queryGroups()` JOIN 优化** — 解决 N+1 查询，大幅降低登录延迟
5. **`groupChat()` 批量用户状态查询** — 将 O(N) 次 DB 查询合并为 1 次
6. **登录流程批量化** — 合并多次独立 DB 操作为批量查询

### 后续迭代：Phase 3c

7. 好友申请完整流程（含 FriendRequest 表）
8. 用户搜索
9. `MSG_ACK` 消息已读确认

### 技术债务跟进

- **密码加盐**应从"中"优先级提升到"高"——这是安全基线
- **Redis 重连**需要加入——当前 `observer_channel_message()` 线程在 Redis 断连后退出，无法自动恢复
- **单元测试集成**应尽早建立——Phase 3a 的锁粒度修改如果没有测试覆盖，回归风险较高

---

## 附录 A：锁竞争热点图

```
connMutex 竞争来源（当前）：
┌────────────────────────────────────────────────────────┐
│ 操作              │ 持锁时间          │ 锁内操作        │
├───────────────────┼───────────────────┼─────────────────┤
│ login()           │ 短 (~1μs)        │ map insert      │
│ oneChat()         │ 短 (~1μs)        │ map find        │
│ loginout()        │ 短 (~1μs)        │ map find+erase  │
│ clientConnectExc. │ 中 (~10μs)       │ map 遍历+erase  │
│ groupChat()       │ 长 (N×DB_RTT!)   │ map find + DB   │ ← 问题所在
└────────────────────────────────────────────────────────┘
```

修复后：
```
│ groupChat()       │ 短 (~N×1μs)      │ map find only   │ ← 锁内无 I/O
```

## 附录 B：登录 DB 往返优化路径

```
当前 (6 次 DB 往返):
  DB: queryById → updateState → query(offline) → remove(offline)
      → queryGroups (1+N次) → query(friends)
  总延迟 ≈ 6 × DB_RTT + N × DB_RTT

Phase 3b 优化后 (2~3 次 DB 往返):
  DB: UPSERT state + queryById (合并为 1 次存储过程或事务)
  DB: query(offline + friends + groups with members) (大 JOIN)
  总延迟 ≈ 2 × DB_RTT
```
