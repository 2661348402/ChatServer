#ifndef FFIEND_MODEL_H_
#define FFIEND_MODEL_H_

#include <vector>
#include "User.hpp"
#include "db.h"
using namespace std;

//操作好友数据类
class FriendModel{
public:
    //添加好友
    bool insert(int userId,int friendId);
    //查询好友列表
    vector<User> query(int id);
    
};



#endif