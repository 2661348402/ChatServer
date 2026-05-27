#include "FriendModel.hpp"
#include <iostream>
using namespace std;

// 添加好友（双向好友关系）
bool FriendModel::insert(int userId, int friendId) {
    // 组装SQL：插入 userid -> friendid
    MySQL db;
    if(!db.connect()) return false;

    char sql[1024] = {0};
    sprintf(sql, "insert into Friend(userid, friendid) values(%d, %d)", userId, friendId);

    return db.update(sql);

    // // 双向好友：再插入 friendid -> userid（业务常用）
    // sprintf(sql, "insert into friend(userid, friendid) values(%d, %d)", friendId, userId);
    // return db.update(sql);
}

// 查询好友列表，返回 User 列表
vector<User> FriendModel::query(int id) {
    vector<User> vec;
    MySQL db;
    if(!db.connect()) return vec;

    // 联合查询：从 friend 表找到所有好友，再从 user 表获取信息
    char sql[1024] = {0};
    sprintf(sql, "select u.id, u.name, u.state from user u inner join Friend f on u.id = f.friendid where f.userid = %d", id);

    MYSQL_RES *res = db.query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        // 遍历结果集，封装 User 对象
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