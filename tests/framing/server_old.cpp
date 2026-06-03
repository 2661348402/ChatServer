/**
 * TCP 粘包演示 — 旧版服务端
 *
 * 编译: g++ -std=c++11 server_old.cpp -o server_old
 * 运行: ./server_old
 *
 * 直接 send() 发送多条 JSON，无长度前缀。
 * 配合 client_old.cpp 观察粘包现象。
 */
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(12345);
    bind(listenFd, (sockaddr*)&addr, sizeof(addr));
    listen(listenFd, 1);

    std::cout << "旧版服务端 监听 127.0.0.1:12345，等待客户端连接..." << std::endl;

    int connFd = accept(listenFd, nullptr, nullptr);
    std::cout << "客户端已连接" << std::endl;

    // 模拟：连续发送 3 条 JSON 消息（没有长度前缀）
    const char* msgs[] = {
        "{\"msgId\":5,\"from\":\"alice\",\"message\":\"hello\"}",
        "{\"msgId\":5,\"from\":\"bob\",\"message\":\"world\"}",
        "{\"msgId\":9,\"groupid\":1,\"from\":\"charlie\",\"message\":\"lunch?\"}",
    };

    for (int i = 0; i < 3; ++i) {
        send(connFd, msgs[i], strlen(msgs[i]), 0);
        std::cout << "[服务端] send() 第 " << (i + 1) << " 条: " << msgs[i] << std::endl;
    }

    std::cout << "\n[服务端] 3条消息已全部发出（调用了3次 send）" << std::endl;
    std::cout << "[服务端] 等待客户端关闭..." << std::endl;

    // 等待客户端断开
    char c;
    recv(connFd, &c, 1, 0);  // 阻塞直到客户端关闭

    close(connFd);
    close(listenFd);
    std::cout << "[服务端] 退出" << std::endl;
    return 0;
}
