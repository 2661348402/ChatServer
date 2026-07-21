#ifndef CHAT_DATA_H_
#define CHAT_DATA_H_

#include <QString>
#include <QVector>

enum class ChatType { Private, Group };

// Protocol message type constants — must match EnMsgType in include/public.hpp
namespace MessageType {
    constexpr int LOGIN_MSG       = 1;
    constexpr int REG_MSG         = 2;
    constexpr int REG_MSG_ACK     = 3;
    constexpr int LOG_MSG_ACK     = 4;
    constexpr int ONE_CHAT_MSG    = 5;
    constexpr int ADD_FRIEND_MSG  = 6;
    constexpr int CREATE_GROUP_MSG = 7;
    constexpr int ADD_GROUP_MSG   = 8;
    constexpr int GROUP_CHAT_MSG  = 9;
    constexpr int LOGIN_OUT_MSG   = 10;
    constexpr int ADD_FRIEND_MSG_ACK    = 11;
    constexpr int CREATE_GROUP_MSG_ACK  = 12;
    constexpr int ADD_GROUP_MSG_ACK     = 13;
    constexpr int PING_MSG = 14;
    constexpr int PONG_MSG = 15;    
}

struct FriendInfo {
    int id = 0;
    QString name;
    QString state;   // "online" or "offline"
};

struct GroupMember {
    int id = 0;
    QString name;
    QString role;   // "creator" or "normal"
    QString state;  // "online" or "offline"
};

struct GroupInfo {
    int id = 0;
    QString name;
    QString desc;
    QVector<GroupMember> members;
};

struct ChatMessage {
    int msgId = 0;
    int fromId = 0;
    QString fromName;
    QString message;
    QString sendTime;
    int toId = 0;        // for private chat
    int groupId = 0;     // for group chat
};

#endif // CHAT_DATA_H_
