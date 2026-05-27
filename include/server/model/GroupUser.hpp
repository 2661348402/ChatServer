#ifndef GROUP_USER_HPP_
#define GROUP_USER_HPP_

#include "User.hpp"
#include <string>
using namespace std;

// 群成员类 继承 用户基类
class GroupUser : public User
{
public:
    void setRole(const string& role) { this->role = role; }
    string getRole() const { return role; }

private:
    string role; // 群组角色：creator 群主 / normal 普通成员
};

#endif