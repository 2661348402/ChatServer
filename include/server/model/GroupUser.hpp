#ifndef GROUP_USER_HPP_
#define GROUP_USER_HPP_

#include "User.hpp"
#include <string>

class GroupUser : public User {
public:
    void setRole(const std::string& role) { this->role = role; }
    std::string getRole() const { return role; }

private:
    std::string role;
};

#endif
