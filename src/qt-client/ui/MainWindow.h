#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QTimer>
#include "models/ChatData.h"

class ProtocolClient;
class ChatWidget;
class ContactPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ProtocolClient* client, QWidget* parent = nullptr);

    /// Display offline messages in their corresponding chat tabs
    void showOfflineMessages();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Incoming messages
    void onPrivateMessageReceived(const ChatMessage& msg);
    void onGroupMessageReceived(const ChatMessage& msg);

    // Connection state
    void onServerDisconnected();
    void onServerConnected();
    void onReconnectTimer();

    // UI interactions (delegated from ContactPanel)
    void onFriendDoubleClicked(int friendId, const QString& name);
    void onGroupDoubleClicked(int groupId, const QString& name);
    void onTabCloseRequested(int index);
    void onTabCurrentChanged(int index);

    // Theme
    void onThemeToggled();

    // Dialog actions
    void onAddFriend();
    void onCreateGroup();
    void onJoinGroup();

    // Operation results (real-time UI updates)
    void onAddFriendResult(bool success, int friendId,
                           const QString& friendName, const QString& friendState);
    void onCreateGroupResult(bool success, int groupId,
                             const QString& groupName, const QString& groupDesc);
    void onJoinGroupResult(bool success, int groupId,
                           const QString& groupName, const QString& groupDesc);

private:
    ChatWidget* findOrCreatePrivateChat(int friendId, const QString& friendName);
    ChatWidget* findOrCreateGroupChat(int groupId, const QString& groupName);
    ChatWidget* findChatWidget(ChatType type, int targetId) const;
    void updateTabUnread(ChatWidget* cw);
    void refreshTrees();
    void restyle();

    ProtocolClient* m_client;

    // Left panel
    ContactPanel* m_contactPanel;

    // Right panel
    QTabWidget* m_tabWidget;

    // Status
    QLabel* m_statusLabel;

    // Reconnect
    QTimer* m_reconnectTimer;
    int     m_reconnectRetries = 0;
    static constexpr int MAX_RECONNECT_RETRIES = 5;
};

#endif // MAIN_WINDOW_H_
