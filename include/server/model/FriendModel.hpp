#ifndef FRIEND_MODEL_H_
#define FRIEND_MODEL_H_

#include <vector>
#include "User.hpp"

class FriendModel {
public:
    bool insert(int userId, int friendId);
    std::vector<User> query(int id);
};

#endif
