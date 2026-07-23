#include "public.hpp"
#include "ChatService.hpp"
#include "Group.hpp"
#include "SHA256.hpp"
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <vector>
#include <string>
#include <arpa/inet.h>
#include "Metrics.hpp"

using namespace std::placeholders;

ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

void ChatService::sendFramed(const muduo::net::TcpConnectionPtr &conn,
                             const std::string &msg)
{
    uint32_t netLen = htonl(msg.size());
    std::string packet(reinterpret_cast<const char *>(&netLen), 4);
    packet.append(msg);
    conn->send(packet);
}

void ChatService::login(const muduo::net::TcpConnectionPtr &conn,
                        nlohmann::json &js, muduo::Timestamp)
{
    LOG_INFO << "do login thing";
    int id = js["id"];
    std::string pwd = js["password"];
    User user = _userModel.queryById(id);

    if (user.getId() == -1)
    {
        nlohmann::json response;
        response["msgId"] = LOG_MSG_ACK;
        response["errno"] = 1;
        response["errMessage"] = "user not exist";
        sendFramed(conn, response.dump());
    }
    else if (user.getPwd() != SHA256::hash(pwd))
    {
        nlohmann::json response;
        response["msgId"] = LOG_MSG_ACK;
        response["errno"] = 2;
        response["errMessage"] = "password error";
        sendFramed(conn, response.dump());
    }
    else
    {
        if (user.getState() == "online")
        {
            nlohmann::json response;
            response["msgId"] = LOG_MSG_ACK;
            response["errno"] = 3;
            response["errMessage"] = "already online";
            sendFramed(conn, response.dump());
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(connMutex);
                _userConnMap[user.getId()] = conn;
                _lastActiveMap[user.getId()] = std::time(nullptr);
                Metrics::instance().setOnlineUsers(_userConnMap.size());
            }

            _redis.subscribe(id);

            user.setState("online");
            if (!_userModel.updateState(user))
            {
                LOG_ERROR << "update state failed";
            }

            nlohmann::json response;
            response["msgId"] = LOG_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            std::vector<std::string> messages = _offlineMsgModel.query(user.getId());
            if (!messages.empty())
            {
                response["offlineMsg"] = messages;
                _offlineMsgModel.remove(user.getId());
            }

            std::vector<Group> groupVec = _groupModel.queryGroups(id);
            std::vector<std::string> groups;
            for (auto &group : groupVec)
            {
                nlohmann::json js;
                js["id"] = group.getId();
                js["name"] = group.getName();
                js["desc"] = group.getDesc();
                // Serialize group members
                nlohmann::json membersArr = nlohmann::json::array();
                for (auto &gu : group.getUsers())
                {
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
            for (auto &u : userVec)
            {
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

void ChatService::reg(const muduo::net::TcpConnectionPtr &conn,
                      nlohmann::json &js, muduo::Timestamp)
{
    LOG_INFO << "do reg thing";
    std::string name = js["name"];
    std::string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);

    bool state = _userModel.insert(user);
    if (state)
    {
        nlohmann::json response;
        response["msgId"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        sendFramed(conn, response.dump());
    }
    else
    {
        nlohmann::json response;
        response["msgId"] = REG_MSG_ACK;
        response["errno"] = 1;
        sendFramed(conn, response.dump());
    }
}

void ChatService::oneChat(const muduo::net::TcpConnectionPtr &conn,
                          nlohmann::json &js, muduo::Timestamp receiveTime)
{
    int toid = js["toid"];
    js["sendtime"] = receiveTime.toFormattedString();
    std::string msg = js.dump();

    {
        std::lock_guard<std::mutex> lock(connMutex);
        auto iter = _userConnMap.find(toid);
        if (iter != _userConnMap.end())
        {
            sendFramed(iter->second, msg);
            return;
        }
    }

    User user = _userModel.queryById(toid);
    if (user.getState() == "online")
    {

        enqueueRedisPublish(toid, msg);
    }
    else
    {

        auto offlineBegin = std::chrono::steady_clock::now();
        _offlineMsgModel.insert(toid, msg);
        auto offlineEnd = std::chrono::steady_clock::now();
        Metrics::instance().recordOfflineStore(offlineEnd - offlineBegin, 1);
    }
}

MsgHandler ChatService::getHandler(int msgId)
{
    auto iter = msgHandlerMap.find(msgId);
    if (iter == msgHandlerMap.end())
    {
        return [=](const muduo::net::TcpConnectionPtr &conn,
                   nlohmann::json &js, muduo::Timestamp)
        {
            LOG_ERROR << "msgId: " << msgId << " can't find handler";
        };
    }
    return iter->second;
}

ChatService::ChatService()
{
    msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    msgHandlerMap.insert({LOGIN_OUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    msgHandlerMap.insert({PING_MSG, std::bind(&ChatService::heartbeat, this, _1, _2, _3)});

    if (_redis.connect())
    {
        _redis.init_notify_handler(
            std::bind(&ChatService::redisSubscribeMessage, this, _1, _2));
        _redisPublishThread = std::thread(&ChatService::redisPublishLoop, this);
    }
}

void ChatService::redisSubscribeMessage(int userid, std::string msg)
{
    {
        std::lock_guard<std::mutex> lock(connMutex);
        auto iter = _userConnMap.find(userid);
        if (iter != _userConnMap.end())
        {
            sendFramed(iter->second, msg);
            return;
        }
    }
    auto offlineBegin = std::chrono::steady_clock::now();
    _offlineMsgModel.insert(userid, msg);
    auto offlineEnd = std::chrono::steady_clock::now();
    Metrics::instance().recordOfflineStore(offlineEnd - offlineBegin, 1);
}

void ChatService::loginout(const muduo::net::TcpConnectionPtr &conn,
                           nlohmann::json &js, muduo::Timestamp)
{
    int id = js["id"];
    {
        std::lock_guard<std::mutex> lock(connMutex);
        _userConnMap.erase(id);
        _lastActiveMap.erase(id);
        Metrics::instance().setOnlineUsers(_userConnMap.size());
    }
    _redis.unsubscribe(id);
    User user;
    user.setId(id);
    user.setState("offline");
    _userModel.updateState(user);
    LOG_INFO << "id: " << id << " logged out";
}

void ChatService::clientConnectException(const muduo::net::TcpConnectionPtr &conn)
{
    int id = -1;
    {
        std::lock_guard<std::mutex> lock(connMutex);
        for (auto iter = _userConnMap.begin(); iter != _userConnMap.end(); ++iter)
        {
            if (iter->second == conn)
            {
                id = iter->first;
                _userConnMap.erase(iter);
                _lastActiveMap.erase(id);
                Metrics::instance().setOnlineUsers(_userConnMap.size());

                break;
            }
        }
    }
    if (id != -1)
    {
        setUserOffline(id, "disconnect");
    }
}

bool ChatService::reset()
{
    return _userModel.stateReset();
}

void ChatService::addFriend(const muduo::net::TcpConnectionPtr &conn,
                            nlohmann::json &js, muduo::Timestamp)
{
    int friendId = js["friendId"];
    int userId = js["id"];

    nlohmann::json response;
    response["msgId"] = ADD_FRIEND_MSG_ACK;

    if (_friendModel.insert(userId, friendId))
    {
        User friendUser = _userModel.queryById(friendId);
        response["errno"] = 0;
        response["friendId"] = friendId;
        response["friendName"] = friendUser.getName();
        response["friendState"] = friendUser.getState();
    }
    else
    {
        response["errno"] = 1;
        response["errMessage"] = "failed to add friend";
    }
    sendFramed(conn, response.dump());
}

void ChatService::createGroup(const muduo::net::TcpConnectionPtr &conn,
                              nlohmann::json &js, muduo::Timestamp)
{
    int userId = js["id"];
    std::string groupname = js["groupname"];
    std::string groupdesc = js["groupdesc"];
    Group group(-1, groupname, groupdesc);

    nlohmann::json response;
    response["msgId"] = CREATE_GROUP_MSG_ACK;

    if (_groupModel.createGroup(group))
    {
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
    }
    else
    {
        response["errno"] = 1;
        response["errMessage"] = "failed to create group";
    }
    sendFramed(conn, response.dump());
}

void ChatService::addGroup(const muduo::net::TcpConnectionPtr &conn,
                           nlohmann::json &js, muduo::Timestamp)
{
    int userId = js["id"];
    int groupId = js["groupId"];
    _groupModel.addGroup(userId, groupId, "normal");
    invalidateGroupUsersCache(groupId);
    nlohmann::json response;
    response["msgId"] = ADD_GROUP_MSG_ACK;
    response["errno"] = 0;
    response["groupId"] = groupId;

    // Query updated group info including members
    std::vector<Group> groups = _groupModel.queryGroups(userId);
    for (auto &g : groups)
    {
        if (g.getId() == groupId)
        {
            response["groupName"] = g.getName();
            response["groupDesc"] = g.getDesc();
            nlohmann::json membersArr = nlohmann::json::array();
            for (auto &gu : g.getUsers())
            {
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

void ChatService::groupChat(const muduo::net::TcpConnectionPtr &conn,
                            nlohmann::json &js, muduo::Timestamp)
{
    auto begin = std::chrono::steady_clock::now();
    int userId = js["id"];
    int groupId = js["groupId"];
    std::vector<int> offlineUsers;
    std::vector<int> userVec = getGroupUsersCached(groupId);
    std::vector<muduo::net::TcpConnectionPtr> localConns;
    std::vector<int> nonLocalUsers;

    {
        std::lock_guard<std::mutex> lock(connMutex);
        for (int uid : userVec)
        {
            if (uid == userId)
                continue;
            auto iter = _userConnMap.find(uid);
            if (iter != _userConnMap.end())
            {
                localConns.push_back(iter->second);
            }
            else
            {
                nonLocalUsers.push_back(uid);
            }
        }
    }

    auto localBegin = std::chrono::steady_clock::now();
    std::string msg = js.dump();
    for (auto &conn : localConns)
    {
        sendFramed(conn, msg);
    }
    auto localEnd = std::chrono::steady_clock::now();
    Metrics::instance().recordGroupLocalSend(localEnd - localBegin);

    auto states = _userModel.queryStatesByIds(nonLocalUsers);
    for (int uid : nonLocalUsers)
    {
        auto it = states.find(uid);
        bool online = it != states.end() && it->second == "online";

        if (online)
        {
            enqueueRedisPublish(uid, msg);
        }
        else
        {
            offlineUsers.push_back(uid);
        }
    }

    if (!offlineUsers.empty())
    {
        auto offlineBegin = std::chrono::steady_clock::now();
        _offlineMsgModel.insertBatch(offlineUsers, msg);
        auto offlineEnd = std::chrono::steady_clock::now();

        Metrics::instance().recordOfflineStore(
            offlineEnd - offlineBegin,
            offlineUsers.size());
    }
    auto end = std::chrono::steady_clock::now();
    Metrics::instance().recordGroupChat(end - begin);
}

void ChatService::heartbeat(const muduo::net::TcpConnectionPtr &conn,
                            nlohmann::json &js,
                            muduo::Timestamp)
{
    int id = js.value("id", 0);
    bool valid = false;

    {
        std::lock_guard<std::mutex> lock(connMutex);
        auto iter = _userConnMap.find(id);
        if (iter != _userConnMap.end() && iter->second == conn)
        {
            _lastActiveMap[id] = std::time(nullptr);
            valid = true;
        }
    }

    if (!valid)
    {
        conn->shutdown();
        return;
    }

    nlohmann::json response;
    response["msgId"] = PONG_MSG;
    response["id"] = id;
    sendFramed(conn, response.dump());
    LOG_INFO << "heartbeat received, userid=" << id;
}

void ChatService::checkHeartbeatTimeouts()
{
    std::vector<std::pair<int, muduo::net::TcpConnectionPtr>> expired;
    std::time_t now = std::time(nullptr);

    {
        std::lock_guard<std::mutex> lock(connMutex);

        for (auto iter = _lastActiveMap.begin(); iter != _lastActiveMap.end();)
        {
            int id = iter->first;

            if (now - iter->second > HEARTBEAT_TIMEOUT_SECONDS)
            {
                auto connIter = _userConnMap.find(id);
                if (connIter != _userConnMap.end())
                {
                    expired.push_back({id, connIter->second});
                    _userConnMap.erase(connIter);
                }
                iter = _lastActiveMap.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    if (!expired.empty())
    {
        Metrics::instance().setOnlineUsers(_userConnMap.size());
    }

    for (auto &item : expired)
    {
        int id = item.first;
        auto conn = item.second;
        setUserOffline(id, "heartbeat timeout");
        conn->getLoop()->runInLoop([conn]
                                   { conn->shutdown(); });
    }
}
void ChatService::setUserOffline(int id, const std::string &reason)
{
    if (!_redis.unsubscribe(id))
    {
        LOG_ERROR << "redis unsubscribe failed, userid=" << id;
    }

    User user;
    user.setId(id);
    user.setState("offline");
    _userModel.updateState(user);

    LOG_INFO << "userid=" << id << " offline, reason=" << reason;
}

std::vector<int> ChatService::getGroupUsersCached(int groupId)
{
    {
        std::shared_lock<std::shared_mutex> lock(_groupUsersCacheMutex);
        auto it = _groupUsersCache.find(groupId);
        if (it != _groupUsersCache.end())
        {
            return it->second;
        }
    }

    std::vector<int> users = _groupModel.queryGroupAllUsers(groupId);

    {
        std::unique_lock<std::shared_mutex> lock(_groupUsersCacheMutex);
        _groupUsersCache[groupId] = users;
    }

    return users;
}

void ChatService::invalidateGroupUsersCache(int groupId)
{
    std::unique_lock<std::shared_mutex> lock(_groupUsersCacheMutex);
    _groupUsersCache.erase(groupId);
}

ChatService::~ChatService()
{
    _redisPublishRunning = false;
    _redisPublishCv.notify_all();
    if (_redisPublishThread.joinable())
    {
        _redisPublishThread.join();
    }
}

void ChatService::enqueueRedisPublish(int userid, const std::string &message)
{
    {
        std::lock_guard<std::mutex> lock(_redisPublishMutex);
        _redisPublishQueue.push({userid, message});
    }
    _redisPublishCv.notify_one();
}

void ChatService::redisPublishLoop()
{
    while (_redisPublishRunning)
    {
        RedisPublishTask task;

        {
            std::unique_lock<std::mutex> lock(_redisPublishMutex);
            _redisPublishCv.wait(lock, [this]
                                 { return !_redisPublishRunning || !_redisPublishQueue.empty(); });

            if (!_redisPublishRunning && _redisPublishQueue.empty())
            {
                break;
            }

            task = std::move(_redisPublishQueue.front());
            _redisPublishQueue.pop();
        }
        auto redisBegin = std::chrono::steady_clock::now();
        bool ret = _redis.publish(task.userid, task.message);
        auto redisEnd = std::chrono::steady_clock::now();
        Metrics::instance().recordRedisPublish(redisEnd - redisBegin, ret);
        if (!ret)
        {
            LOG_ERROR << "async redis publish failed, degrade to offline message";
            Metrics::instance().incOfflineDegrade();
            auto offlineBegin = std::chrono::steady_clock::now();
            _offlineMsgModel.insert(task.userid, task.message);
            auto offlineEnd = std::chrono::steady_clock::now();
            Metrics::instance().recordOfflineStore(offlineEnd - offlineBegin, 1);
        }
    }
}