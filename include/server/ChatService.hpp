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
using namespace muduo;
using namespace muduo::net;
using namespace std;
using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr& conn, json& js, Timestamp)>;
class ChatService {
public:
    //返回一个实例
    static ChatService* instance();
    //登录
    void login(const TcpConnectionPtr& conn, json& js, Timestamp);
    //注册
    void reg(const TcpConnectionPtr& conn, json& js, Timestamp);
    //处理点对点聊天
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp);
    //添加好友
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp);
    //创建群组
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp);
    //加入群组
    void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp);
    //群聊天
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp);
    //登出
    void loginout(const TcpConnectionPtr& conn, json& js, Timestamp);
    //redis 订阅消息处理
    void redisSubscribeMessage(int channel,string message);
    //获取回调函数
    MsgHandler getHandler(int msgId);
    //处理客户端异常断开
    void clientConnectException(const TcpConnectionPtr& conn);
    //处理服务器异常
    bool reset();

private:
    ChatService();
    //消息和处理业务记录
    unordered_map<int, MsgHandler> msgHandlerMap;
    //用户数据操作类
    UserModel _userModel;
    //离线消息操作类
    OfflineMsgModel _offlineMsgModel;
    //处理朋友类
    FriendModel _friendModel;
    //处理群组数据
    GroupModel _groupModel;
    //在线用户连接记录
    unordered_map<int,TcpConnectionPtr> _userConnMap;
    //记录安全锁
    mutex connMutex;
    //redis
    Redis _redis;
};


#endif