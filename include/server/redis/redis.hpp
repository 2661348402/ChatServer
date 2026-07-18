#ifndef REDIS_HPP_
#define REDIS_HPP_

#include <hiredis/hiredis.h>
#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <set>

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
    void processSubCommands();

private:
    bool reconnectPublish();
    bool reconnectSubscribe();
    bool subscribeControlChannel();
    bool sendSubscribeCommand(const char* op,int channel);
    bool resubscribeAll();
    enum class RedisSubOp {
        Subscribe,
        Unsubscribe
    };

    struct RedisSubCommand {
        RedisSubOp op;
        int channel;
    };
    std::mutex _publishMutex;
    std::mutex _subMutex;
    std::queue<RedisSubCommand> _subCommands;
    bool _running = false;

    redisContext* _publish_context;
    redisContext* _subscribe_context;
    std::function<void(int, std::string)> _notify_message_handler;
    std::string _host;
    int _port;
    std::set<int> _subscribedChannels;
};

#endif
