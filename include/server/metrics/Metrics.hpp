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
    void recordOfflineStore(std::chrono::steady_clock::duration cost, std::size_t row);

    void incOfflineDegrade();

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

    std::atomic<std::int64_t> _offlineStoreBatchCount{0}; // 落库批次数
    std::atomic<std::int64_t> _offlineStoreRows{0};       // 实际插入行数
    std::atomic<std::int64_t> _offlineStoreTotalUs{0};
    std::atomic<std::int64_t> _offlineDegrade{0};

    std::atomic<std::int64_t> _groupChatMaxUs{0};
    std::atomic<std::int64_t> _offlineStoreMaxUs{0};
};

#endif