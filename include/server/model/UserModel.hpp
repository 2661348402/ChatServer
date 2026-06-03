#ifndef USERMODEL_H_
#define USERMODEL_H_
#include "User.hpp"
#include <vector>

class UserModel {
public:
    bool insert(User& user);
    User queryById(int id);
    User queryByName(const std::string& name);
    std::vector<User> queryByNameLike(const std::string& keyword);
    bool updateState(User& user);
    bool stateReset();
};

#endif
