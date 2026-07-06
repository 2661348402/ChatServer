# Util — SHA-256 工具

## 源文件

- 头文件：[include/server/util/SHA256.hpp](../include/server/util/SHA256.hpp)
- 实现：`src/server/util/`

## 职责

- 提供静态 SHA-256 哈希函数，用于密码的哈希存储和验证

## 核心接口

### `static string SHA256::hash(const string& input)`

对输入字符串计算 SHA-256 哈希，返回 64 字符的十六进制小写字符串。

```cpp
std::string hashed = SHA256::hash("mypassword");
// hashed = "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92"
```

## 使用场景

| 场景 | 位置 | 说明 |
|------|------|------|
| 注册 | `ChatService::reg()` | 客户端传来的密码已经过 SHA-256 哈希，服务端直接存储 |
| 登录验证 | `ChatService::login()` | 服务端对输入密码做 SHA-256 哈希后与数据库中的哈希值比对 |
| 客户端 | `ChatClient::login()` / `registerUser()` | 客户端在发送前对密码做 SHA-256 哈希（不传明文） |

## 安全说明

- 密码在客户端先做一次 SHA-256 哈希再传输（网络层不传明文密码）
- 数据库中 `user.password` 字段类型为 `CHAR(64)`，存储哈希后的十六进制字符串
- 登录验证时：`SHA256::hash(inputPwd) == user.getPwd()`
- 这是单次 SHA-256，未加盐。生产环境建议使用 bcrypt / scrypt / Argon2 等密钥派生函数

## 依赖

- OpenSSL 的 SHA-256 实现（通过系统库链接）
