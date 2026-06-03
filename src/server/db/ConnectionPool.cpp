#include "ConnectionPool.hpp"
#include <muduo/base/Logging.h>

ConnectionPool& ConnectionPool::instance() {
    static ConnectionPool pool;
    return pool;
}

void ConnectionPool::init(const std::string& host, int port,
                           const std::string& user, const std::string& password,
                           const std::string& dbname, int poolSize) {
    if (_initialized) return;
    _host = host;
    _port = port;
    _user = user;
    _password = password;
    _dbname = dbname;

    for (int i = 0; i < poolSize; ++i) {
        auto* conn = new MySQL();
        conn->connect(host, port, user, password, dbname);
        _pool.push(conn);
    }
    _initialized = true;
    LOG_INFO << "ConnectionPool initialized with " << poolSize << " connections";
}

ConnectionPool::~ConnectionPool() {
    std::lock_guard<std::mutex> lock(_mutex);
    while (!_pool.empty()) {
        delete _pool.front();
        _pool.pop();
    }
}

std::shared_ptr<MySQL> ConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_pool.empty()) {
        _cv.wait(lock);
    }
    MySQL* conn = _pool.front();
    _pool.pop();
    return std::shared_ptr<MySQL>(conn,
        [this](MySQL* p) { this->returnConnection(p); });
}

void ConnectionPool::returnConnection(MySQL* conn) {
    if (!conn) return;
    std::lock_guard<std::mutex> lock(_mutex);
    _pool.push(conn);
    _cv.notify_one();
}
