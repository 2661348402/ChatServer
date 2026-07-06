# Model 层 — 数据实体与数据访问

## 源文件

### 实体类（Entity）

| 类 | 头文件 | 说明 |
|----|-------|------|
| `User` | [include/server/model/User.hpp](../include/server/model/User.hpp) | 用户实体 |
| `Group` | [include/server/model/Group.hpp](../include/server/model/Group.hpp) | 群组实体 |
| `GroupUser` | [include/server/model/GroupUser.hpp](../include/server/model/GroupUser.hpp) | 群组成员实体（继承 User） |

### 数据访问对象（DAO / Model）

| 类 | 头文件 | 对应数据库表 |
|----|-------|-------------|
| `UserModel` | [include/server/model/UserModel.hpp](../include/server/model/UserModel.hpp) | `user` |
| `FriendModel` | [include/server/model/FriendModel.hpp](../include/server/model/FriendModel.hpp) | `Friend` |
| `GroupModel` | [include/server/model/GroupModel.hpp](../include/server/model/GroupModel.hpp) | `AllGroup` / `GroupUser` |
| `OfflineMsgModel` | [include/server/model/OfflineMessageModel.hpp](../include/server/model/OfflineMessageModel.hpp) | `OfflineMessage` |

## 职责

- **实体类**：定义业务对象的属性和访问器，纯数据结构
- **DAO 类**：封装 SQL 操作，通过 ConnectionPool 获取数据库连接执行 CRUD，将结果集映射为实体对象

## 数据库表结构

### user
```sql
CREATE TABLE user (
    id       INT AUTO_INCREMENT PRIMARY KEY,
    name     VARCHAR(50) NOT NULL UNIQUE,
    password CHAR(64),              -- SHA-256 哈希
    state    ENUM('online','offline') DEFAULT 'offline'
);
```

### Friend
```sql
CREATE TABLE Friend (
    userid   INT NOT NULL,
    friendid INT NOT NULL,
    PRIMARY KEY (userid, friendid),
    FOREIGN KEY (userid)   REFERENCES user(id) ON DELETE CASCADE,
    FOREIGN KEY (friendid) REFERENCES user(id) ON DELETE CASCADE
);
```

### AllGroup
```sql
CREATE TABLE AllGroup (
    id        INT AUTO_INCREMENT PRIMARY KEY,
    groupname VARCHAR(50) NOT NULL,
    groupdesc VARCHAR(200) DEFAULT ''
);
```

### GroupUser
```sql
CREATE TABLE GroupUser (
    groupid   INT NOT NULL,
    userid    INT NOT NULL,
    grouprole ENUM('creator','normal') DEFAULT 'normal',
    PRIMARY KEY (groupid, userid),
    FOREIGN KEY (groupid) REFERENCES AllGroup(id) ON DELETE CASCADE,
    FOREIGN KEY (userid)  REFERENCES user(id) ON DELETE CASCADE
);
```

### OfflineMessage
```sql
CREATE TABLE OfflineMessage (
    userid  INT NOT NULL,
    message VARCHAR(500) NOT NULL   -- JSON 字符串格式存储
);
```

---

## 实体类

### User

```cpp
class User {
    int id;             // -1 表示未初始化/不存在
    std::string name;
    std::string password;
    std::string state;  // "online" / "offline"
};
```

核心方法：`getId()`, `getName()`, `getPwd()`, `getState()`, `setId()`, `setName()`, `setPwd()`, `setState()`

### Group

```cpp
class Group {
    int id;
    std::string name;
    std::string desc;
    std::vector<GroupUser> users;  // 群成员列表（含角色）
};
```

核心方法：`getId()`, `getName()`, `getDesc()`, `getUsers()`, `setUsers()`

### GroupUser (extends User)

```cpp
class GroupUser : public User {
    std::string role;  // "creator" / "normal"
};
```

核心方法：`getRole()`, `setRole()` + 继承自 User 的所有方法

---

## DAO 接口

### UserModel

| 方法 | 说明 |
|------|------|
| `bool insert(User& user)` | 插入用户，成功后回填 `user.setId()` |
| `User queryById(int id)` | 按 ID 查询，不存在返回 `id=-1` 的空对象 |
| `User queryByName(const string& name)` | 按名称查询（用于搜索） |
| `vector<User> queryByNameLike(const string& keyword)` | 模糊搜索（Phase 3 搜索功能用） |
| `bool updateState(User& user)` | 更新在线状态 |
| `bool stateReset()` | 重置所有用户为 offline（服务器重启用） |

### FriendModel

| 方法 | 说明 |
|------|------|
| `bool insert(int userId, int friendId)` | 添加好友关系（双向） |
| `vector<User> query(int id)` | 查询某用户的所有好友 |

### GroupModel

| 方法 | 说明 |
|------|------|
| `bool createGroup(Group& group)` | 创建群组，成功后回填 `group.setId()` |
| `void addGroup(int userid, int groupid, const string& role)` | 用户加入群组 |
| `vector<Group> queryGroups(int userid)` | 查询用户所属的所有群组（含成员列表） |
| `vector<int> queryGroupUsers(int userid, int groupid)` | 查询群内其他成员 ID（排除 userid 自己） |

### OfflineMsgModel

| 方法 | 说明 |
|------|------|
| `bool insert(int userid, const string& message)` | 存储一条离线消息（JSON 字符串） |
| `vector<string> query(int userid)` | 查询所有离线消息 |
| `bool remove(int userid)` | 删除某用户的所有离线消息（登录推送后清除） |

## 数据库操作模式

所有 Model 类遵循统一模式：

```cpp
// 1. 从连接池获取连接
shared_ptr<MySQL> conn = ConnectionPool::instance().getConnection();

// 2. 构建 SQL（使用参数化防注入）
string sql = "...";

// 3. 执行查询
MYSQL_RES* res = conn->query(sql);

// 4. 遍历结果集，映射为实体对象
while (MYSQL_ROW row = mysql_fetch_row(res)) { ... }

// 5. 释放结果集
mysql_free_result(res);

// 6. shared_ptr 析构自动归还连接到池
```

## 线程安全

Model 类本身**无锁**——线程安全由 ConnectionPool 内部的条件变量保证（连接获取与归还的同步）。
