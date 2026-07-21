#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool(size_t threadNum, size_t maxQueueSize);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void start();
    void stop();

    bool submit(size_t key, Task task);

private:
    struct Worker {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<Task> tasks;
    };

    void runWorker(size_t index);

    std::vector<std::unique_ptr<Worker>> _workers;
    std::vector<std::thread> _threads;
    size_t _maxQueueSize;
    std::atomic<bool> _running;
};

#endif