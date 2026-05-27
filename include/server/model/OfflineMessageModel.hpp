#ifndef OFFLINEMSG_H_
#define OFFLINEMSG_H_

#include "db.h"  // 包含你写好的数据库类
#include <vector>
#include <string>

using namespace std;

// 离线消息表 数据操作类（DAO）
class OfflineMsgModel
{
public:
    // 1. 插入用户的离线消息
    bool insert(int userid, const string& message);

    // 2. 查询用户的离线消息（返回该用户所有离线消息）
    vector<string> query(int userid);

    // 3. 删除用户的离线消息（用户登录成功后清空）
    bool remove(int userid);

};

#endif 
