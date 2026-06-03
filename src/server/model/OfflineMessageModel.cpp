#include "OfflineMessageModel.hpp"
#include "ConnectionPool.hpp"
#include <muduo/base/Logging.h>
#include <sstream>

bool OfflineMsgModel::insert(int userid, const std::string& message) {
    auto db = ConnectionPool::instance().getConnection();

    std::string sql = "insert into OfflineMessage(userid, message) values("
        + std::to_string(userid) + "," + db->escape(message) + ")";

    return db->update(sql);
}

std::vector<std::string> OfflineMsgModel::query(int userid) {
    auto db = ConnectionPool::instance().getConnection();
    std::vector<std::string> vec;

    std::string sql = "select message from OfflineMessage where userid = "
        + std::to_string(userid);

    MYSQL_RES* res = db->query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            vec.push_back(row[0]);
        }
        mysql_free_result(res);
    }
    return vec;
}

bool OfflineMsgModel::remove(int userid) {
    auto db = ConnectionPool::instance().getConnection();

    std::string sql = "delete from OfflineMessage where userid = "
        + std::to_string(userid);

    return db->update(sql);
}
