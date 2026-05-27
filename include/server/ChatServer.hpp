#ifndef CHAT_SERVER_H_
#define CHAT_SERVER_H_

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <string>
#include <set>

using namespace muduo;
using namespace muduo::net;

class ChatServer {
public:
    //初始化聊天服务器
    ChatServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const string& nameArg);
    //启动服务
    void start();
private:
    //上报链接相关回调函数
    void onConnection(const TcpConnectionPtr&);
    //上报读写相关回调函数
    void onMessage(const TcpConnectionPtr&,
        Buffer*,
        Timestamp);
    TcpServer _server;
    EventLoop* _loop;

};



#endif