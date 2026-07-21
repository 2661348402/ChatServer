#include "ChatServer.hpp"
#include "ChatService.hpp"
#include "json.hpp"
#include <muduo/base/Logging.h>
#include <arpa/inet.h>
#include <cstring>

ChatServer::ChatServer(muduo::net::EventLoop* loop,
                       const muduo::net::InetAddress& listenAddr,
                       const std::string& nameArg)
    : _loop(loop)
    , _server(loop, listenAddr, nameArg)
{
    using namespace std::placeholders;
    _server.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1));
    _server.setMessageCallback(
        std::bind(&ChatServer::onMessage, this, _1, _2, _3));
    _loop->runEvery(10.0, [] {
        ChatService::instance()->checkHeartbeatTimeouts();
    });
    _server.setThreadNum(4);
}

void ChatServer::start() {
    _server.start();
}

void ChatServer::onConnection(const muduo::net::TcpConnectionPtr& conn) {
    if (!conn->connected()) {
        ChatService::instance()->clientConnectException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                            muduo::net::Buffer* buf,
                            muduo::Timestamp receiveTime) {
    while (buf->readableBytes() >= 4) {
        uint32_t beLen = 0;
        memcpy(&beLen, buf->peek(), 4);
        uint32_t msgLen = ntohl(beLen);

        if (msgLen > 1024 * 1024) {
            LOG_ERROR << "Message too large: " << msgLen;
            conn->shutdown();
            return;
        }

        if (buf->readableBytes() < 4 + msgLen) {
            break;
        }

        buf->retrieve(4);
        std::string msg = buf->retrieveAsString(msgLen);

        try {
            auto js = nlohmann::json::parse(msg);
            if (!js.contains("msgId")) continue;
            auto msgHandler = ChatService::instance()->getHandler(js["msgId"].get<int>());
            msgHandler(conn, js, receiveTime);
        } catch (const std::exception& e) {
            LOG_ERROR << "Message parse error: " << e.what();
        }
    }
}
