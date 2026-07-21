#include "network/ProtocolClient.h"
#include "network/FramedTcpClient.h"

#include <QJsonDocument>
#include <QJsonArray>

ProtocolClient::ProtocolClient(QObject* parent)
    : QObject(parent)
    , m_tcp(new FramedTcpClient(this))
{
    connect(m_tcp, &FramedTcpClient::frameReceived,
            this, &ProtocolClient::onFrameReceived);
    connect(m_tcp, &FramedTcpClient::connected,
            this, &ProtocolClient::onConnected);
    connect(m_tcp, &FramedTcpClient::disconnected,
            this, &ProtocolClient::onDisconnected);
    connect(m_tcp, &FramedTcpClient::errorOccurred,
            this, &ProtocolClient::onSocketError);
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(30000);
    connect(m_heartbeatTimer, &QTimer::timeout,this, &ProtocolClient::sendHeartbeat);
}

ProtocolClient::~ProtocolClient()
{
}

// ---- Connection ----

void ProtocolClient::connectToServer(const QString& host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_tcp->connectToHost(host, port);
}

void ProtocolClient::disconnectFromServer()
{
    m_tcp->disconnectFromHost();
}

bool ProtocolClient::isConnected() const
{
    return m_tcp->isConnected();
}

// ---- Outgoing messages ----

void ProtocolClient::sendLogin(int userId, const QString& password)
{
    m_userId   = userId;
    m_password = password;

    QJsonObject js;
    js["msgId"] = MessageType::LOGIN_MSG;
    js["id"]    = userId;
    js["password"] = password;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::sendRegister(const QString& name, const QString& password)
{
    QJsonObject js;
    js["msgId"]    = MessageType::REG_MSG;
    js["name"]     = name;
    js["password"] = password;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::sendPrivateMessage(int toId, const QString& message)
{
    QJsonObject js;
    js["msgId"]  = MessageType::ONE_CHAT_MSG;
    js["id"]     = m_userId;
    js["from"]   = m_userName;
    js["toid"]   = toId;
    js["message"] = message;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::sendGroupMessage(int groupId, const QString& message)
{
    QJsonObject js;
    js["msgId"]   = MessageType::GROUP_CHAT_MSG;
    js["id"]      = m_userId;
    js["from"]    = m_userName;
    js["groupId"] = groupId;
    js["message"] = message;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::sendAddFriend(int friendId)
{
    QJsonObject js;
    js["msgId"]    = MessageType::ADD_FRIEND_MSG;
    js["id"]       = m_userId;
    js["friendId"] = friendId;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::sendCreateGroup(const QString& name, const QString& desc)
{
    QJsonObject js;
    js["msgId"]     = MessageType::CREATE_GROUP_MSG;
    js["id"]        = m_userId;
    js["groupname"] = name;
    js["groupdesc"] = desc;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::sendJoinGroup(int groupId)
{
    QJsonObject js;
    js["msgId"]   = MessageType::ADD_GROUP_MSG;
    js["id"]      = m_userId;
    js["groupId"] = groupId;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::sendLogout()
{
    if (m_userId == 0) return;

    QJsonObject js;
    js["msgId"] = MessageType::LOGIN_OUT_MSG;
    js["id"]    = m_userId;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
    stopHeartbeat();
}

// ---- Network callbacks ----

void ProtocolClient::onConnected()
{
    emit serverConnected();
}

void ProtocolClient::onDisconnected()
{
    stopHeartbeat();
    emit serverDisconnected();
}

void ProtocolClient::onSocketError(const QString& errorString)
{
    emit serverError(errorString);
}

// ---- Message dispatch ----

void ProtocolClient::onFrameReceived(const QByteArray& frame)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(frame, &err);
    if (err.error != QJsonParseError::NoError)
        return;

    QJsonObject json = doc.object();
    int msgId = json["msgId"].toInt();

    switch (msgId) {
    case MessageType::LOG_MSG_ACK:
        handleLoginAck(json);
        break;
    case MessageType::REG_MSG_ACK:
        handleRegisterAck(json);
        break;
    case MessageType::ONE_CHAT_MSG:
        handlePrivateMessage(json);
        break;
    case MessageType::GROUP_CHAT_MSG:
        handleGroupMessage(json);
        break;
    case MessageType::ADD_FRIEND_MSG_ACK:
        handleAddFriendAck(json);
        break;
    case MessageType::CREATE_GROUP_MSG_ACK:
        handleCreateGroupAck(json);
        break;
    case MessageType::ADD_GROUP_MSG_ACK:
        handleAddGroupAck(json);
        break;
    case MessageType::PONG_MSG:
        break;
    default:
        break;  // silently ignore unknown msgIds
    }
}

void ProtocolClient::handleLoginAck(const QJsonObject& json)
{
    int errnoCode = json["errno"].toInt();

    if (errnoCode != 0) {
        QString errMsg = json["errMessage"].toString("unknown error");
        m_userId = 0;
        emit loginResult(false, errnoCode, errMsg,
                         0, QString(), {}, {}, {});
        return;
    }

    m_userId   = json["id"].toInt();
    m_userName = json["name"].toString();

    // Parse offline messages
    m_offlineMessages.clear();
    QJsonArray offlineArr = json["offlineMsg"].toArray();
    for (const auto& val : offlineArr) {
        QJsonDocument subDoc = QJsonDocument::fromJson(
            val.toString().toUtf8());
        if (subDoc.isNull()) continue;
        QJsonObject sub = subDoc.object();
        ChatMessage msg;
        msg.msgId    = sub["msgId"].toInt();
        msg.fromId   = sub.contains("id") ? sub["id"].toInt() : 0;
        msg.fromName = sub["from"].toString();
        msg.message  = sub["message"].toString();
        msg.sendTime = sub["sendtime"].toString();
        msg.toId     = sub.contains("toid") ? sub["toid"].toInt() : 0;
        msg.groupId  = sub.contains("groupid") ? sub["groupid"].toInt() : 0;
        m_offlineMessages.append(msg);
    }

    // Parse friends
    m_friends.clear();
    QJsonArray friendsArr = json["friends"].toArray();
    for (const auto& val : friendsArr) {
        QJsonDocument subDoc = QJsonDocument::fromJson(
            val.toString().toUtf8());
        if (subDoc.isNull()) continue;
        QJsonObject sub = subDoc.object();
        FriendInfo f;
        f.id    = sub["id"].toInt();
        f.name  = sub["name"].toString();
        f.state = sub["state"].toString();
        m_friends.append(f);
    }

    // Parse groups
    m_groups.clear();
    QJsonArray groupsArr = json["groups"].toArray();
    for (const auto& val : groupsArr) {
        QJsonDocument subDoc = QJsonDocument::fromJson(
            val.toString().toUtf8());
        if (subDoc.isNull()) continue;
        QJsonObject sub = subDoc.object();
        GroupInfo g;
        g.id   = sub["id"].toInt();
        g.name = sub["name"].toString();
        g.desc = sub["desc"].toString();

        // Parse group members
        QJsonArray membersArr = sub["members"].toArray();
        for (const auto& mval : membersArr) {
            QJsonObject mobj = mval.toObject();
            GroupMember gm;
            gm.id    = mobj["id"].toInt();
            gm.name  = mobj["name"].toString();
            gm.role  = mobj["role"].toString();
            gm.state = mobj["state"].toString();
            g.members.append(gm);
        }
        m_groups.append(g);
    }

    startHeartbeat();
    emit loginResult(true, 0, QString(),
                     m_userId, m_userName,
                     m_offlineMessages, m_friends, m_groups);
}

void ProtocolClient::handleRegisterAck(const QJsonObject& json)
{
    int errnoCode = json["errno"].toInt();
    if (errnoCode != 0) {
        emit registerResult(false, 0);
    } else {
        int id = json["id"].toInt();
        emit registerResult(true, id);
    }
}

void ProtocolClient::handlePrivateMessage(const QJsonObject& json)
{
    ChatMessage msg;
    msg.msgId    = MessageType::ONE_CHAT_MSG;
    msg.fromId   = json["id"].toInt();
    msg.fromName = json["from"].toString();
    msg.message  = json["message"].toString();
    msg.sendTime = json["sendtime"].toString();
    msg.toId     = json["toid"].toInt();

    emit privateMessageReceived(msg);
}

void ProtocolClient::handleGroupMessage(const QJsonObject& json)
{
    ChatMessage msg;
    msg.msgId    = MessageType::GROUP_CHAT_MSG;
    msg.fromId   = json["id"].toInt();
    msg.fromName = json["from"].toString();
    msg.message  = json["message"].toString();
    msg.groupId  = json["groupId"].toInt();

    emit groupMessageReceived(msg);
}

void ProtocolClient::handleAddFriendAck(const QJsonObject& json)
{
    int errnoCode = json["errno"].toInt();
    if (errnoCode != 0) {
        emit addFriendResult(false, 0, QString(), QString());
    } else {
        int friendId = json["friendId"].toInt();
        QString name = json["friendName"].toString();
        QString state = json["friendState"].toString();

        // Update local state
        FriendInfo f;
        f.id = friendId;
        f.name = name;
        f.state = state;
        m_friends.append(f);

        emit addFriendResult(true, friendId, name, state);
    }
}

void ProtocolClient::handleCreateGroupAck(const QJsonObject& json)
{
    int errnoCode = json["errno"].toInt();
    if (errnoCode != 0) {
        emit createGroupResult(false, 0, QString(), QString());
    } else {
        int groupId = json["groupId"].toInt();
        QString name = json["groupName"].toString();
        QString desc = json["groupDesc"].toString();

        // Update local state
        GroupInfo g;
        g.id = groupId;
        g.name = name;
        g.desc = desc;
        QJsonArray membersArr = json["members"].toArray();
        for (const auto& mval : membersArr) {
            QJsonObject mobj = mval.toObject();
            GroupMember gm;
            gm.id    = mobj["id"].toInt();
            gm.name  = mobj["name"].toString();
            gm.role  = mobj["role"].toString();
            gm.state = mobj["state"].toString();
            g.members.append(gm);
        }
        m_groups.append(g);

        emit createGroupResult(true, groupId, name, desc);
    }
}

void ProtocolClient::handleAddGroupAck(const QJsonObject& json)
{
    int errnoCode = json["errno"].toInt();
    if (errnoCode != 0) {
        emit joinGroupResult(false, 0, QString(), QString());
    } else {
        int groupId = json["groupId"].toInt();
        QString name = json["groupName"].toString();
        QString desc = json["groupDesc"].toString();

        // Update local state
        GroupInfo g;
        g.id = groupId;
        g.name = name;
        g.desc = desc;
        QJsonArray membersArr = json["members"].toArray();
        for (const auto& mval : membersArr) {
            QJsonObject mobj = mval.toObject();
            GroupMember gm;
            gm.id    = mobj["id"].toInt();
            gm.name  = mobj["name"].toString();
            gm.role  = mobj["role"].toString();
            gm.state = mobj["state"].toString();
            g.members.append(gm);
        }
        m_groups.append(g);

        emit joinGroupResult(true, groupId, name, desc);
    }
}

void ProtocolClient::sendHeartbeat()
{
    if (m_userId <= 0 || !isConnected()) return;

    QJsonObject js;
    js["msgId"] = MessageType::PING_MSG;
    js["id"] = m_userId;

    QJsonDocument doc(js);
    m_tcp->sendFrame(doc.toJson(QJsonDocument::Compact));
}

void ProtocolClient::startHeartbeat()
{
    if (m_heartbeatTimer && !m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->start();
    }
}

void ProtocolClient::stopHeartbeat()
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
}
