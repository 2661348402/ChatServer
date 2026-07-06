# Config — 配置系统

## 源文件

- 头文件：[include/server/config/Config.hpp](../include/server/config/Config.hpp)
- 实现：`src/server/config/`

## 职责

- 解析 INI 格式的配置文件 (`conf/server.conf`)
- 提供键值对的读取接口（字符串和整数）
- 支持默认值，配置项缺失时不报错

## 设计模式

**单例模式**：全局唯一配置实例，各模块均可访问。

```cpp
static Config& instance();
```

## 配置文件格式

文件路径：`conf/server.conf`（可通过命令行参数覆盖）

```ini
server.ip=127.0.0.1
server.port=12345
server.threads=4
db.host=127.0.0.1
db.port=3306
db.user=root
db.password=12345
db.name=chat
db.poolsize=8
redis.host=127.0.0.1
redis.port=6379
```

- `key=value` 格式，每行一个配置项
- 无 section 分组，扁平键值结构
- 键名使用点号命名空间（`db.host`、`redis.port` 等）

## 核心接口

| 方法 | 说明 |
|------|------|
| `static Config& instance()` | 获取单例 |
| `void load(const string& filepath)` | 加载并解析配置文件 |
| `string get(const string& key, const string& defaultValue="")` | 获取字符串配置值 |
| `int getInt(const string& key, int defaultValue=0)` | 获取整数配置值 |

### 使用示例

```cpp
// main() 中
Config::instance().load(cfgPath);
string ip   = Config::instance().get("server.ip", "127.0.0.1");
int port    = Config::instance().getInt("server.port", 12345);
int poolSize = Config::instance().getInt("db.poolsize", 8);
```

## 内部实现

- 使用 `std::unordered_map<string, string>` 存储键值对
- `load()` 逐行读取文件，按 `=` 分割键和值，去除首尾空白
- 忽略空行和注释行（`#` 开头）
- `getInt()` 内部调用 `std::stoi()` 转换

## 生命周期

1. `main()` 启动时最早加载（在所有组件初始化之前）
2. 之后所有模块通过 `Config::instance().get()` 按需读取
3. 整个进程生命周期内有效
