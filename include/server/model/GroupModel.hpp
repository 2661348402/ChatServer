#ifndef GROUP_MODEL_HPP_
#define GROUP_MODEL_HPP_

#include "Group.hpp"
#include <vector>
#include <string>
using namespace std;

class GroupModel {
public:
    // 创建群组
    bool createGroup(Group &group);

    // 加入群组
    void addGroup(int userid, int groupid, string role);

    // 查询用户所在的所有群组（返回群组列表，包含成员）
    vector<Group> queryGroups(int userid);

    // 根据群组id，查询该群所有用户的id（用于群聊转发）
    vector<int> queryGroupUsers(int userid,int groupid);
};

#endif