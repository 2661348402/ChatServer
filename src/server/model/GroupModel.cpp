#include "GroupModel.hpp"
#include "db.h"
#include "GroupUser.hpp"
#include <iostream>
using namespace std;

// 创建群组
bool GroupModel::createGroup(Group &group) {
    MySQL mysql;
    if(!mysql.connect()) return false;
    char sql[1024] = {0};
    sprintf(sql, "insert into `AllGroup`(groupname, groupdesc) values('%s', '%s')",
        group.getName().c_str(),
        group.getDesc().c_str());

    if (mysql.update(sql)) {
        // 获取刚插入的群组ID
        group.setId(mysql_insert_id(mysql.getConnection()));
        return true;
    }
    return false;
}

// 加入群组
void GroupModel::addGroup(int userid, int groupid, string role) {
    MySQL mysql;
    if(!mysql.connect()) return;

    char sql[1024] = {0};
    sprintf(sql, "insert into GroupUser values(%d, %d, '%s')",
        groupid, userid, role.c_str());

    mysql.update(sql);
}

// 查询用户加入的所有群组
vector<Group> GroupModel::queryGroups(int userid) {
    vector<Group> vec;
    MySQL mysql;
    if(!mysql.connect()) return vec;

    // 1. 先查用户加入了哪些群组
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from `AllGroup` a \
                  inner join GroupUser b on a.id = b.groupid \
                  where b.userid = %d", userid);

    MYSQL_RES *res = mysql.query(sql);
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

    // 2. 为每个群组查询群成员信息
    for (Group &group : vec) {
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from user a \
                      inner join GroupUser b on a.id = b.userid \
                      where b.groupid = %d", group.getId());

        MYSQL_RES *res2 = mysql.query(sql);
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

// 根据群组id，查询群组里所有用户的id（排除自己）
vector<int> GroupModel::queryGroupUsers(int userid,int groupid) {
     vector<int> vec;
    MySQL mysql;
    if(!mysql.connect()) return vec;
   
    char sql[1024] = {0};
    sprintf(sql, "select userid from GroupUser where groupid = %d and userid != %d", groupid,userid);

    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            vec.push_back(atoi(row[0]));
        }
        mysql_free_result(res);
    }

    return vec;
}