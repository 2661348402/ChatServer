#include "redis.hpp"
#include <muduo/base/Logging.h>
#include <cstring>
#include <iostream>

Redis::Redis()
    : _publish_context(nullptr)
    , _subscribe_context(nullptr)
    , _host("127.0.0.1")
    , _port(6379)
{
}

Redis::~Redis() {
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

    std::thread t([this]() { observer_channel_message(); });
    t.detach();
    LOG_INFO << "Redis connected: " << host << ":" << port;
    return true;
}

bool Redis::publish(int channel, const std::string& message) {
    redisReply* reply = (redisReply*)redisCommand(
        _publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR << "redis publish error";
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    return true;
}

bool Redis::subscribe(int channel) {
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context,
                                         "SUBSCRIBE %d", channel)) {
        LOG_ERROR << "subscribe command failed";
        return false;
    }
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done)) {
            LOG_ERROR << "subscribe buffer write failed";
            return false;
        }
    }
    return true;
}

bool Redis::unsubscribe(int channel) {
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context,
                                         "UNSUBSCRIBE %d", channel)) {
        LOG_ERROR << "unsubscribe command failed";
        return false;
    }
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done)) {
            LOG_ERROR << "unsubscribe buffer write failed";
            return false;
        }
    }
    return true;
}

void Redis::observer_channel_message() {
    while (true) {
        redisReply* reply = nullptr;
        if (redisGetReply(_subscribe_context, (void**)&reply) != REDIS_OK) {
            LOG_ERROR << "redis get reply error";
            break;
        }

        if (reply != nullptr && reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
            if (strcmp(reply->element[0]->str, "message") == 0) {
                int channel = atoi(reply->element[1]->str);
                std::string message = reply->element[2]->str;
                _notify_message_handler(channel, message);
            }
        }
        freeReplyObject(reply);
    }
}

void Redis::init_notify_handler(std::function<void(int, std::string)> fn) {
    _notify_message_handler = std::move(fn);
}
