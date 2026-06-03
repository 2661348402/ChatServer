#include "db.h"
#include <muduo/base/Logging.h>
#include <cstring>
#include <vector>

MySQL::MySQL() {
    _conn = mysql_init(nullptr);
    if (_conn == nullptr) {
        LOG_ERROR << "mysql_init failed!";
    }
}

MySQL::~MySQL() {
    if (_conn != nullptr) {
        mysql_close(_conn);
    }
}

bool MySQL::connect() {
    return false;
}

bool MySQL::connect(const std::string& host, int port,
                     const std::string& user, const std::string& password,
                     const std::string& dbname) {
    MYSQL* p = mysql_real_connect(_conn,
                                  host.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(),
                                  port, nullptr, 0);
    if (p == nullptr) {
        LOG_ERROR << "MySQL connect failed: " << mysql_error(_conn);
        return false;
    }
    mysql_query(_conn, "set names utf8");
    return true;
}

bool MySQL::update(const std::string& sql) {
    if (mysql_query(_conn, sql.c_str())) {
        LOG_ERROR << "update failed: " << mysql_error(_conn)
                  << " SQL: " << sql;
        return false;
    }
    return true;
}

MYSQL_RES* MySQL::query(const std::string& sql) {
    if (mysql_query(_conn, sql.c_str())) {
        LOG_ERROR << "query failed: " << mysql_error(_conn)
                  << " SQL: " << sql;
        return nullptr;
    }
    return mysql_store_result(_conn);
}

MYSQL* MySQL::getConnection() {
    return _conn;
}

std::string MySQL::escape(const std::string& str) {
    if (!_conn) {
        LOG_ERROR << "MySQL::escape() called with null connection";
        return "''";
    }
    if (str.empty()) return "''";

    // MySQL 官方要求: 缓冲区最小长度为 (原始长度 * 2 + 1)
    std::vector<char> buf(str.size() * 2 + 1);

    unsigned long escapedLen = mysql_real_escape_string(
        _conn, buf.data(), str.c_str(), str.size());

    if (escapedLen == (unsigned long)-1) {
        LOG_ERROR << "mysql_real_escape_string failed: " << mysql_error(_conn);
        return "''";
    }

    std::string result;
    result.reserve(escapedLen + 2);
    result += '\'';
    result.append(buf.data(), escapedLen);
    result += '\'';
    return result;
}
