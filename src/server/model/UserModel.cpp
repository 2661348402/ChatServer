#include "UserModel.hpp"
#include "ConnectionPool.hpp"
#include "SHA256.hpp"
#include <muduo/base/Logging.h>
#include <cstring>
#include <sstream>

bool UserModel::insert(User& user) {
    auto db = ConnectionPool::instance().getConnection();

    std::string hash = SHA256::hash(user.getPwd());
    std::string sql = "insert into user(name,password,state) values("
        + db->escape(user.getName()) + ","
        + db->escape(hash) + ","
        + db->escape(user.getState()) + ")";

    if (db->update(sql)) {
        long long uid = mysql_insert_id(db->getConnection());
        user.setId((int)uid);
        return true;
    }
    return false;
}

User UserModel::queryById(int id) {
    auto db = ConnectionPool::instance().getConnection();
    std::string sql = "select id,name,password,state from user where id="
        + std::to_string(id);

    MYSQL_RES* res = db->query(sql);
    if (!res) return User();

    MYSQL_ROW row = mysql_fetch_row(res);
    User user;
    if (row) {
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        user.setPwd(row[2]);
        user.setState(row[3]);
    }
    mysql_free_result(res);
    return user;
}

User UserModel::queryByName(const std::string& name) {
    auto db = ConnectionPool::instance().getConnection();
    std::string sql = "select id,name,password,state from user where name="
        + db->escape(name);

    MYSQL_RES* res = db->query(sql);
    if (!res) return User();

    MYSQL_ROW row = mysql_fetch_row(res);
    User user;
    if (row) {
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        user.setPwd(row[2]);
        user.setState(row[3]);
    }
    mysql_free_result(res);
    return user;
}

std::vector<User> UserModel::queryByNameLike(const std::string& keyword) {
    auto db = ConnectionPool::instance().getConnection();
    std::string sql = "select id,name,password,state from user where name like '%"
        + keyword + "%'";
    // keyword is validated to be alphanumeric before calling

    MYSQL_RES* res = db->query(sql);
    std::vector<User> result;
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            User user;
            user.setId(atoi(row[0]));
            user.setName(row[1]);
            user.setPwd(row[2]);
            user.setState(row[3]);
            result.push_back(user);
        }
        mysql_free_result(res);
    }
    return result;
}

bool UserModel::updateState(User& user) {
    auto db = ConnectionPool::instance().getConnection();
    std::string sql = "update user set state="
        + db->escape(user.getState())
        + " where id=" + std::to_string(user.getId());
    return db->update(sql);
}

bool UserModel::stateReset() {
    auto db = ConnectionPool::instance().getConnection();
    std::string sql = "update user set state='offline' where state='online'";
    return db->update(sql);
}

std::unordered_map<int, std::string>UserModel::queryStatesByIds(const std::vector<int>& ids) {
    std::unordered_map<int, std::string> states;
    if (ids.empty()) {
        return states;
    }

    auto db = ConnectionPool::instance().getConnection();

    std::string sql = "select id,state from user where id in (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
            sql += ",";
        }
        sql += std::to_string(ids[i]);
    }
    sql += ")";

    MYSQL_RES* res = db->query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            states[atoi(row[0])] = row[1];
        }
        mysql_free_result(res);
    }

    return states;
}
