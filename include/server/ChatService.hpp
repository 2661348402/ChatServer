#ifndef CHAT_SERVICE_H_
#define CHAT_SERVICE_H_

#include <muduo/net/TcpConnection.h>
#include <functional>
#include <unordered_map>
#include <mutex>
#include "UserModel.hpp"
#include "OfflineMessageModel.hpp"
#include "FriendModel.hpp"
#include "GroupModel.hpp"
#include "json.hpp"
#include "redis.hpp"

using MsgHandler = std::function<void(const muduo::net::TcpConnectionPtr& conn,
                                      nlohmann::json& js, muduo::Timestamp)>;

class ChatService {
public:
    static ChatService* instance();

    void login(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void reg(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void oneChat(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void addFriend(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void createGroup(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void addGroup(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void groupChat(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void loginout(const muduo::net::TcpConnectionPtr& conn, nlohmann::json& js, muduo::Timestamp);
    void redisSubscribeMessage(int channel, std::string message);

    MsgHandler getHandler(int msgId);
    void clientConnectException(const muduo::net::TcpConnectionPtr& conn);
    bool reset();

    static void sendFramed(const muduo::net::TcpConnectionPtr& conn,
                            const std::string& msg);

private:
    ChatService();

    std::unordered_map<int, MsgHandler> msgHandlerMap;
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;
    std::unordered_map<int, muduo::net::TcpConnectionPtr> _userConnMap;
    std::mutex connMutex;
    Redis _redis;
};

#endif
