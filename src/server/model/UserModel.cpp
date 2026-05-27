#include "UserModel.hpp"
#include <iostream>
using namespace std;

bool UserModel::insert(User& user)
{
string sql = "insert into user(name,password,state) values('"
        + user.getName() + "','"
        + user.getPwd() + "','"  
        + user.getState() + "')";

    MySQL db;
    if (!db.connect()) return false;
    if (db.update(sql))
    {
        // 获取自增id
        long long uid = mysql_insert_id(db.getConnection());
        user.setId((int)uid);
        return true;
    }
    return false;
}

User UserModel::queryById(int id)
{
    string sql = "select id,name,password,state from user where id=" + to_string(id);
    MySQL db;
    if (!db.connect()) return User();

    MYSQL_RES* res = db.query(sql);
    if (!res) return User();

    MYSQL_ROW row = mysql_fetch_row(res);
    User user;
    if (row)
    {
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        user.setPwd(row[2]);
        user.setState(row[3]);
    }
    mysql_free_result(res);
    return user;
}

// User UserModel::queryByName(string name)
// {
//     string sql = "select id,name,password,state from user where name='" + name + "'";
//     MySQL db;
//     if (!db.connect()) return User();

//     MYSQL_RES* res = db.query(sql);
//     if (!res) return User();

//     MYSQL_ROW row = mysql_fetch_row(res);
//     User user;
//     if (row)
//     {
//         user.setId(atoi(row[0]));
//         user.setName(row[1]);
//         user.setPwd(row[2]);
//         user.setState(atoi(row[3]));
//     }
//     mysql_free_result(res);
//     return user;
// }

bool UserModel::updateState(User& user)
{
    string sql = "update user set state='"+ user.getState() +"' where id="+to_string(user.getId());
    MySQL db;
    return db.connect() && db.update(sql);
}

bool UserModel::stateReset(){
    string sql = "update user set state='offline' where state = 'online'";
    MySQL db;
    return db.connect() && db.update(sql);
}