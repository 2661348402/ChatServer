#include "FriendModel.hpp"
#include "ConnectionPool.hpp"
#include <muduo/base/Logging.h>
#include <sstream>

bool FriendModel::insert(int userId, int friendId) {
    auto db = ConnectionPool::instance().getConnection();

    std::string sql1 = "insert into Friend(userid, friendid) values("
        + std::to_string(userId) + "," + std::to_string(friendId) + ")";
    std::string sql2 = "insert into Friend(userid, friendid) values("
        + std::to_string(friendId) + "," + std::to_string(userId) + ")";

    return db->update(sql1) && db->update(sql2);
}

std::vector<User> FriendModel::query(int id) {
    auto db = ConnectionPool::instance().getConnection();
    std::vector<User> vec;

    std::string sql = "select u.id, u.name, u.state from user u "
        "inner join Friend f on u.id = f.friendid where f.userid = "
        + std::to_string(id);

    MYSQL_RES* res = db->query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            User user;
            user.setId(atoi(row[0]));
            user.setName(row[1]);
            user.setState(row[2]);
            vec.push_back(user);
        }
        mysql_free_result(res);
    }
    return vec;
}
