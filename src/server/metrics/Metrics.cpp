#include "Metrics.hpp"
#include <muduo/base/Logging.h>
#include <sstream>
#include <Config.hpp>
#include <fstream>

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


void Metrics::recordBusinessTaskSubmitted()
{
    _businessTaskSubmitted.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::recordBusinessTaskCompleted()
{
    _businessTaskCompleted.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::recordBusinessTaskRejected()
{
    _businessTaskRejected.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::recordBusinessQueueSize(std::int64_t size)
{
    _businessQueueSize.store(size, std::memory_order_relaxed);
    updateMax(_businessMaxQueueSize, size);
}

void Metrics::recordBusinessQueueWait(std::int64_t waitUs)
{
    _businessQueueWaitCount.fetch_add(1, std::memory_order_relaxed);
    _businessQueueWaitTotalUs.fetch_add(waitUs, std::memory_order_relaxed);
    updateMax(_businessQueueWaitMaxUs, waitUs);
}
void Metrics::recordOfflineQueueSize(std::int64_t size)
{
    _offlineQueueSize.store(size, std::memory_order_relaxed);
    updateMax(_offlineMaxQueueSize, size);
}

void Metrics::recordOfflineFlush(std::chrono::steady_clock::duration cost,
                                 std::size_t rows)
{
    auto costUs = toUs(cost);

    _offlineFlushCount.fetch_add(1, std::memory_order_relaxed);
    _offlineFlushRows.fetch_add(static_cast<std::int64_t>(rows),
                                std::memory_order_relaxed);
    _offlineFlushTotalUs.fetch_add(costUs, std::memory_order_relaxed);
    updateMax(_offlineFlushMaxUs, costUs);
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
    auto bizWaitCount = _businessQueueWaitCount.load();
    auto offlineFlushCount = _offlineFlushCount.load();

    std::ostringstream oss;
    oss << "[METRICS]"
        << " conn=" << _onlineConnections.load()
        << " users=" << _onlineUsers.load()
        << " parsed=" << _parsedMessages.load()
        << " parse_errors=" << _parseErrors.load()
        << " group_chat=" << groupCount
        << " redis_publish=" << redisCount
        << " redis_fail=" << _redisPublishFailed.load()
        << " avg_group_us=" << avg(_groupChatTotalUs.load(), groupCount)
        << " avg_local_send_us=" << avg(_groupLocalSendTotalUs.load(), localCount)
        << " avg_redis_us=" << avg(_redisPublishTotalUs.load(), redisCount)
        << " max_group_us=" << _groupChatMaxUs.load()
        << " biz_submit=" << _businessTaskSubmitted.load()
        << " biz_done=" << _businessTaskCompleted.load()
        << " biz_reject=" << _businessTaskRejected.load()
        << " biz_queue=" << _businessQueueSize.load()
        << " biz_max_queue=" << _businessMaxQueueSize.load()
        << " biz_avg_queue_wait_us=" << avg(_businessQueueWaitTotalUs.load(), bizWaitCount)
        << " biz_max_queue_wait_us=" << _businessQueueWaitMaxUs.load()
        << " offline_queue=" << _offlineQueueSize.load()
        << " offline_max_queue=" << _offlineMaxQueueSize.load()
        << " offline_flush_batch=" << offlineFlushCount
        << " offline_flush_rows=" << _offlineFlushRows.load()
        << " offline_avg_flush_us=" << avg(_offlineFlushTotalUs.load(), offlineFlushCount)
        << " offline_max_flush_us=" << _offlineFlushMaxUs.load();

    return oss.str();
}

void Metrics::dump() const
{
    std::string line = snapshot();
    std::string filePath = Config::instance().get("metrics.file", "");

    if (filePath.empty())
    {
        LOG_INFO << line;
        return;
    }

    std::ofstream out(filePath, std::ios::app);
    if (!out.is_open())
    {
        LOG_ERROR << "failed to open metrics file: " << filePath;
        return;
    }

    out << line << std::endl;
}
