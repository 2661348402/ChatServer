#include "public.hpp"
#include "ChatService.hpp"
#include "Group.hpp"
#include "SHA256.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <string>
#include <arpa/inet.h>

using namespace std::placeholders;

ChatService* ChatService::instance() {
    static ChatService service;
    return &service;
}

void ChatService::sendFramed(const muduo::net::TcpConnectionPtr& conn,
                              const std::string& msg) {
    uint32_t netLen = htonl(msg.size());
    std::string packet(reinterpret_cast<const char*>(&netLen), 4);
    packet.append(msg);
    conn->send(packet);
}

void ChatService::login(const muduo::net::TcpConnectionPtr& conn,
                         nlohmann::json& js, muduo::Timestamp) {
    LOG_INFO << "do login thing";
    int id = js["id"];
    std::string pwd = js["password"];
    User user = _userModel.queryById(id);

    if (user.getId() == -1) {
        nlohmann::json response;
        response["msgId"] = LOG_MSG_ACK;
        response["errno"] = 1;
        response["errMessage"] = "user not exist";
        sendFramed(conn, response.dump());
    } else if (user.getPwd() != SHA256::hash(pwd)) {
        nlohmann::json response;
        response["msgId"] = LOG_MSG_ACK;
        response["errno"] = 2;
        response["errMessage"] = "password error";
        sendFramed(conn, response.dump());
    } else {
        if (user.getState() == "online") {
            nlohmann::json response;
            response["msgId"] = LOG_MSG_ACK;
            response["errno"] = 3;
            response["errMessage"] = "already online";
            sendFramed(conn, response.dump());
        } else {
            {
                std::lock_guard<std::mutex> lock(connMutex);
                _userConnMap[user.getId()] = conn;
            }

            _redis.subscribe(id);

            user.setState("online");
            if (!_userModel.updateState(user)) {
                LOG_ERROR << "update state failed";
            }

            nlohmann::json response;
            response["msgId"] = LOG_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            std::vector<std::string> messages = _offlineMsgModel.query(user.getId());
            if (!messages.empty()) {
                response["offlineMsg"] = messages;
                _offlineMsgModel.remove(user.getId());
            }

            std::vector<Group> groupVec = _groupModel.queryGroups(id);
            std::vector<std::string> groups;
            for (auto& group : groupVec) {
                nlohmann::json js;
                js["id"] = group.getId();
                js["name"] = group.getName();
                js["desc"] = group.getDesc();
                // Serialize group members
                nlohmann::json membersArr = nlohmann::json::array();
                for (auto& gu : group.getUsers()) {
                    nlohmann::json m;
                    m["id"] = gu.getId();
                    m["name"] = gu.getName();
                    m["role"] = gu.getRole();
                    m["state"] = gu.getState();
                    membersArr.push_back(m);
                }
                js["members"] = membersArr;
                groups.push_back(js.dump());
            }
            response["groups"] = groups;

            std::vector<std::string> friendVec;
            std::vector<User> userVec = _friendModel.query(id);
            for (auto& u : userVec) {
                nlohmann::json js;
                js["id"] = u.getId();
                js["name"] = u.getName();
                js["state"] = u.getState();
                friendVec.push_back(js.dump());
            }
            response["friends"] = friendVec;
            sendFramed(conn, response.dump());
        }
    }
}

void ChatService::reg(const muduo::net::TcpConnectionPtr& conn,
                       nlohmann::json& js, muduo::Timestamp) {
    LOG_INFO << "do reg thing";
    std::string name = js["name"];
    std::string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);

    bool state = _userModel.insert(user);
    if (state) {
        nlohmann::json response;
        response["msgId"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        sendFramed(conn, response.dump());
    } else {
        nlohmann::json response;
        response["msgId"] = REG_MSG_ACK;
        response["errno"] = 1;
        sendFramed(conn, response.dump());
    }
}

void ChatService::oneChat(const muduo::net::TcpConnectionPtr& conn,
                           nlohmann::json& js, muduo::Timestamp receiveTime) {
    int toid = js["toid"];
    js["sendtime"] = receiveTime.toFormattedString();

    {
        std::lock_guard<std::mutex> lock(connMutex);
        auto iter = _userConnMap.find(toid);
        if (iter != _userConnMap.end()) {
            sendFramed(iter->second, js.dump());
            return;
        }
    }

    User user = _userModel.queryById(toid);
    if (user.getState() == "online") {
        _redis.publish(toid, js.dump());
        return;
    }

    _offlineMsgModel.insert(toid, js.dump());
}

MsgHandler ChatService::getHandler(int msgId) {
    auto iter = msgHandlerMap.find(msgId);
    if (iter == msgHandlerMap.end()) {
        return [=](const muduo::net::TcpConnectionPtr& conn,
                    nlohmann::json& js, muduo::Timestamp) {
            LOG_ERROR << "msgId: " << msgId << " can't find handler";
        };
    }
    return iter->second;
}

ChatService::ChatService() {
    msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    msgHandlerMap.insert({LOGIN_OUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    if (_redis.connect()) {
        _redis.init_notify_handler(
            std::bind(&ChatService::redisSubscribeMessage, this, _1, _2));
    }
}

void ChatService::redisSubscribeMessage(int userid, std::string msg) {
    {
        std::lock_guard<std::mutex> lock(connMutex);
        auto iter = _userConnMap.find(userid);
        if (iter != _userConnMap.end()) {
            sendFramed(iter->second, msg);
            return;
        }
    }
    _offlineMsgModel.insert(userid, msg);
}

void ChatService::loginout(const muduo::net::TcpConnectionPtr& conn,
                            nlohmann::json& js, muduo::Timestamp) {
    int id = js["id"];
    {
        std::lock_guard<std::mutex> lock(connMutex);
        auto iter = _userConnMap.find(id);
        if (iter != _userConnMap.end()) {
            _userConnMap.erase(iter);
        }
    }
    _redis.unsubscribe(id);
    User user;
    user.setId(id);
    user.setState("offline");
    _userModel.updateState(user);
    LOG_INFO << "id: " << id << " logged out";
}

void ChatService::clientConnectException(const muduo::net::TcpConnectionPtr& conn) {
    int id = -1;
    {
        std::lock_guard<std::mutex> lock(connMutex);
        for (auto iter = _userConnMap.begin(); iter != _userConnMap.end(); ++iter) {
            if (iter->second == conn) {
                id = iter->first;
                _userConnMap.erase(iter);
                break;
            }
        }
    }
    if (id != -1) {
        _redis.unsubscribe(id);
        User user;
        user.setId(id);
        user.setState("offline");
        _userModel.updateState(user);
        LOG_INFO << "id: " << id << " disconnected";
    }
}

bool ChatService::reset() {
    return _userModel.stateReset();
}

void ChatService::addFriend(const muduo::net::TcpConnectionPtr& conn,
                             nlohmann::json& js, muduo::Timestamp) {
    int friendId = js["friendId"];
    int userId = js["id"];

    nlohmann::json response;
    response["msgId"] = ADD_FRIEND_MSG_ACK;

    if (_friendModel.insert(userId, friendId)) {
        User friendUser = _userModel.queryById(friendId);
        response["errno"] = 0;
        response["friendId"] = friendId;
        response["friendName"] = friendUser.getName();
        response["friendState"] = friendUser.getState();
    } else {
        response["errno"] = 1;
        response["errMessage"] = "failed to add friend";
    }
    sendFramed(conn, response.dump());
}

void ChatService::createGroup(const muduo::net::TcpConnectionPtr& conn,
                               nlohmann::json& js, muduo::Timestamp) {
    int userId = js["id"];
    std::string groupname = js["groupname"];
    std::string groupdesc = js["groupdesc"];
    Group group(-1, groupname, groupdesc);

    nlohmann::json response;
    response["msgId"] = CREATE_GROUP_MSG_ACK;

    if (_groupModel.createGroup(group)) {
        _groupModel.addGroup(userId, group.getId(), "creator");
        User creator = _userModel.queryById(userId);
        response["errno"] = 0;
        response["groupId"] = group.getId();
        response["groupName"] = groupname;
        response["groupDesc"] = groupdesc;
        nlohmann::json membersArr = nlohmann::json::array();
        nlohmann::json m;
        m["id"] = userId;
        m["name"] = creator.getName();
        m["role"] = "creator";
        m["state"] = "online";
        membersArr.push_back(m);
        response["members"] = membersArr;
    } else {
        response["errno"] = 1;
        response["errMessage"] = "failed to create group";
    }
    sendFramed(conn, response.dump());
}

void ChatService::addGroup(const muduo::net::TcpConnectionPtr& conn,
                            nlohmann::json& js, muduo::Timestamp) {
    int userId = js["id"];
    int groupId = js["groupId"];
    _groupModel.addGroup(userId, groupId, "normal");

    nlohmann::json response;
    response["msgId"] = ADD_GROUP_MSG_ACK;
    response["errno"] = 0;
    response["groupId"] = groupId;

    // Query updated group info including members
    std::vector<Group> groups = _groupModel.queryGroups(userId);
    for (auto& g : groups) {
        if (g.getId() == groupId) {
            response["groupName"] = g.getName();
            response["groupDesc"] = g.getDesc();
            nlohmann::json membersArr = nlohmann::json::array();
            for (auto& gu : g.getUsers()) {
                nlohmann::json m;
                m["id"] = gu.getId();
                m["name"] = gu.getName();
                m["role"] = gu.getRole();
                m["state"] = gu.getState();
                membersArr.push_back(m);
            }
            response["members"] = membersArr;
            break;
        }
    }
    sendFramed(conn, response.dump());
}

void ChatService::groupChat(const muduo::net::TcpConnectionPtr& conn,
                             nlohmann::json& js, muduo::Timestamp) {
    int userId = js["id"];
    int groupId = js["groupId"];
    std::vector<int> userVec = _groupModel.queryGroupUsers(userId, groupId);

    {
        std::lock_guard<std::mutex> lock(connMutex);
        for (int& user_id : userVec) {
            auto iter = _userConnMap.find(user_id);
            if (iter != _userConnMap.end()) {
                sendFramed(iter->second, js.dump());
                continue;
            }
            User user = _userModel.queryById(user_id);
            if (user.getState() == "online") {
                _redis.publish(user_id, js.dump());
            } else {
                _offlineMsgModel.insert(user_id, js.dump());
            }
        }
    }
}
