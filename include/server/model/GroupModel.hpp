#ifndef GROUP_MODEL_HPP_
#define GROUP_MODEL_HPP_

#include "Group.hpp"
#include <vector>
#include <string>

class GroupModel {
public:
    bool createGroup(Group& group);
    void addGroup(int userid, int groupid, const std::string& role);
    std::vector<Group> queryGroups(int userid);
    std::vector<int> queryGroupUsers(int userid, int groupid);
};

#endif
