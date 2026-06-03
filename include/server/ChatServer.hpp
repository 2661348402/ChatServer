#ifndef CHAT_SERVER_H_
#define CHAT_SERVER_H_

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <string>

class ChatServer {
public:
    ChatServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr,
               const std::string& nameArg);

    void start();

private:
    void onConnection(const muduo::net::TcpConnectionPtr&);
    void onMessage(const muduo::net::TcpConnectionPtr&,
                   muduo::net::Buffer*,
                   muduo::Timestamp);

    muduo::net::TcpServer _server;
    muduo::net::EventLoop* _loop;
};

#endif
