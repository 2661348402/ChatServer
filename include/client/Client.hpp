#ifndef CLIENT_HPP_
#define CLIENT_HPP_

#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <functional>
#include "User.hpp"
#include "public.hpp"
using namespace std;

class ChatClient
{
public:
    ChatClient();
    ~ChatClient();

    // 初始化连接服务器
    bool connectServer(const string& ip, int port);
    // 客户端主菜单入口
    void mainMenu();
    // 登录
    bool login(int id, const string& pwd);
    // 注册
    bool registerUser(const string& name, const string& pwd);
    // 登录成功后聊天主界面
    void chatMain();
    // 子线程读取服务端消息
    void recvMsgThread();
    // 发送消息
    void sendMsg(const string& buf);
    // 关闭客户端
    void closeClient();

private:
    int _sockfd;
    bool _running;
    User _curUser;

    // 命令处理函数
    void help(const std::string& param);
    void addFriend(const std::string& param);
    void oneToOneChat(const std::string& param);
    void createGroup(const std::string& param);
    void addGroup(const std::string& param);
    void groupChat(const std::string& param);
    void loginOut(const std::string& param);

    // 命令注册
    std::unordered_map<std::string, std::function<void(const std::string&)>> _cmdHandler;
    void initCommandHandler();
};

#endif