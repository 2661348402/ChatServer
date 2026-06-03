#include "ChatServer.hpp"
#include "ChatService.hpp"
#include "Config.hpp"
#include "ConnectionPool.hpp"
#include <signal.h>
#include <iostream>

void serverHandler(int sig) {
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, serverHandler);

    const char* cfgPath = "conf/server.conf";
    if (argc >= 2) {
        cfgPath = argv[1];
    }
    Config::instance().load(cfgPath);

    std::string serverIp = Config::instance().get("server.ip", "127.0.0.1");
    int serverPort = Config::instance().getInt("server.port", 12345);

    std::string dbHost = Config::instance().get("db.host", "127.0.0.1");
    int dbPort = Config::instance().getInt("db.port", 3306);
    std::string dbUser = Config::instance().get("db.user", "root");
    std::string dbPass = Config::instance().get("db.password", "");
    std::string dbName = Config::instance().get("db.name", "chat");
    int poolSize = Config::instance().getInt("db.poolsize", 8);

    std::string redisHost = Config::instance().get("redis.host", "127.0.0.1");
    int redisPort = Config::instance().getInt("redis.port", 6379);

    ConnectionPool::instance().init(dbHost, dbPort, dbUser, dbPass, dbName, poolSize);

    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(serverIp, serverPort);
    ChatServer server(&loop, listenAddr, "ChatServer");

    std::cout << "ChatServer started at " << serverIp << ":" << serverPort << std::endl;

    server.start();
    loop.loop();

    return 0;
}
