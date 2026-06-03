#ifndef CONNECTION_POOL_HPP_
#define CONNECTION_POOL_HPP_

#include "db.h"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>

class ConnectionPool {
public:
    static ConnectionPool& instance();

    void init(const std::string& host, int port,
              const std::string& user, const std::string& password,
              const std::string& dbname, int poolSize = 8);

    std::shared_ptr<MySQL> getConnection();

    void returnConnection(MySQL* conn);

private:
    ConnectionPool() = default;
    ~ConnectionPool();
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    std::queue<MySQL*> _pool;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::string _host;
    int _port;
    std::string _user;
    std::string _password;
    std::string _dbname;
    bool _initialized = false;
};

#endif
