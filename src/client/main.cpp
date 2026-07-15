#include "Client.hpp"
#include <iostream>
#include <cstdlib> // atoi

int main(int argc, char* argv[])
{
    // 默认 IP 和端口（连接 Nginx 负载均衡器）
    const char* ip = "127.0.0.1";
    int port = 12345;

    // 如果命令行传了 IP 和端口，就覆盖默认值
    if (argc >= 3)
    {
        ip = argv[1];
        port = atoi(argv[2]);
    }
    // 参数格式错误
    else if (argc == 2)
    {
        std::cerr << "用法: " << argv[0] << " [IP] [端口]" << std::endl;
        std::cerr << "示例: " << argv[0] << " 192.168.1.100 9000" << std::endl;
        return 1;
    }

    ChatClient client;
    if (client.connectServer(ip, port))
    {
        std::cout << "已连接服务器: " << ip << ":" << port << std::endl;
        client.mainMenu();
    }
    else
    {
        std::cerr << "连接失败: " << ip << ":" << port << std::endl;
    }

    return 0;
}