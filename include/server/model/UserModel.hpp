#ifndef USERMODEL_H_
#define USERMODEL_H_
#include "User.hpp"
#include "db.h"

class UserModel
{
public:
    // 新增用户
    bool insert(User& user);
    // 根据id查用户
    User queryById(int id);
    // // 根据用户名查询
    // User queryByName(string name);
    // 更新用户状态
    bool updateState(User& user);
    //重置用户状态
    bool stateReset();
};

#endif