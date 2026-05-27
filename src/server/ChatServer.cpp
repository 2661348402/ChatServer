#include "ChatServer.hpp"
#include "ChatService.hpp"
#include <iostream>
#include "json.hpp"
using json = nlohmann::json;

//初始化聊天服务器
ChatServer::ChatServer(EventLoop* loop,
    const InetAddress& listenAddr,
    const string& nameArg) :
    _loop(loop),
    _server(loop, listenAddr, nameArg) {
    //注册回调函数
    _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));
    _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));
    //设置线程数
    _server.setThreadNum(4);
}
//启动服务
void ChatServer::start() {
    _server.start();
}

//上报链接相关回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn) {
    if (!conn->connected())  {
        ChatService::instance()->clientConnectException(conn);
        conn->shutdown();
    }
}
//上报读写相关回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {

    std::string msg = buf->retrieveAllAsString();
    //增强代码壮硕性
    if(msg.empty()) return;
    try{
        json js = json::parse(msg);
        if(!js.contains("msgId")) return;
        auto msgHandler = ChatService::instance()->getHandler(js["msgId"].get<int>());
        msgHandler(conn, js, receiveTime);
    }catch(const std::exception& e ){
         std::cerr << "Exception: " << e.what() << std::endl;
    }
}

