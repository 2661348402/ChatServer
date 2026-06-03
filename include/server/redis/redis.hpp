#ifndef REDIS_HPP_
#define REDIS_HPP_

#include <hiredis/hiredis.h>
#include <functional>
#include <string>
#include <thread>

class Redis {
public:
    Redis();
    ~Redis();

    bool connect();
    bool connect(const std::string& host, int port);
    bool publish(int channel, const std::string& message);
    bool subscribe(int channel);
    bool unsubscribe(int channel);
    void observer_channel_message();
    void init_notify_handler(std::function<void(int, std::string)> fn);

private:
    redisContext* _publish_context;
    redisContext* _subscribe_context;
    std::function<void(int, std::string)> _notify_message_handler;
    std::string _host;
    int _port;
};

#endif
