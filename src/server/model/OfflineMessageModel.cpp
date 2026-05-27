#include "OfflineMessageModel.hpp"




bool OfflineMsgModel::insert(int userid, const string& message)
{
    // 组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "insert into OfflineMessage(userid, message) values(%d, '%s')",
            userid, message.c_str());

    // 执行插入
    MySQL db;
    return db.connect() && db.update(sql);
}

vector<string> OfflineMsgModel::query(int userid)
{
    vector<string> vec;
    MySQL db;
    if (!db.connect()) return vec;

    char sql[1024] = {0};
    sprintf(sql, "select message from OfflineMessage where userid = %d", userid);

    // 执行查询
    MYSQL_RES* res = db.query(sql);
    if (res != nullptr)
    {
        // 读取每一行消息
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            vec.push_back(row[0]);
        }
        // 释放结果集
        mysql_free_result(res);
    }
    return vec;
}

bool OfflineMsgModel::remove(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from OfflineMessage where userid = %d", userid);
    MySQL db;
    
    return db.connect() && db.update(sql);
}