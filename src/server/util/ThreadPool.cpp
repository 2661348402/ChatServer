#include "ThreadPool.hpp"
#include <muduo/base/Logging.h>

ThreadPool::ThreadPool(size_t threadNum, size_t maxQueueSize)
    : _maxQueueSize(maxQueueSize == 0 ? 10000 : maxQueueSize)
    , _running(false)
{
    if (threadNum == 0) {
        threadNum = 1;
    }

    _workers.reserve(threadNum);
    for (size_t i = 0; i < threadNum; ++i) {
        _workers.emplace_back(new Worker());
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    
    bool expected = false;
    if (!_running.compare_exchange_strong(expected, true)) {
        return;
    }

    _threads.reserve(_workers.size());
    for (size_t i = 0; i < _workers.size(); ++i) {
        _threads.emplace_back(&ThreadPool::runWorker, this, i);
    }

    LOG_INFO << "Business ThreadPool started, threads=" << _workers.size()
             << ", maxQueueSize=" << _maxQueueSize;
}

void ThreadPool::stop() {
    bool expected = true;
    if (!_running.compare_exchange_strong(expected, false)) {
        return;
    }

    for (auto& worker : _workers) {
        worker->cv.notify_all();
    }

    for (auto& thread : _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    _threads.clear();
    LOG_INFO << "Business ThreadPool stopped";
}

bool ThreadPool::submit(size_t key, Task task) {
    if (!_running || _workers.empty()) {
        return false;
    }

    size_t index = key % _workers.size();
    auto& worker = *_workers[index];

    {
        std::lock_guard<std::mutex> lock(worker.mutex);

        if (worker.tasks.size() >= _maxQueueSize) {
            LOG_ERROR << "Business ThreadPool queue full, worker=" << index
                      << ", queueSize=" << worker.tasks.size();
            return false;
        }

        worker.tasks.emplace_back(std::move(task));
    }

    worker.cv.notify_one();
    return true;
}

void ThreadPool::runWorker(size_t index) {
    auto& worker = *_workers[index];

    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(worker.mutex);
            worker.cv.wait(lock, [&] {
                return !_running || !worker.tasks.empty();
            });

            if (!_running && worker.tasks.empty()) {
                break;
            }

            task = std::move(worker.tasks.front());
            worker.tasks.pop_front();
        }

        try {
            task();
        } catch (const std::exception& e) {
            LOG_ERROR << "Business task exception, worker=" << index
                      << ", error=" << e.what();
        } catch (...) {
            LOG_ERROR << "Business task unknown exception, worker=" << index;
        }
    }
}