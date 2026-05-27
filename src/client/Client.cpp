#include "Client.hpp"
#include "json.hpp"

using json = nlohmann::json;
ChatClient::ChatClient()
:_sockfd(-1), _running(false)
{
    initCommandHandler();
}
void ChatClient::initCommandHandler() {
    _cmdHandler["help"] = std::bind(&ChatClient::help, this, std::placeholders::_1);
    _cmdHandler["addfriend"] = std::bind(&ChatClient::addFriend, this, std::placeholders::_1);
    _cmdHandler["chat"] = std::bind(&ChatClient::oneToOneChat, this, std::placeholders::_1);
    _cmdHandler["creategroup"] = std::bind(&ChatClient::createGroup, this, std::placeholders::_1);
    _cmdHandler["addgroup"] = std::bind(&ChatClient::addGroup, this, std::placeholders::_1);
    _cmdHandler["groupchat"] = std::bind(&ChatClient::groupChat, this, std::placeholders::_1);
    _cmdHandler["loginout"] = std::bind(&ChatClient::loginOut, this, std::placeholders::_1);
}
ChatClient::~ChatClient()
{
    closeClient();
}

bool ChatClient::connectServer(const string& ip, int port)
{
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(_sockfd < 0)
    {
        cerr << "创建套接字失败" << endl;
        return false;
    }

    sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &servAddr.sin_addr);

    if(connect(_sockfd, (sockaddr*)&servAddr, sizeof(servAddr)) < 0)
    {
        close(_sockfd);
        _sockfd = -1;
        return false;
    }
    return true;
}

void ChatClient::recvMsgThread()
{
    char buf[1024] = {0};
    while(_running)
    {
        memset(buf, 0, sizeof(buf));
        int len = recv(_sockfd, buf, sizeof(buf), 0);
        if(len <= 0)
        {
            cout << "服务器断开连接" << endl;
            _running = false;
            break;
        }
        json js = json::parse(buf);
        int msgId = js["msgId"];
        string from = js["from"];       // 谁发的
        string msg = js["message"];     // 消息内容
        // ===================== 你要的格式 =====================
        if (msgId == 5) {
            // 一对一：直接打印 【谁：消息】
            cout << "\n【" << from << "】：" << msg << endl;
        }
        else if (msgId == 9) {
            // 群聊：先打印群名/群ID，再打印谁发的
            int groupid = js["groupId"];
            cout << "\n【群聊 " << groupid << "】" << endl;
            cout << "【" << from << "】：" << msg << endl;
        }
        // ======================================================
        fflush(stdout);
    }
}

void ChatClient::sendMsg(const string& buf)
{
    send(_sockfd, buf.c_str(), buf.size(), 0);
}

bool ChatClient::login(int id, const string& pwd)
{
    json js;
    js["msgId"] = LOGIN_MSG;
    js["id"] = id;
    js["password"] = pwd;
    sendMsg(js.dump());

    char recvBuf[1024] = {0};
    int len = recv(_sockfd, recvBuf, sizeof(recvBuf), 0);
    if(len <= 0) 
        return false;

    json res = json::parse(recvBuf);

    // 登录失败
    if(res["errno"] != 0)
    {
        cout << "\n❌ 登录失败：" << res["errMessage"] << endl;
        return false;
    }

    // ====================== 登录成功：打印 3 类数据 ======================
    cout << "\n================================================" << endl;
    cout << "               ✅ 登录成功！" << endl;
    cout << "================================================" << endl;

    _running = true;
    _curUser.setId(id);
    _curUser.setName(res["name"]);
    // 1. 打印离线消息
    if (res.contains("offlineMsg")) {
        cout << "\n📩 【离线消息】：" << endl;
        for (auto& msgStr : res["offlineMsg"]) {
            // 取出通用字段
            json msg = json::parse(msgStr.get<std::string>());
            string sendtime = msg["sendtime"];
            string message = msg["message"];
            int msgId = msg["msgId"];       // 关键：用 msgId 区分类型
            if (msgId == 5) {
                // ===================== 一对一消息 =====================
                string fromName = msg["from"];
                cout << "[" << sendtime << "] 【个人消息】 " << fromName << "：" << message << endl;
            }
            else if (msgId == 9) {
                // ===================== 群聊消息 =====================
                int groupId = msg["groupid"];   // 群ID
                string fromName = msg["from"];
                cout << "[" << sendtime << "] 【群聊 " << groupId << "】 " << fromName << "：" << message << endl;
            }
        }
    } else {
        cout << "\n📩 【离线消息】：无新消息" << endl;
    }

    // 2. 打印好友列表
    if (res.contains("friends")) {
        cout << "\n👥 【好友列表】：" << endl;
        for (auto& friendJsonStr : res["friends"]) {
            // 关键：先获取字符串！
            string jsonStr = friendJsonStr.get<string>();
            json friendJson = json::parse(jsonStr);
            cout << "   ID:" << friendJson["id"] 
                 << "  名称:" << friendJson["name"] 
                 << "  状态:" << friendJson["state"] << endl;
        }
    }

    // 3. 打印群组列表
    if (res.contains("groups")) {
        cout << "\n🏠 【群组列表】：" << endl;
        for (auto& groupJsonStr : res["groups"]) {
            string jsonStr = groupJsonStr.get<string>();
            json groupJson = json::parse(jsonStr);
            cout << "   群名：" << groupJson["name"] 
                 << "  描述：" << groupJson["desc"] << endl;
        }
    }

    cout << "\n================================================" << endl;
    return true;
}
bool ChatClient::registerUser(const string& name, const string& pwd)
{
    json req;
    req["msgId"] = REG_MSG;
    req["name"] = name;
    req["password"] = pwd;
    sendMsg(req.dump());

    char buf[1024] = {0};
    int len = recv(_sockfd, buf, sizeof(buf), 0);
    if(len <= 0)
        return false;

    json res = json::parse(buf);
    if(res["errno"] == 0)
    {
        cout << "注册成功，你的账号ID：" << res["id"] << endl;
        return true;
    }
    else
    {
        cout << "注册失败，用户名已存在" << endl;
        return false;
    }
}

// === 聊天主循环（你图里的功能） ===
void ChatClient::chatMain() {
    _running = true;
    //创建接收线程
    std::thread recvThread(&ChatClient::recvMsgThread, this);
    recvThread.detach();

    std::string cmdLine;
    std::cout << "✅ 进入聊天界面，输入 help 查看命令\n" << std::endl;

    while (_running) {
        std::cout << "[命令] >> ";
        std::getline(std::cin, cmdLine);

        size_t pos = cmdLine.find(':');
        std::string cmd, param;

        if (pos == std::string::npos) {
            cmd = cmdLine;
        } else {
            cmd = cmdLine.substr(0, pos);
            param = cmdLine.substr(pos + 1);
        }

        if (_cmdHandler.find(cmd) != _cmdHandler.end()) {
            _cmdHandler[cmd](param);
        } else {
            std::cout << "❌ 未知命令，输入 help 查看\n";
        }
    }
}

// === 命令实现 ===
void ChatClient::help(const std::string&) {
    std::cout << "\n===== 支持命令 =====" << std::endl;
    std::cout << "help                  : 显示帮助" << std::endl;
    std::cout << "addfriend:好友ID      : 添加好友" << std::endl;
    std::cout << "chat:好友ID:消息      : 一对一聊天" << std::endl;
    std::cout << "creategroup:群名:描述 : 创建群组" << std::endl;
    std::cout << "addgroup:群ID         : 加入群组" << std::endl;
    std::cout << "groupchat:群ID:消息   : 群聊" << std::endl;
    std::cout << "loginout              : 退出登录" << std::endl;
    std::cout << "====================\n" << std::endl;
}

void ChatClient::addFriend(const std::string& param) {
    json js;
    js["msgId"] = ADD_FRIEND_MSG;
    js["id"] = _curUser.getId();
    js["friendId"] = std::stoi(param);
    send(_sockfd, js.dump().c_str(), js.dump().size(), 0);
    std::cout << "✅ 已向用户："<<std::stoi(param)<<"发送添加好友请求\n";
}

void ChatClient::oneToOneChat(const std::string& param) {
    size_t pos = param.find(':');
    // 安全判断
    if (pos == std::string::npos) {
        std::cout << "❌ 消息格式错误！请输入: chat:ID:消息" << std::endl;
        return;
    }
    int toid = std::stoi(param.substr(0, pos));
    std::string msg = param.substr(pos + 1);

    json js;
    js["msgId"] = ONE_CHAT_MSG;
    js["id"] = _curUser.getId();
    js["from"] = _curUser.getName();
    js["toid"] = toid;
    js["message"] = msg;
    send(_sockfd, js.dump().c_str(), js.dump().size(), 0);
    std::cout << "✅ 消息已发送\n";
}

void ChatClient::createGroup(const std::string& param) {
    size_t pos1 = param.find(':');
    size_t pos2 = param.find(':', pos1 + 1);
    std::string name = param.substr(0, pos1);
    std::string desc = param.substr(pos1 + 1);

    json js;
    js["msgId"] = CREATE_GROUP_MSG;
    js["id"] = _curUser.getId();
    js["groupname"] = name;
    js["groupdesc"] = desc;
    send(_sockfd, js.dump().c_str(), js.dump().size(), 0);
    std::cout << "✅ 群组创建请求已发送\n";
}

void ChatClient::addGroup(const std::string& param) {
    json js;
    js["msgId"] = ADD_GROUP_MSG;
    js["id"] = _curUser.getId();
    js["groupId"] = std::stoi(param);
    send(_sockfd, js.dump().c_str(), js.dump().size(), 0);
    std::cout << "✅ 加入群组请求已发送\n";
}

void ChatClient::groupChat(const std::string& param) {
    size_t pos = param.find(':');
    int groupid = std::stoi(param.substr(0, pos));
    std::string msg = param.substr(pos + 1);

    json js;
    js["msgId"] = GROUP_CHAT_MSG;
    js["id"] = _curUser.getId();
    js["from"] = _curUser.getName();
    js["groupId"] = groupid;
    js["message"] = msg;
    send(_sockfd, js.dump().c_str(), js.dump().size(), 0);
    std::cout << "✅ 群聊消息已发送\n";
}

void ChatClient::loginOut(const std::string&) {
    json js;
    js["msgId"] = LOGIN_OUT_MSG;
    js["id"] = _curUser.getId();
    send(_sockfd, js.dump().c_str(), js.dump().size(), 0);

    // 清空用户信息
    _curUser = User{};

    // 把聊天主循环关掉！回到 mainMenu
    _running = false;

    std::cout << "\n✅ 已退出登录，返回主菜单...\n";

}

void ChatClient::mainMenu()
{
    while(true)
    {
        cout << "\n===== 客户端主菜单 =====" << endl;
        cout << "1. 登录" << endl;
        cout << "2. 注册" << endl;
        cout << "3. 退出" << endl;
        cout << "请选择操作：";
        int op;
        cin >> op;
        cin.ignore();

        if(op == 1)
        {
            int id;
            string pwd;
            cout << "输入账号："; cin >> id;
            cout << "输入密码："; cin >> pwd;
            cin.ignore();
            if(login(id, pwd))
            {
                chatMain();
            }
        }
        else if(op == 2)
        {
            string name, pwd;
            cout << "输入用户名："; cin >> name;
            cout << "输入密码："; cin >> pwd;
            cin.ignore();
        }
        else if(op == 3)
        {
            cout << "退出客户端" << endl;
            break;
        }
        else
        {
            cout << "无效选项" << endl;
        }
    }
}

void ChatClient::closeClient()
{
    _running = false;
    if(_sockfd > 0)
    {
        close(_sockfd);
        _sockfd = -1;
    }
}