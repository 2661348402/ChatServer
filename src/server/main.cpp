#include "ChatServer.hpp"
#include "ChatService.hpp"
#include <signal.h>


void serverHandler(int sig){
    //重置状态
    ChatService::instance()->reset();
    //正常退出
    exit(0);
}

int main(int argc, char* argv[])
{
    signal(SIGINT, serverHandler);

    // 默认配置
    const char* ip = "127.0.0.1";
    uint16_t port = 12345;

    // 如果传入了 IP 和端口，覆盖默认值
    if (argc >= 3)
    {
        ip = argv[1];
        port = atoi(argv[2]);
    }

    EventLoop loop;
    InetAddress listenAddr(ip, port);
    ChatServer server(&loop, listenAddr, "ChatServer");

    std::cout << "ChatServer 启动在: " << ip << ":" << port << std::endl;

    server.start();
    loop.loop();

    return 0;
}