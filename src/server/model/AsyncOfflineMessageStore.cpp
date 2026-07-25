#include "AsyncOfflineMessageStore.hpp"
#include "Metrics.hpp"
AsyncOfflineMessageStore::AsyncOfflineMessageStore()
    : _running(true),
      _worker(&AsyncOfflineMessageStore::workerLoop, this)
{
}
AsyncOfflineMessageStore::~AsyncOfflineMessageStore()
{
    _running = false;
    _cv.notify_all();

    if (_worker.joinable())
    {
        _worker.join();
    }
}

void AsyncOfflineMessageStore::enqueue(int userid, std::string message)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push(OfflineTask{{userid}, std::move(message)});
        Metrics::instance().recordOfflineQueueSize(
            static_cast<std::int64_t>(_queue.size()));
    }

    _cv.notify_one();
}

void AsyncOfflineMessageStore::enqueueBatch(std::vector<int> userids, std::string message)
{
    if (userids.empty())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push(OfflineTask{std::move(userids), std::move(message)});
        Metrics::instance().recordOfflineQueueSize(
            static_cast<std::int64_t>(_queue.size()));
    }

    _cv.notify_one();
}

void AsyncOfflineMessageStore::workerLoop()
{
    constexpr std::size_t MAX_FLUSH_ROWS = 1000;
    constexpr auto FLUSH_INTERVAL = std::chrono::milliseconds(50);

    while (true)
    {
        std::vector<OfflineTask> batch;
        std::size_t rows = 0;

        {
            std::unique_lock<std::mutex> lock(_mutex);

            // 第一步：没有任务时一直等，直到有任务或准备退出
            _cv.wait(lock, [this]
                     { return !_running || !_queue.empty(); });

            if (!_running && _queue.empty())
            {
                break;
            }

            // 从第一条任务开始计时
            auto deadline = std::chrono::steady_clock::now() + FLUSH_INTERVAL;

            while (rows < MAX_FLUSH_ROWS)
            {
                while (!_queue.empty() && rows < MAX_FLUSH_ROWS)
                {
                    OfflineTask task = std::move(_queue.front());
                    _queue.pop();

                    rows += task.userids.size();
                    batch.push_back(std::move(task));
                }

                if (rows >= MAX_FLUSH_ROWS || !_running)
                {
                    break;
                }

                // 第二步：队列暂时空了，继续等到 50ms 截止，看有没有新任务进来
                if (_cv.wait_until(lock, deadline, [this]
                                   { return !_running || !_queue.empty(); }))
                {
                    continue;
                }

                // 到 50ms 了，还没攒满，也要 flush
                break;
            }

            Metrics::instance().recordOfflineQueueSize(
                static_cast<std::int64_t>(_queue.size()));
        }

        if (!batch.empty())
        {
            flush(batch);
        }
    }
}

void AsyncOfflineMessageStore::flush(std::vector<OfflineTask> &tasks)
{
    std::vector<OfflineMessageRow> rows;

    for (auto &task : tasks)
    {
        for (int userid : task.userids)
        {
            rows.push_back({userid, task.message});
        }
    }

    if (rows.empty())
    {
        return;
    }

    auto begin = std::chrono::steady_clock::now();
    _offlineMsgModel.insertRows(rows);
    auto end = std::chrono::steady_clock::now();

    Metrics::instance().recordOfflineFlush(end - begin, rows.size());
}