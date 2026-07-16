#include "redis.hpp"
#include <muduo/base/Logging.h>
#include <cstring>
#include <iostream>


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

    _publish_context = redisConnect(host.c_str(), port);
    if (_publish_context == nullptr || _publish_context->err) {
        LOG_ERROR << "redis publish connect error: "
                  << (_publish_context ? _publish_context->errstr : "nullptr");
        return false;
    }

    _subscribe_context = redisConnect(host.c_str(), port);
    if (_subscribe_context == nullptr || _subscribe_context->err) {
        LOG_ERROR << "redis subscribe connect error: "
                  << (_subscribe_context ? _subscribe_context->errstr : "nullptr");
        redisFree(_publish_context);
        _publish_context = nullptr;
        return false;
    }

    // 先订阅控制频道，用来唤醒 observer 线程
    if (redisAppendCommand(_subscribe_context, "SUBSCRIBE %d", REDIS_CONTROL_CHANNEL) == REDIS_ERR) {
        LOG_ERROR << "subscribe control channel failed";
        return false;
    }

    int done = 0;
    while (!done) {
        if (redisBufferWrite(_subscribe_context, &done) == REDIS_ERR) {
            LOG_ERROR << "write control subscribe failed";
            return false;
        }
    }

    _running = true;
    std::thread t([this]() { observer_channel_message(); });
    t.detach();

    LOG_INFO << "Redis connected: " << host << ":" << port;

    return true;
}

bool Redis::publish(int channel, const std::string& message) {
    std::lock_guard<std::mutex> lock(_publishMutex);

    if (_publish_context == nullptr) {
        LOG_ERROR << "redis publish context is null";
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(
        _publish_context, "PUBLISH %d %s", channel, message.c_str());

    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR << "redis publish error";
        if (reply != nullptr) {
            freeReplyObject(reply);
        }
        return false;
    }

    freeReplyObject(reply);
    return true;
}

bool Redis::subscribe(int channel) {
    {
        std::lock_guard<std::mutex> lock(_subMutex);
        _subCommands.push({RedisSubOp::Subscribe, channel});
    }
    publish(REDIS_CONTROL_CHANNEL, "wake");
    return true;
}

bool Redis::unsubscribe(int channel) {
    {
        std::lock_guard<std::mutex> lock(_subMutex);
        _subCommands.push({RedisSubOp::Unsubscribe, channel});
    }
    publish(REDIS_CONTROL_CHANNEL, "wake");
    return true;
}

void Redis::observer_channel_message() {
    while (_running) {
        processSubCommands();

        redisReply* reply = nullptr;
        int ret = redisGetReply(_subscribe_context, (void**)&reply);

        if (ret == REDIS_ERR) {
            if (_subscribe_context != nullptr && _subscribe_context->err == REDIS_ERR_IO) {
                // 超时也可能走这里，具体 hiredis 版本表现略有差异
                continue;
            }

            LOG_ERROR << "redis get reply error";
            continue;
        }

        if (reply != nullptr) {
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
                if (strcmp(reply->element[0]->str, "message") == 0) {
                    int channel = atoi(reply->element[1]->str);
                    if(channel==0) continue;
                    std::string message = reply->element[2]->str;
                    _notify_message_handler(channel, message);
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

        if (redisAppendCommand(_subscribe_context, "%s %d", op, cmd.channel) == REDIS_ERR) {
            LOG_ERROR << "redis append " << op << " failed: "<<_subscribe_context->errstr;
            continue;
        }

        int done = 0;
        while (!done) {
            if (redisBufferWrite(_subscribe_context, &done) == REDIS_ERR) {
                LOG_ERROR << "redis write " << op << " failed: "<<_subscribe_context->errstr;
                break;
            }
        }
    }
}