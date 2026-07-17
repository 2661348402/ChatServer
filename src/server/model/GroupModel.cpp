#include "GroupModel.hpp"
#include "ConnectionPool.hpp"
#include <muduo/base/Logging.h>
#include <sstream>
#include <unordered_map>

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
    std::unordered_map<int,size_t> groupIdx;


    std::string sql =
        "select g.id,g.groupname,g.groupdesc,"
        "u.id,u.name,u.state,gm.grouprole "
        "from `AllGroup` g "
        "inner join GroupUser self on g.id = self.groupid "
        "inner join GroupUser gm on g.id = gm.groupid "
        "inner join user u on gm.userid = u.id "
        "where self.userid = " + std::to_string(userid) + " "
        "order by g.id,u.id";

    MYSQL_RES* res = db->query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            int gid = atoi(row[0]);
            auto it = groupIdx.find(gid);
            if(it == groupIdx.end()) {
                //第一次加入
                Group group;
                group.setId(gid);
                group.setName(row[1]);
                group.setDesc(row[2]);
                vec.push_back(group);
                groupIdx[gid] = vec.size() - 1;
                it = groupIdx.find(gid);
            }
            GroupUser gu;
            gu.setId(atoi(row[3]));
            gu.setName(row[4]);
            gu.setState(row[5]);
            gu.setRole(row[6]);
            vec[it->second].getUsers().push_back(gu);

        }
        mysql_free_result(res);
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
