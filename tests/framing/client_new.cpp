/**
 * TCP 粘包演示 — 新版客户端（帧协议）
 *
 * 编译: g++ -std=c++11 client_new.cpp -o client_new
 *
 * 先启动 server_new，再运行本程序。
 *
 * recvFramedMessage() 先读 4 字节长度头，再读指定长度的消息体。
 * 无论 TCP 如何合并/拆分，每条消息都能正确分离。
 */
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static bool recvAll(int fd, char* buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, buf + received, len - received, 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

static std::string recvFramedMessage(int fd, bool* ok) {
    uint32_t netLen = 0;
    if (!recvAll(fd, (char*)&netLen, 4)) { *ok = false; return ""; }
    uint32_t msgLen = ntohl(netLen);
    if (msgLen > 1024 * 1024) { *ok = false; return ""; }
    std::string out(msgLen, '\0');
    if (!recvAll(fd, &out[0], msgLen)) { *ok = false; return ""; }
    *ok = true;
    return out;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(12346);
    connect(fd, (sockaddr*)&addr, sizeof(addr));

    std::cout << "新版客户端 已连接 127.0.0.1:12346\n" << std::endl;

    // === 新方案的接收循环 ===
    int recvCount = 0;
    while (true) {
        bool ok = false;
        std::string msg = recvFramedMessage(fd, &ok);
        if (!ok) {
            std::cout << "recvFramedMessage() 失败，连接断开，退出循环" << std::endl;
            break;
        }

        ++recvCount;
        std::cout << "========== 第 " << recvCount << " 条消息 ==========" << std::endl;
        std::cout << "消息长度: " << msg.size() << " 字节" << std::endl;
        std::cout << "消息内容: " << msg << "\n" << std::endl;
    }

    std::cout << "\n总结：" << std::endl;
    std::cout << "  服务端 sendFramedMessage() 了 3 次" << std::endl;
    std::cout << "  客户端成功接收了 " << recvCount << " 条消息" << std::endl;
    std::cout << "  原因：4字节长度头定义了消息边界，与 TCP 分段无关" << std::endl;
    std::cout << "  send 3 次 == 收到 3 条" << std::endl;

    close(fd);
    return 0;
}
