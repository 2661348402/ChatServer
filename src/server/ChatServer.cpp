#include "ChatServer.hpp"
#include "ChatService.hpp"
#include "json.hpp"
#include <muduo/base/Logging.h>
#include <arpa/inet.h>
#include <cstring>
#include "Metrics.hpp"
#include "Config.hpp"


ChatServer::ChatServer(muduo::net::EventLoop *loop,
                       const muduo::net::InetAddress &listenAddr,
                       const std::string &nameArg)
    : _loop(loop), _server(loop, listenAddr, nameArg), _businessPool(
                                                           Config::instance().getInt("business.threads", 8),
                                                           Config::instance().getInt("business.queue_size", 10000))
{
    using namespace std::placeholders;

    _server.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1));

    _server.setMessageCallback(
        std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    _loop->runEvery(10.0, []
                    { ChatService::instance()->checkHeartbeatTimeouts(); });
    _loop->runEvery(10.0, []
                    { Metrics::instance().dump(); });

    _server.setThreadNum(Config::instance().getInt("server.threads", 4));
}

void ChatServer::start()
{
    _server.start();
    _businessPool.start();
}

void ChatServer::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        Metrics::instance().incOnlineConnections();
    }
    else
    {
        Metrics::instance().decOnlineConnections();

        ChatService::instance()->clientConnectException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                           muduo::net::Buffer *buf,
                           muduo::Timestamp receiveTime)
{
    while (buf->readableBytes() >= 4)
    {
        uint32_t beLen = 0;
        memcpy(&beLen, buf->peek(), 4);
        uint32_t msgLen = ntohl(beLen);

        if (msgLen > 1024 * 1024)
        {
            LOG_ERROR << "Message too large: " << msgLen;
            conn->shutdown();
            return;
        }

        if (buf->readableBytes() < 4 + msgLen)
        {
            break;
        }

        buf->retrieve(4);
        std::string msg = buf->retrieveAsString(msgLen);

        try
        {
            auto js = nlohmann::json::parse(msg);
            Metrics::instance().incParsedMessages();
            if (!js.contains("msgId"))
                continue;

            auto msgHandler = ChatService::instance()->getHandler(js["msgId"].get<int>());
            size_t connKey = reinterpret_cast<size_t>(conn.get());

            bool accepted = _businessPool.submit(
                connKey,
                [conn, js = std::move(js), receiveTime, msgHandler]() mutable
                {
                    if (!conn->connected())
                    {
                        return;
                    }
                    msgHandler(conn, js, receiveTime);
                });

            if (!accepted)
            {
                LOG_ERROR << "business queue full, close connection";
                conn->shutdown();
                return;
            }
        }
        catch (const std::exception &e)
        {
            Metrics::instance().incParseErrors();
            LOG_ERROR << "Message parse error: " << e.what();
        }
    }
}
