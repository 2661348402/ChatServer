#include<vector>
#include<string>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<atomic>
#include<OfflineMessageModel.hpp>


class AsyncOfflineMessageStore
{
public:
    AsyncOfflineMessageStore();
    ~AsyncOfflineMessageStore();

    void enqueue(int userid, std::string message);
    void enqueueBatch(std::vector<int> userids, std::string message);

private:
    struct OfflineTask
    {
        std::vector<int> userids;
        std::string message;
    };

    void workerLoop();
    void flush(std::vector<OfflineTask> &tasks);

    OfflineMsgModel _offlineMsgModel;
    std::queue<OfflineTask> _queue;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _worker;
    std::atomic_bool _running{true};
};