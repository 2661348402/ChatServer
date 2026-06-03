#ifndef GROUP_HPP_
#define GROUP_HPP_

#include "GroupUser.hpp"
#include <vector>
#include <string>

class Group {
public:
    Group(int id = -1, std::string name = "", std::string desc = "")
        : id(id), name(name), desc(desc) {}

    void setId(int id) { this->id = id; }
    void setName(const std::string& name) { this->name = name; }
    void setDesc(const std::string& desc) { this->desc = desc; }
    void setUsers(const std::vector<GroupUser>& users) { this->users = users; }

    int getId() const { return id; }
    std::string getName() const { return name; }
    std::string getDesc() const { return desc; }
    std::vector<GroupUser>& getUsers() { return users; }

private:
    int id;
    std::string name;
    std::string desc;
    std::vector<GroupUser> users;
};

#endif
