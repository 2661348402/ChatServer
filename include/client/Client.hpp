#ifndef CLIENT_HPP_
#define CLIENT_HPP_

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <functional>
#include "User.hpp"
#include "public.hpp"

class ChatClient {
public:
    ChatClient();
    ~ChatClient();

    bool connectServer(const std::string& ip, int port);
    void mainMenu();
    bool login(int id, const std::string& pwd);
    bool registerUser(const std::string& name, const std::string& pwd);
    void chatMain();
    void recvMsgThread();
    void sendMsg(const std::string& buf);
    void closeClient();

private:
    int _sockfd;
    std::atomic<bool> _running;
    User _curUser;

    void help(const std::string& param);
    void addFriend(const std::string& param);
    void oneToOneChat(const std::string& param);
    void createGroup(const std::string& param);
    void addGroup(const std::string& param);
    void groupChat(const std::string& param);
    void loginOut(const std::string& param);

    std::unordered_map<std::string, std::function<void(const std::string&)>> _cmdHandler;
    void initCommandHandler();
};

#endif
