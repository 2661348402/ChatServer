#include "redis.hpp"
#include <muduo/base/Logging.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <cerrno>

namespace {
const int REDIS_CONTROL_CHANNEL = 0;
}

Redis::Redis()
    : _publish_context(nullptr)
    , _subscribe_context(nullptr)
    , _host("127.0.0.1")
    , _port(6379)
{
}

Redis::~Redis() {
    _running = false;
    if (_publish_context != nullptr) {
        redisFree(_publish_context);
    }
    if (_subscribe_context != nullptr) {
        redisFree(_subscribe_context);
    }
}

bool Redis::connect() {
    return connect(_host, _port);
}

bool Redis::connect(const std::string& host, int port) {

    _host = host;
    _port = port;
    _running = true;

    bool publishOk = reconnectPublish();
    if (!publishOk) {
        LOG_ERROR << "redis publish initial connect failed";
    }

    bool subscribeOk = reconnectSubscribe();
    if (!subscribeOk) {
        LOG_ERROR << "redis subscribe initial connect failed";
    }

    //启动订阅线程
    std::thread t([this]() { observer_channel_message(); });
    t.detach();

    if( publishOk && subscribeOk){
        LOG_INFO << "Redis connected: " << host << ":" << port;
    }else{
        LOG_ERROR << "Redis initial connect failed, service will keep retrying";
    }
   
    return publishOk && subscribeOk;

}
bool Redis::publish(int channel, const std::string& message) {
    std::lock_guard<std::mutex> lock(_publishMutex);

    if (_publish_context == nullptr) {
        LOG_ERROR << "redis publish context is null, try reconnect";

        if (!reconnectPublish()) {
            LOG_ERROR << "redis publish failed: reconnect failed, channel="
                      << channel;
            return false;
        }
    }

    redisReply* reply = (redisReply*)redisCommand(
        _publish_context,
        "PUBLISH %d %s",
        channel,
        message.c_str()
    );

    if (reply != nullptr && reply->type != REDIS_REPLY_ERROR) {
        freeReplyObject(reply);
        return true;
    }

    LOG_ERROR << "redis publish error, try reconnect and retry, channel="
              << channel;

    if (reply != nullptr) {
        freeReplyObject(reply);
    }

    if (!reconnectPublish()) {
        LOG_ERROR << "redis publish retry failed: reconnect failed, channel="
                  << channel;
        return false;
    }

    reply = (redisReply*)redisCommand(
        _publish_context,
        "PUBLISH %d %s",
        channel,
        message.c_str()
    );

    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR << "redis publish retry failed, channel=" << channel;

        if (reply != nullptr) {
            freeReplyObject(reply);
        }

        return false;
    }

    freeReplyObject(reply);

    LOG_INFO << "redis publish recovered after reconnect, channel=" << channel;
    return true;
}

bool Redis::subscribe(int channel) {
    {
        std::lock_guard<std::mutex> lock(_subMutex);
        _subCommands.push({RedisSubOp::Subscribe, channel});
        _subscribedChannels.insert(channel);
    }
    publish(REDIS_CONTROL_CHANNEL, "wake");
    return true;
}

bool Redis::unsubscribe(int channel) {
    {
        std::lock_guard<std::mutex> lock(_subMutex);
        _subCommands.push({RedisSubOp::Unsubscribe, channel});
        _subscribedChannels.erase(channel);
    }
    publish(REDIS_CONTROL_CHANNEL, "wake");
    return true;
}
void Redis::observer_channel_message() {
    while (_running) {
        processSubCommands();

        if (_subscribe_context == nullptr) {
            LOG_ERROR << "redis subscribe context is null, try reconnect";

            if (!reconnectSubscribe()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            resubscribeAll();
        }

        redisReply* reply = nullptr;
        int ret = redisGetReply(_subscribe_context, (void**)&reply);

        if (ret == REDIS_ERR) {
            if (reply != nullptr) {
                freeReplyObject(reply);
            }

            // 如果你给 redisSetTimeout 设置了读超时，超时可能也会走这里
            if (_subscribe_context != nullptr &&
                _subscribe_context->err == REDIS_ERR_IO &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }

            LOG_ERROR << "redis subscribe connection error: "
                      << (_subscribe_context ? _subscribe_context->errstr : "null");

            if (_subscribe_context != nullptr) {
                redisFree(_subscribe_context);
                _subscribe_context = nullptr;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        if (reply != nullptr) {
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
                if (strcmp(reply->element[0]->str, "message") == 0) {
                    int channel = atoi(reply->element[1]->str);
                    if(channel!=0 && _notify_message_handler){
                        std::string message = reply->element[2]->str;
                        _notify_message_handler(channel, message);
                    } 
                }
            }

            freeReplyObject(reply);
        }
    }
}

void Redis::init_notify_handler(std::function<void(int, std::string)> fn) {
    _notify_message_handler = std::move(fn);
}
void Redis::processSubCommands() {
    std::queue<RedisSubCommand> commands;

    {
        std::lock_guard<std::mutex> lock(_subMutex);
        commands.swap(_subCommands);
    }

    while (!commands.empty()) {
        RedisSubCommand cmd = commands.front();
        commands.pop();

        const char* op =
            cmd.op == RedisSubOp::Subscribe ? "SUBSCRIBE" : "UNSUBSCRIBE";

        LOG_INFO << "redis process command: " << op << " " << cmd.channel;

        sendSubscribeCommand(op,cmd.channel);
        if (!sendSubscribeCommand(op, cmd.channel)) {
             LOG_ERROR << "redis " << op << " failed, mark subscribe context broken";

            if (_subscribe_context != nullptr) {
                 redisFree(_subscribe_context);
                 _subscribe_context = nullptr;
            }
            break;
        }
    }
}

bool Redis::reconnectPublish() {
    if (_publish_context != nullptr) {
        redisFree(_publish_context);
        _publish_context = nullptr;
    }

    LOG_INFO << "redis publish reconnecting";

    _publish_context = redisConnect(_host.c_str(), _port);
    if (_publish_context == nullptr || _publish_context->err) {
        LOG_ERROR << "redis publish reconnect error: "
                  << (_publish_context ? _publish_context->errstr : "nullptr");

        if (_publish_context != nullptr) {
            redisFree(_publish_context);
            _publish_context = nullptr;
        }

        return false;
    }

    LOG_INFO << "redis publish reconnect success";
    return true;
}

bool Redis::reconnectSubscribe() {
    if (_subscribe_context != nullptr) {
        redisFree(_subscribe_context);
        _subscribe_context = nullptr;
    }

    LOG_INFO << "redis subscribe reconnecting";

    _subscribe_context = redisConnect(_host.c_str(), _port);
    if (_subscribe_context == nullptr || _subscribe_context->err) {
        LOG_ERROR << "redis subscribe reconnect error: "
                  << (_subscribe_context ? _subscribe_context->errstr : "nullptr");

        if (_subscribe_context != nullptr) {
            redisFree(_subscribe_context);
            _subscribe_context = nullptr;
        }

        return false;
    }

    if (!subscribeControlChannel()) {
        redisFree(_subscribe_context);
        _subscribe_context = nullptr;
        return false;
    }

    LOG_INFO << "redis subscribe reconnect success";
    return true;
}
bool Redis::subscribeControlChannel() {
    return sendSubscribeCommand("SUBSCRIBE",REDIS_CONTROL_CHANNEL);
}
bool Redis::sendSubscribeCommand(const char* op,int channel) {

    if (_subscribe_context == nullptr) {
        return false;
    }

    if (redisAppendCommand(
            _subscribe_context,
            "%s %d",
            op,channel) == REDIS_ERR) {
        LOG_ERROR << op << " redis append subscribe failed: "
                  << _subscribe_context->errstr;
        return false;
    }

    int done = 0;
    while (!done) {
        if (redisBufferWrite(_subscribe_context, &done) == REDIS_ERR) {
            LOG_ERROR << op << "redis write subscribe failed: "
                      << _subscribe_context->errstr;
            return false;
        }
    }

    return true;
}
bool Redis::resubscribeAll() {
    std::set<int> channels;
    bool ok = true;
    {
        std::lock_guard<std::mutex> lock(_subMutex);
        channels = _subscribedChannels;
    }

    for (int channel : channels) {
        if(!sendSubscribeCommand("SUBSCRIBE",channel)){
            LOG_ERROR << "redis resubscribe failed, channel=" << channel;
            ok = false;
        }
    }

    return ok;
}