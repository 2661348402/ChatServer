#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>

#include <iostream>
#include <string>

using namespace muduo;
using namespace muduo::net;

class ChatServer {
public:
    ChatServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const std::string& serverName)
        :_loop(loop),
        _server(loop, listenAddr, serverName)
    {
        //设置连接回调函数
        _server.setConnectionCallback(
            [this](const TcpConnectionPtr& conn) { onConnection(conn); });
        //设置消息回调函数
        _server.setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
                onMessage(conn, buf, receiveTime);
            });
    }
    //
    void start() {
        _server.start();
    }
private:
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            std::cout << "New connection from "
                << conn->peerAddress().toIpPort()
                << " -> " << conn->localAddress().toIpPort()
                << std::endl;
        }
        else {
            std::cout << "Disconnected: "
                << conn->peerAddress().toIpPort()
                << std::endl;
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
        std::string msg = buf->retrieveAllAsString();
        std::cout << "Received at " << receiveTime.toString() << ": "
            << msg << std::endl;

        // 简单回显
        conn->send("Server received: " + msg);
    }

    EventLoop* _loop;
    TcpServer _server;

};

int main() {
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 12345);
    ChatServer server(&loop, listenAddr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}
