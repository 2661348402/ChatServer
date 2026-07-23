#include "Metrics.hpp"
#include <muduo/base/Logging.h>
#include <sstream>

Metrics &Metrics::instance()
{
    static Metrics metrics;
    return metrics;
}

void Metrics::incOnlineConnections()
{
    _onlineConnections.fetch_add(1);
}

void Metrics::decOnlineConnections()
{
    if (_onlineConnections.load() > 0)
    {
        _onlineConnections.fetch_sub(1);
    }
}

void Metrics::setOnlineUsers(std::int64_t value)
{
    _onlineUsers.store(value);
}

void Metrics::incParsedMessages()
{
    _parsedMessages.fetch_add(1);
}

void Metrics::incParseErrors()
{
    _parseErrors.fetch_add(1);
}

static std::int64_t toUs(std::chrono::steady_clock::duration d)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

static void updateMax(std::atomic<std::int64_t> &target, std::int64_t value)
{
    std::int64_t old = target.load();
    while (value > old && !target.compare_exchange_weak(old, value))
    {
    }
}

void Metrics::recordGroupChat(std::chrono::steady_clock::duration cost)
{
    auto costUs = toUs(cost);
    _groupChatCount.fetch_add(1);
    _groupChatTotalUs.fetch_add(costUs);
    updateMax(_groupChatMaxUs, costUs);
    
}

void Metrics::recordGroupLocalSend(std::chrono::steady_clock::duration cost)
{
    _groupLocalSendCount.fetch_add(1);
    _groupLocalSendTotalUs.fetch_add(toUs(cost));
}

void Metrics::recordRedisPublish(std::chrono::steady_clock::duration cost, bool ok)
{
    _redisPublishCount.fetch_add(1);
    _redisPublishTotalUs.fetch_add(toUs(cost));
    if (!ok)
    {
        _redisPublishFailed.fetch_add(1);
    }
}

void Metrics::recordOfflineStore(std::chrono::steady_clock::duration cost, std::size_t rows)
{
    auto costUs = toUs(cost);
    _offlineStoreBatchCount.fetch_add(1);
    _offlineStoreRows.fetch_add(static_cast<std::int64_t>(rows));
    _offlineStoreTotalUs.fetch_add(costUs);
    updateMax(_offlineStoreMaxUs, costUs);
}

void Metrics::incOfflineDegrade()
{
    _offlineDegrade.fetch_add(1);
}
std::string Metrics::snapshot() const
{
    auto avg = [](std::int64_t total, std::int64_t count)
    {
        return count == 0 ? 0 : total / count;
    };

    auto groupCount = _groupChatCount.load();
    auto localCount = _groupLocalSendCount.load();
    auto redisCount = _redisPublishCount.load();
    auto offlineStoreBatchCount = _offlineStoreBatchCount.load();

    std::ostringstream oss;
    oss << "[METRICS]"
        << " conn=" << _onlineConnections.load()
        << " users=" << _onlineUsers.load()
        << " parsed=" << _parsedMessages.load()
        << " parse_errors=" << _parseErrors.load()
        << " group_chat=" << groupCount
        << " redis_publish=" << redisCount
        << " redis_fail=" << _redisPublishFailed.load()
        << " offline_degrade=" << _offlineDegrade.load()
        << " avg_group_us=" << avg(_groupChatTotalUs.load(), groupCount)
        << " avg_local_send_us=" << avg(_groupLocalSendTotalUs.load(), localCount)
        << " avg_redis_us=" << avg(_redisPublishTotalUs.load(), redisCount)
        << " avg_offline_us=" << avg(_offlineStoreTotalUs.load(), offlineStoreBatchCount)
        << " offline_store_batch=" << offlineStoreBatchCount
        << " offline_store_rows=" << _offlineStoreRows.load()
        << " max_group_us=" << _groupChatMaxUs.load()
        << " max_offline_us=" << _offlineStoreMaxUs.load();

    return oss.str();
}

void Metrics::dump() const
{
    LOG_INFO << snapshot();
}
