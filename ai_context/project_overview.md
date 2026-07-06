# Cluster Chat Server — 项目总览

## 一句话定位

基于 Muduo 网络库的分布式集群聊天服务器，支持多节点部署、跨服务器消息路由与横向扩展。配套命令行终端客户端与 Qt GUI 客户端。

## 架构概览

```
Client (Terminal / Qt GUI)
        │
        ▼
   Nginx (TCP Stream 负载均衡)
        │
   ┌────┼────┐
   ▼    ▼    ▼
ChatServer-1  ChatServer-2  ChatServer-N
   │    │    │
   └────┼────┘
        ▼
   MySQL (持久化)  +  Redis (Pub/Sub 跨服路由)
```

- **客户端层**：终端客户端（TCP Socket，命令行交互）+ Qt GUI 客户端（图形界面，深色/浅色主题切换）
- **负载均衡层**：Nginx TCP Stream 代理（轮询 / least_conn），对外暴露单一端口，支持心跳检测与故障转移
- **网络层**：Muduo Reactor 多线程模型，主线程 accept + 4 个 I/O worker 线程
- **业务层**：ChatService 单例，基于 msgId 的消息分发机制，处理登录/注册/聊天/好友/群组等业务
- **数据层**：MySQL 8.0（用户/好友/群组/离线消息持久化）+ Redis Pub/Sub（跨服务器消息实时路由）

## 核心能力

| 能力 | 实现方式 |
|------|---------|
| **高并发** | Muduo Reactor 非阻塞 I/O + 多线程模型 + MySQL 连接池 |
| **分布式** | Nginx TCP 负载均衡 + Redis Pub/Sub 跨服务器消息通道 |
| **安全性** | SHA-256 密码哈希（不存明文）、MySQL 参数化查询防注入 |
| **可靠传输** | TCP 自定义帧协议：4 字节大端序长度前缀 + JSON Body，最大 1MB |
| **离线消息** | 三级投递：本地直连 → Redis 跨服 → MySQL 离线存储，登录时统一拉取 |
| **UI 体验** | Qt 原生 GUI，亮色/暗色主题一键切换，CJK 字体适配 |

## 技术栈

| 层 | 技术 | 版本/说明 |
|----|------|----------|
| 语言 | C++ | C++11 标准 |
| 网络库 | Muduo | Reactor 模式，多线程网络库 |
| 数据库 | MySQL | 8.0，InnoDB |
| 缓存/消息队列 | Redis + hiredis | Pub/Sub 跨服务器消息路由 |
| JSON 序列化 | nlohmann/json | header-only 库，`third_party/json.hpp` |
| 负载均衡 | Nginx | TCP Stream 模块（`--with-stream`） |
| 密码哈希 | OpenSSL SHA-256 | 通过系统库链接 |
| 构建系统 | CMake | >= 3.16 |
| Qt 客户端 | Qt 5/6 (Widgets) | 图形界面客户端 |

## 目录结构

```
ChatServer/
├── CMakeLists.txt              # 顶层 CMake 构建文件
├── conf/server.conf            # 服务器配置文件（INI 格式）
├── sql/chatdb.sql              # 数据库表结构 + 示例数据
├── include/                    # 头文件目录
│   ├── public.hpp              # 公共枚举（EnMsgType 消息类型）
│   ├── server/                 # 服务端头文件
│   │   ├── ChatServer.hpp      # 网络层：TCP Server
│   │   ├── ChatService.hpp     # 业务层：消息分发与处理
│   │   ├── db/db.h             # MySQL 底层封装
│   │   ├── db/ConnectionPool.hpp  # MySQL 连接池
│   │   ├── model/User.hpp      # 用户实体
│   │   ├── model/UserModel.hpp # 用户 DAO
│   │   ├── model/Group.hpp     # 群组实体
│   │   ├── model/GroupUser.hpp # 群组成员实体
│   │   ├── model/GroupModel.hpp# 群组 DAO
│   │   ├── model/FriendModel.hpp   # 好友 DAO
│   │   ├── model/OfflineMessageModel.hpp # 离线消息 DAO
│   │   ├── redis/redis.hpp     # Redis Pub/Sub 客户端
│   │   ├── config/Config.hpp   # 配置文件解析
│   │   └── util/SHA256.hpp     # SHA-256 哈希工具
│   └── client/Client.hpp       # 终端客户端
├── src/
│   ├── CMakeLists.txt
│   ├── server/                 # 服务端源码
│   │   ├── main.cpp            # 入口：初始化配置、连接池、启动 Server
│   │   ├── ChatServer.cpp      # 网络层实现
│   │   ├── ChatService.cpp     # 业务层实现
│   │   ├── db/                 # MySQL 封装实现
│   │   ├── model/              # DAO 实现
│   │   ├── redis/              # Redis 客户端实现
│   │   ├── config/             # 配置解析实现
│   │   └── util/               # 工具类实现
│   ├── client/                 # 终端客户端源码
│   └── qt-client/              # Qt GUI 客户端源码
├── tests/                      # 测试代码
├── docs/                       # 学习笔记与变更日志
├── third_party/                # 第三方依赖（json.hpp, muduo, nginx, hiredis）
│   ├── json.hpp                # nlohmann/json
│   ├── muduo-master.zip
│   ├── nginx-1.30.2.tar.gz
│   └── hiredis-master.zip
└── bin/                        # 编译产物
    ├── ChatServer
    ├── ChatClient
    └── ChatClientQt
```

## 模块划分

| 模块 | 源码位置 | 说明 |
|------|---------|------|
| **ChatServer** | `src/server/ChatServer.cpp` | 网络层，TCP 连接管理、帧解包、消息分发 |
| **ChatService** | `src/server/ChatService.cpp` | 业务层核心，msgId 路由、用户状态管理 |
| **Model 层** | `src/server/model/` | 数据实体 (User/Group/GroupUser) + DAO |
| **MySQL 连接池** | `src/server/db/` | MySQL 连接封装 + 生产者-消费者连接池 |
| **Redis** | `src/server/redis/` | Pub/Sub 跨服务器消息路由 |
| **Config** | `src/server/config/` | INI 格式配置文件解析 |
| **Util** | `src/server/util/` | SHA-256 密码哈希工具 |
| **终端客户端** | `src/client/Client.cpp` | 命令行 TCP 客户端 |
| **Qt 客户端** | `src/qt-client/` | Qt Widgets 图形界面客户端 |


## 快速开始

### 构建

```bash
# 1. 安装依赖：MySQL 8.0, Redis, hiredis, Muduo, Nginx (with --with-stream), Boost
# 2. 构建
mkdir build && cd build
cmake ..
make -j$(nproc)
# 3. 产物在 bin/ 目录
```

### 启动

```bash
# 按需启动多个服务端实例（不同端口），前面挂 Nginx 负载均衡
./bin/ChatServer conf/server.conf

# 终端客户端
./bin/ChatClient <server_ip> <port>

# Qt 客户端
./bin/ChatClientQt -H <server_ip> -P <port>
```
