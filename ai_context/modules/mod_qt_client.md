# ChatClientQt — Qt GUI 客户端

## 源文件

- 主入口：[src/qt-client/main.cpp](../src/qt-client/main.cpp)
- 主题系统：[src/qt-client/Theme.h](../src/qt-client/Theme.h)
- 网络层：`src/qt-client/network/ProtocolClient.h`
- UI 层：`src/qt-client/ui/LoginDialog.h`, `src/qt-client/ui/MainWindow.h`

## 职责

- 提供 Qt Widgets 图形界面的聊天客户端
- 封装 TCP 通信（`ProtocolClient`），与服务端帧协议兼容
- 提供登录对话框和主聊天窗口
- 支持亮色/暗色主题一键切换
- 自动适配 CJK 字体

## 架构

```
Qt Application
  ├── ProtocolClient       # 网络通信层（TCP Socket + 帧协议）
  ├── LoginDialog          # 登录对话框
  ├── MainWindow           # 主窗口（聊天）
  └── Theme                # 主题/颜色系统
```

## 核心组件

### ProtocolClient

`src/qt-client/network/ProtocolClient.h`

- 封装 TCP Socket 连接和帧协议通信
- 与服务端/终端客户端使用相同的帧格式（4 字节长度头 + JSON Body）

### LoginDialog

`src/qt-client/ui/LoginDialog.h`

- 登录/注册的用户界面
- 调用 `ProtocolClient` 完成认证

### MainWindow

`src/qt-client/ui/MainWindow.h`

- 聊天主界面
- 显示好友列表、群组列表、聊天消息
- 发送一对一消息和群聊消息
- 显示离线消息

### Theme

[src/qt-client/Theme.h](../src/qt-client/Theme.h)

静态方法类，集中管理全部 UI 颜色：

| 类别 | 方法示例 |
|------|---------|
| 品牌色 | `green()`, `greenHover()`, `greenPressed()` |
| 背景色 | `bgDialog()`, `bgChat()`, `bgInput()`, `bgCard()` |
| 边框色 | `borderCard()`, `borderInput()`, `borderChat()` |
| 文字色 | `textPrimary()`, `textSecondary()`, `textMuted()`, `textError()` |
| 气泡色 | `bubbleSelf()`, `bubbleOther()` |
| 控件色 | `scrollHandle()`, `hoverBg()`, `selectedBg()` |

**模式切换**：
```cpp
Theme::setDarkMode(true);   // 暗色模式
Theme::setDarkMode(false);  // 亮色模式
Theme::toggle();            // 切换模式
Theme::isDark();            // 查询当前模式
```

每个颜色访问器内部根据 `Theme::isDark()` 返回对应的亮色或暗色值。

## 字体适配

`main()` 中按优先级探测可用 CJK 字体：

```
WenQuanYi Micro Hei → Noto Sans CJK SC → Noto Sans SC
→ Source Han Sans SC → Microsoft YaHei → SimHei → PingFang SC
→ 系统默认
```

## 程序化图标

在运行时用 `QPainter` 绘制绿色聊天气泡 + "C" 字母作为应用图标。

## 命令行参数

```bash
./bin/ChatClientQt -H <server_ip> -P <port>
```

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `-H` / `--host` | `127.0.0.1` | 服务器地址 |
| `-P` / `--port` | `12345` | 服务器端口 |

## 生命周期

1. 解析命令行参数
2. 创建 `ProtocolClient`，连接服务器
3. 显示 `LoginDialog`
4. 用户取消 → 退出
5. 登录成功 → 显示 `MainWindow`，加载离线消息
6. 进入 Qt 事件循环 (`app.exec()`)
7. 窗口关闭 → `client.sendLogout()` 发送登出 → 退出

## 依赖

- Qt 5 或 Qt 6 (Widgets 模块)
- `ProtocolClient`（内部使用 Qt 网络模块或 POSIX socket）
- 服务端帧协议兼容
