#ifndef METRICS_HPP_
#define METRICS_HPP_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

class Metrics
{
public:
    static Metrics &instance();

    void incOnlineConnections();
    void decOnlineConnections();
    void setOnlineUsers(std::int64_t value);

    void incParsedMessages();
    void incParseErrors();

    void recordGroupChat(std::chrono::steady_clock::duration cost);
    void recordGroupLocalSend(std::chrono::steady_clock::duration cost);
    void recordRedisPublish(std::chrono::steady_clock::duration cost, bool ok);

    void recordBusinessTaskSubmitted();
    void recordBusinessTaskCompleted();
    void recordBusinessTaskRejected();
    void recordBusinessQueueSize(std::int64_t size);
    void recordBusinessQueueWait(std::int64_t waitUs);
    void recordOfflineQueueSize(std::int64_t size);
    void recordOfflineFlush(std::chrono::steady_clock::duration cost, std::size_t rows);

    std::string snapshot() const;
    void dump() const;

private:
    Metrics() = default;

    std::atomic<std::int64_t> _onlineConnections{0};
    std::atomic<std::int64_t> _onlineUsers{0};

    std::atomic<std::int64_t> _parsedMessages{0};
    std::atomic<std::int64_t> _parseErrors{0};

    std::atomic<std::int64_t> _groupChatCount{0};
    std::atomic<std::int64_t> _groupChatTotalUs{0};

    std::atomic<std::int64_t> _groupLocalSendCount{0};
    std::atomic<std::int64_t> _groupLocalSendTotalUs{0};

    std::atomic<std::int64_t> _redisPublishCount{0};
    std::atomic<std::int64_t> _redisPublishTotalUs{0};
    std::atomic<std::int64_t> _redisPublishFailed{0};

    std::atomic<std::int64_t> _offlineQueueSize{0};
    std::atomic<std::int64_t> _offlineMaxQueueSize{0};

    std::atomic<std::int64_t> _offlineFlushCount{0};
    std::atomic<std::int64_t> _offlineFlushRows{0};
    std::atomic<std::int64_t> _offlineFlushTotalUs{0};
    std::atomic<std::int64_t> _offlineFlushMaxUs{0};

    std::atomic<std::int64_t> _groupChatMaxUs{0};

    std::atomic<std::int64_t> _businessTaskSubmitted{0};
    std::atomic<std::int64_t> _businessTaskCompleted{0};
    std::atomic<std::int64_t> _businessTaskRejected{0};
    std::atomic<std::int64_t> _businessQueueSize{0};
    std::atomic<std::int64_t> _businessMaxQueueSize{0};
    std::atomic<std::int64_t> _businessQueueWaitCount{0};
    std::atomic<std::int64_t> _businessQueueWaitTotalUs{0};
    std::atomic<std::int64_t> _businessQueueWaitMaxUs{0};
};

#endif