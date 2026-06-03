#include "GroupModel.hpp"
#include "ConnectionPool.hpp"
#include <muduo/base/Logging.h>
#include <sstream>

bool GroupModel::createGroup(Group& group) {
    auto db = ConnectionPool::instance().getConnection();

    std::string sql = "insert into `AllGroup`(groupname, groupdesc) values("
        + db->escape(group.getName()) + ","
        + db->escape(group.getDesc()) + ")";

    if (db->update(sql)) {
        group.setId(mysql_insert_id(db->getConnection()));
        return true;
    }
    return false;
}

void GroupModel::addGroup(int userid, int groupid, const std::string& role) {
    auto db = ConnectionPool::instance().getConnection();

    std::string sql = "insert into GroupUser values("
        + std::to_string(groupid) + "," + std::to_string(userid) + ","
        + db->escape(role) + ")";

    db->update(sql);
}

std::vector<Group> GroupModel::queryGroups(int userid) {
    auto db = ConnectionPool::instance().getConnection();
    std::vector<Group> vec;

    std::string sql = "select a.id,a.groupname,a.groupdesc from `AllGroup` a "
        "inner join GroupUser b on a.id = b.groupid "
        "where b.userid = " + std::to_string(userid);

    MYSQL_RES* res = db->query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            Group group;
            group.setId(atoi(row[0]));
            group.setName(row[1]);
            group.setDesc(row[2]);
            vec.push_back(group);
        }
        mysql_free_result(res);
    }

    for (Group& group : vec) {
        std::string sql2 = "select a.id,a.name,a.state,b.grouprole from user a "
            "inner join GroupUser b on a.id = b.userid "
            "where b.groupid = " + std::to_string(group.getId());

        MYSQL_RES* res2 = db->query(sql2);
        if (res2 != nullptr) {
            MYSQL_ROW row2;
            while ((row2 = mysql_fetch_row(res2)) != nullptr) {
                GroupUser gu;
                gu.setId(atoi(row2[0]));
                gu.setName(row2[1]);
                gu.setState(row2[2]);
                gu.setRole(row2[3]);
                group.getUsers().push_back(gu);
            }
            mysql_free_result(res2);
        }
    }

    return vec;
}

std::vector<int> GroupModel::queryGroupUsers(int userid, int groupid) {
    auto db = ConnectionPool::instance().getConnection();
    std::vector<int> vec;

    std::string sql = "select userid from GroupUser where groupid = "
        + std::to_string(groupid) + " and userid != " + std::to_string(userid);

    MYSQL_RES* res = db->query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            vec.push_back(atoi(row[0]));
        }
        mysql_free_result(res);
    }

    return vec;
}
