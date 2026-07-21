#ifndef PROTOCOL_CLIENT_H_
#define PROTOCOL_CLIENT_H_

#include <QObject>
#include <QJsonObject>
#include <QVector>
#include "models/ChatData.h"
#include <QTimer>

class FramedTcpClient;

/// Mid-level protocol client: JSON encoding/decoding + msgId dispatch + user state.
/// Sits between the low-level FramedTcpClient and the UI layer.
class ProtocolClient : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolClient(QObject* parent = nullptr);
    ~ProtocolClient() override;

    // Connection
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    // Outgoing messages (build JSON → send frame)
    void sendLogin(int userId, const QString& password);
    void sendRegister(const QString& name, const QString& password);
    void sendPrivateMessage(int toId, const QString& message);
    void sendGroupMessage(int groupId, const QString& message);
    void sendAddFriend(int friendId);
    void sendCreateGroup(const QString& name, const QString& desc);
    void sendJoinGroup(int groupId);
    void sendLogout();
    void sendHeartbeat();
    void startHeartbeat();
    void stopHeartbeat();
    QTimer* m_heartbeatTimer = nullptr;

    // State accessors (populated after successful login)
    int  userId()   const { return m_userId; }
    QString userName() const { return m_userName; }
    const QVector<FriendInfo>& friends() const { return m_friends; }
    const QVector<GroupInfo>&  groups()  const { return m_groups; }
    const QVector<ChatMessage>& offlineMessages() const { return m_offlineMessages; }

    // Connection info (for reconnect)
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

signals:
    void serverConnected();
    void serverDisconnected();
    void serverError(const QString& errorString);

    void loginResult(bool success, int errnoCode, const QString& errMsg,
                     int userId, const QString& userName,
                     const QVector<ChatMessage>& offlineMessages,
                     const QVector<FriendInfo>& friends,
                     const QVector<GroupInfo>& groups);
    void registerResult(bool success, int assignedId);

    void privateMessageReceived(const ChatMessage& msg);
    void groupMessageReceived(const ChatMessage& msg);

    void addFriendResult(bool success, int friendId,
                         const QString& friendName, const QString& friendState);
    void createGroupResult(bool success, int groupId,
                           const QString& groupName, const QString& groupDesc);
    void joinGroupResult(bool success, int groupId,
                         const QString& groupName, const QString& groupDesc);

private slots:
    void onFrameReceived(const QByteArray& frame);
    void onConnected();
    void onDisconnected();
    void onSocketError(const QString& errorString);

private:
    void handleLoginAck(const QJsonObject& json);
    void handleRegisterAck(const QJsonObject& json);
    void handlePrivateMessage(const QJsonObject& json);
    void handleGroupMessage(const QJsonObject& json);
    void handleAddFriendAck(const QJsonObject& json);
    void handleCreateGroupAck(const QJsonObject& json);
    void handleAddGroupAck(const QJsonObject& json);

    FramedTcpClient* m_tcp;

    // Connection info (stored for reconnect)
    QString m_host;
    quint16 m_port = 0;

    // User state (populated on successful login)
    int  m_userId   = 0;
    QString m_userName;
    QString m_password;         // stored for auto re-login after reconnect
    QVector<FriendInfo> m_friends;
    QVector<GroupInfo>  m_groups;
    QVector<ChatMessage> m_offlineMessages;
};

#endif // PROTOCOL_CLIENT_H_
