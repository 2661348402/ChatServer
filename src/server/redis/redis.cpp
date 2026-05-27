#include "redis.hpp"
#include <iostream>
#include <cstring>

using namespace std;
Redis::Redis()
    : _publish_context(nullptr), _subscribe_context(nullptr)
{
}

Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }
    if (_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

// 连接 redis 服务器
bool Redis::connect()
{
    // 1. 创建 publish 上下文
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr || _publish_context->err)
    {
        std::cerr << "redis connect publish context error: " 
                  << (_publish_context ? _publish_context->errstr : "nullptr") << std::endl;
        return false;
    }

    // 2. 创建 subscribe 上下文
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (_subscribe_context == nullptr || _subscribe_context->err)
    {
        std::cerr << "redis connect subscribe context error: " 
                  << (_subscribe_context ? _subscribe_context->errstr : "nullptr") << std::endl;
        redisFree(_publish_context);
        _publish_context = nullptr;
        return false;
    }
    std::thread t ([&](){observer_channel_message();});
    t.detach();
    std::cout<<"connect redis-server success!"<<std::endl;

    return true;
}

// 向 redis 指定的通道 channel 发布消息
bool Redis::publish(int channel, std::string message)
{
    redisReply* reply = (redisReply*)redisCommand(
        _publish_context, "PUBLISH %d %s", channel, message.c_str()
    );
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
    {
        std::cerr << "redis publish command error" << std::endl;
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    return true;
}

bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息
    // 这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源

    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel))
    {
        std::cerr << "subscribe command failed!" << std::endl;
        return false;
    }

    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            std::cerr << "subscribe command failed!" << std::endl;
            return false;
        }
    }

    return true;
}

// 向 redis 指定的通道 unsubscribe 取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        std::cerr << "unsubscribe command failed!" << std::endl;
        return false;
    }

    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            std::cerr << "unsubscribe command failed!" << std::endl;
            return false;
        }
    }

    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    while (true)
    {
        // 阻塞等待订阅消息
        redisReply* reply = nullptr;
        if (redisGetReply(_subscribe_context, (void**)&reply) != REDIS_OK)
        {
            std::cerr << "redis get reply error" << std::endl;
            break;
        }

        // 收到的订阅消息是一个数组：
        // reply->element[0] = "message"
        // reply->element[1] = channel
        // reply->element[2] = message
        if (reply != nullptr && reply->type == REDIS_REPLY_ARRAY && reply->elements == 3)
        {
            if (strcmp(reply->element[0]->str, "message") == 0)
            {
                int channel = atoi(reply->element[1]->str);
                std::string message = reply->element[2]->str;
                _notify_message_handler(channel, message);

            }
        }

        freeReplyObject(reply);
    }
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(std::function<void(int, std::string)> fn)
{
    _notify_message_handler = std::move(fn);
}