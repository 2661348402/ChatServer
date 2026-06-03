/**
 * TCP 粘包演示 — 新版服务端（帧协议）
 *
 * 编译: g++ -std=c++11 server_new.cpp -o server_new
 * 运行: ./server_new
 *
 * 每条消息前加 4 字节大端长度前缀。
 * 配合 client_new.cpp 验证帧协议正确拆分每条消息。
 */
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static void sendFramedMessage(int fd, const std::string& buf) {
    uint32_t netLen = htonl(buf.size());
    std::string packet(reinterpret_cast<const char*>(&netLen), 4);
    packet.append(buf);
    send(fd, packet.c_str(), packet.size(), 0);
}

int main() {
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(12346);
    bind(listenFd, (sockaddr*)&addr, sizeof(addr));
    listen(listenFd, 1);

    std::cout << "新版服务端 监听 127.0.0.1:12346，等待客户端连接..." << std::endl;

    int connFd = accept(listenFd, nullptr, nullptr);
    std::cout << "客户端已连接" << std::endl;

    // 同样的 3 条消息，用帧协议发送
    const char* msgs[] = {
        "{\"msgId\":5,\"from\":\"alice\",\"message\":\"hello\"}",
        "{\"msgId\":5,\"from\":\"bob\",\"message\":\"world\"}",
        "{\"msgId\":9,\"groupid\":1,\"from\":\"charlie\",\"message\":\"lunch?\"}",
    };

    for (int i = 0; i < 3; ++i) {
        std::cout << "[服务端] sendFramedMessage() 第 " << (i + 1)
                  << " 条: [4字节长度头][" << strlen(msgs[i]) << "字节消息]" << std::endl;
        std::cout << "         内容: " << msgs[i] << std::endl;
        sendFramedMessage(connFd, msgs[i]);
    }

    std::cout << "\n[服务端] 3条消息已全部发出" << std::endl;
    std::cout << "[服务端] 等待客户端关闭..." << std::endl;

    char c;
    recv(connFd, &c, 1, 0);

    close(connFd);
    close(listenFd);
    std::cout << "[服务端] 退出" << std::endl;
    return 0;
}
