#ifndef GROUP_HPP_
#define GROUP_HPP_

#include "GroupUser.hpp"
#include <iostream>
#include <vector>
#include <string>
using namespace std;

// 群组类（你要的：id, name, desc, 成员列表）
class Group {
public:
    Group(int id = -1,string name = "",string desc = ""){
        this->id = id;
        this->name = name;
        this->desc = desc;
    }

    void setId(int id) { this->id = id; }
    void setName(const string& name) { this->name = name; }
    void setDesc(const string& desc) { this->desc = desc; }
    void setUsers(const vector<GroupUser>& users) { this->users = users; }

    int getId() const { return id; }
    string getName() const { return name; }
    string getDesc() const { return desc; }
    vector<GroupUser>& getUsers() { return users; }

private:
    int id;                 // 群组id
    string name;            // 群名称
    string desc;            // 群描述
    vector<GroupUser> users;// 群成员列表
};

#endif