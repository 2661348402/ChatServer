/**
 * TCP 粘包演示 — 旧版客户端
 *
 * 编译: g++ -std=c++11 client_old.cpp -o client_old
 *
 * 先启动 server_old，再运行本程序。
 *
 * recv() 在 while 循环中接收，但每次 recv() 拿到的字节数
 * 和 send() 的次数没有对应关系 —— 这就是粘包。
 */
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(12345);
    connect(fd, (sockaddr*)&addr, sizeof(addr));

    std::cout << "旧版客户端 已连接 127.0.0.1:12345\n" << std::endl;

    // === 旧方案的接收循环（和原 Client.cpp 逻辑一样） ===
    int recvCount = 0;
    while (true) {
        char buf[1024] = {0};
        int len = recv(fd, buf, sizeof(buf) - 1, 0);
        if (len <= 0) {
            std::cout << "recv() 返回 " << len << "，连接断开，退出循环" << std::endl;
            break;
        }

        ++recvCount;
        std::cout << "========== 第 " << recvCount << " 次 recv() ==========" << std::endl;
        std::cout << "读到 " << len << " 字节" << std::endl;
        std::cout << "原始内容:\n  " << std::string(buf, len) << "\n" << std::endl;

        // 模拟 json::parse —— 三个 JSON 拼在一起不是合法 JSON
        std::string raw(buf, len);
        if (raw.find("}{") != std::string::npos) {
            std::cout << "!!! 发现 " << std::string(buf, len) << " 中包含多个 JSON !!!" << std::endl;
            std::cout << "!!! json::parse 会报错，因为拼在一起的 JSON 非法 !!!" << std::endl;
            std::cout << "!!! 服务端调用了 3 次 send()，但客户端第 1 次 recv() 就拿到了全部数据 !!!" << std::endl;
        }
    }

    std::cout << "\n总结：" << std::endl;
    std::cout << "  服务端 send() 了 3 次" << std::endl;
    std::cout << "  客户端 recv() 循环只执行了 " << recvCount << " 次" << std::endl;
    std::cout << "  原因：TCP 是字节流，不保留消息边界" << std::endl;
    std::cout << "  send 3 次 != recv 3 次" << std::endl;

    close(fd);
    return 0;
}
