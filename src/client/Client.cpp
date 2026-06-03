#include "Client.hpp"
#include "json.hpp"

ChatClient::ChatClient()
    : _sockfd(-1), _running(false)
{
    initCommandHandler();
}

void ChatClient::initCommandHandler() {
    using namespace std::placeholders;
    _cmdHandler["help"] = std::bind(&ChatClient::help, this, _1);
    _cmdHandler["addfriend"] = std::bind(&ChatClient::addFriend, this, _1);
    _cmdHandler["chat"] = std::bind(&ChatClient::oneToOneChat, this, _1);
    _cmdHandler["creategroup"] = std::bind(&ChatClient::createGroup, this, _1);
    _cmdHandler["addgroup"] = std::bind(&ChatClient::addGroup, this, _1);
    _cmdHandler["groupchat"] = std::bind(&ChatClient::groupChat, this, _1);
    _cmdHandler["loginout"] = std::bind(&ChatClient::loginOut, this, _1);
}

ChatClient::~ChatClient() {
    closeClient();
}

bool ChatClient::connectServer(const std::string& ip, int port) {
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        std::cerr << "socket create failed" << std::endl;
        return false;
    }

    sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &servAddr.sin_addr);

    if (connect(_sockfd, (sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        close(_sockfd);
        _sockfd = -1;
        return false;
    }
    return true;
}

static bool recvAll(int fd, char* buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, buf + received, len - received, 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

static bool recvFramedMessage(int fd, std::string& out) {
    uint32_t netLen = 0;
    if (!recvAll(fd, (char*)&netLen, 4)) return false;
    uint32_t msgLen = ntohl(netLen);
    if (msgLen > 1024 * 1024) return false; // safety: max 1MB
    out.resize(msgLen);
    if (!recvAll(fd, &out[0], msgLen)) return false;
    return true;
}

static void sendFramedMessage(int fd, const std::string& buf) {
    uint32_t netLen = htonl(buf.size());
    std::string packet(reinterpret_cast<const char*>(&netLen), 4);
    packet.append(buf);

    const char* data = packet.c_str();
    size_t remaining = packet.size();
    while (remaining > 0) {
        ssize_t n = send(fd, data, remaining, 0);
        if (n < 0) return;
        data += n;
        remaining -= n;
    }
}

void ChatClient::recvMsgThread() {
    while (_running) {
        std::string msg;
        if (!recvFramedMessage(_sockfd, msg)) {
            std::cout << "server disconnected" << std::endl;
            _running = false;
            break;
        }

        auto js = nlohmann::json::parse(msg);
        int msgId = js["msgId"];
        std::string from = js.value("from", "");
        std::string message = js.value("message", "");

        if (msgId == ONE_CHAT_MSG) {
            std::cout << "\n[" << from << "]: " << message << std::endl;
        } else if (msgId == GROUP_CHAT_MSG) {
            int groupid = js["groupId"];
            std::cout << "\n[group " << groupid << "]" << std::endl;
            std::cout << "[" << from << "]: " << message << std::endl;
        }
        fflush(stdout);
    }
}

void ChatClient::sendMsg(const std::string& buf) {
    sendFramedMessage(_sockfd, buf);
}

bool ChatClient::login(int id, const std::string& pwd) {
    nlohmann::json js;
    js["msgId"] = LOGIN_MSG;
    js["id"] = id;
    js["password"] = pwd;
    sendMsg(js.dump());

    std::string msg;
    if (!recvFramedMessage(_sockfd, msg)) return false;

    auto res = nlohmann::json::parse(msg);

    if (res["errno"] != 0) {
        std::cout << "\nlogin failed: " << res["errMessage"] << std::endl;
        return false;
    }

    std::cout << "\n===== Login Success! =====" << std::endl;

    _running = true;
    _curUser.setId(id);
    _curUser.setName(res["name"]);

    if (res.contains("offlineMsg")) {
        std::cout << "\n[Offline Messages]:" << std::endl;
        for (auto& msgStr : res["offlineMsg"]) {
            auto msg = nlohmann::json::parse(msgStr.get<std::string>());
            std::string sendtime = msg["sendtime"];
            std::string message = msg["message"];
            int mid = msg["msgId"];
            if (mid == ONE_CHAT_MSG) {
                std::string fromName = msg["from"];
                std::cout << "[" << sendtime << "] " << fromName << ": " << message << std::endl;
            } else if (mid == GROUP_CHAT_MSG) {
                int groupId = msg["groupid"];
                std::string fromName = msg["from"];
                std::cout << "[" << sendtime << "] [group " << groupId << "] "
                          << fromName << ": " << message << std::endl;
            }
        }
    } else {
        std::cout << "\n[Offline Messages]: none" << std::endl;
    }

    if (res.contains("friends")) {
        std::cout << "\n[Friends]:" << std::endl;
        for (auto& friendJsonStr : res["friends"]) {
            std::string jsonStr = friendJsonStr.get<std::string>();
            auto friendJson = nlohmann::json::parse(jsonStr);
            std::cout << "  ID:" << friendJson["id"]
                      << "  Name:" << friendJson["name"]
                      << "  State:" << friendJson["state"] << std::endl;
        }
    }

    if (res.contains("groups")) {
        std::cout << "\n[Groups]:" << std::endl;
        for (auto& groupJsonStr : res["groups"]) {
            std::string jsonStr = groupJsonStr.get<std::string>();
            auto groupJson = nlohmann::json::parse(jsonStr);
            std::cout << "  " << groupJson["name"]
                      << " - " << groupJson["desc"] << std::endl;
        }
    }

    std::cout << "==============================" << std::endl;
    return true;
}

bool ChatClient::registerUser(const std::string& name, const std::string& pwd) {
    nlohmann::json req;
    req["msgId"] = REG_MSG;
    req["name"] = name;
    req["password"] = pwd;
    sendMsg(req.dump());

    std::string msg;
    if (!recvFramedMessage(_sockfd, msg)) return false;

    auto res = nlohmann::json::parse(msg);
    if (res["errno"] == 0) {
        std::cout << "register success, your ID: " << res["id"] << std::endl;
        return true;
    } else {
        std::cout << "register failed" << std::endl;
        return false;
    }
}

void ChatClient::chatMain() {
    _running = true;
    std::thread recvThread(&ChatClient::recvMsgThread, this);
    recvThread.detach();

    std::string cmdLine;
    std::cout << "Enter chat, type help for commands" << std::endl;

    while (_running) {
        std::cout << "[cmd] >> ";
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
            std::cout << "unknown command, type help" << std::endl;
        }
    }
}

void ChatClient::help(const std::string&) {
    std::cout << "\n===== Commands =====" << std::endl;
    std::cout << "help                  : show help" << std::endl;
    std::cout << "addfriend:friendID    : add friend" << std::endl;
    std::cout << "chat:friendID:message : private chat" << std::endl;
    std::cout << "creategroup:name:desc : create group" << std::endl;
    std::cout << "addgroup:groupID      : join group" << std::endl;
    std::cout << "groupchat:groupID:msg : group chat" << std::endl;
    std::cout << "loginout              : logout" << std::endl;
    std::cout << "====================\n" << std::endl;
}

void ChatClient::addFriend(const std::string& param) {
    nlohmann::json js;
    js["msgId"] = ADD_FRIEND_MSG;
    js["id"] = _curUser.getId();
    js["friendId"] = std::stoi(param);
    sendMsg(js.dump());
    std::cout << "friend request sent to user: " << param << std::endl;
}

void ChatClient::oneToOneChat(const std::string& param) {
    size_t pos = param.find(':');
    if (pos == std::string::npos) {
        std::cout << "format: chat:ID:message" << std::endl;
        return;
    }
    int toid = std::stoi(param.substr(0, pos));
    std::string msgStr = param.substr(pos + 1);

    nlohmann::json js;
    js["msgId"] = ONE_CHAT_MSG;
    js["id"] = _curUser.getId();
    js["from"] = _curUser.getName();
    js["toid"] = toid;
    js["message"] = msgStr;
    sendMsg(js.dump());
    std::cout << "message sent" << std::endl;
}

void ChatClient::createGroup(const std::string& param) {
    size_t pos1 = param.find(':');
    std::string name = param.substr(0, pos1);
    std::string desc = param.substr(pos1 + 1);

    nlohmann::json js;
    js["msgId"] = CREATE_GROUP_MSG;
    js["id"] = _curUser.getId();
    js["groupname"] = name;
    js["groupdesc"] = desc;
    sendMsg(js.dump());
    std::cout << "group create request sent" << std::endl;
}

void ChatClient::addGroup(const std::string& param) {
    nlohmann::json js;
    js["msgId"] = ADD_GROUP_MSG;
    js["id"] = _curUser.getId();
    js["groupId"] = std::stoi(param);
    sendMsg(js.dump());
    std::cout << "join group request sent" << std::endl;
}

void ChatClient::groupChat(const std::string& param) {
    size_t pos = param.find(':');
    int groupid = std::stoi(param.substr(0, pos));
    std::string msgStr = param.substr(pos + 1);

    nlohmann::json js;
    js["msgId"] = GROUP_CHAT_MSG;
    js["id"] = _curUser.getId();
    js["from"] = _curUser.getName();
    js["groupId"] = groupid;
    js["message"] = msgStr;
    sendMsg(js.dump());
    std::cout << "group message sent" << std::endl;
}

void ChatClient::loginOut(const std::string&) {
    nlohmann::json js;
    js["msgId"] = LOGIN_OUT_MSG;
    js["id"] = _curUser.getId();
    sendMsg(js.dump());

    _curUser = User{};
    _running = false;
    std::cout << "\nlogged out, returning to menu..." << std::endl;
}

void ChatClient::mainMenu() {
    while (true) {
        std::cout << "\n===== Main Menu =====" << std::endl;
        std::cout << "1. Login" << std::endl;
        std::cout << "2. Register" << std::endl;
        std::cout << "3. Exit" << std::endl;
        std::cout << "Select: ";
        int op;
        std::cin >> op;
        std::cin.ignore();

        if (op == 1) {
            int id;
            std::string pwd;
            std::cout << "ID: "; std::cin >> id;
            std::cout << "Password: "; std::cin >> pwd;
            std::cin.ignore();
            if (login(id, pwd)) {
                chatMain();
            }
        } else if (op == 2) {
            std::string name, pwd;
            std::cout << "Name: "; std::cin >> name;
            std::cout << "Password: "; std::cin >> pwd;
            std::cin.ignore();
            registerUser(name, pwd);
        } else if (op == 3) {
            std::cout << "Goodbye" << std::endl;
            break;
        } else {
            std::cout << "invalid option" << std::endl;
        }
    }
}

void ChatClient::closeClient() {
    _running = false;
    if (_sockfd > 0) {
        close(_sockfd);
        _sockfd = -1;
    }
}
