#ifndef CHAT_WIDGET_H_
#define CHAT_WIDGET_H_

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "models/ChatData.h"

class ProtocolClient;
class BubbleWidget;

/// One chat tab — WeChat-style bubbles, input box, optional group header.
class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    ChatWidget(ChatType type, int targetId, const QString& targetName,
               ProtocolClient* client, QWidget* parent = nullptr);

    ChatType chatType()  const { return m_chatType; }
    int targetId()       const { return m_targetId; }
    QString targetName() const { return m_targetName; }

    /// Set group details (desc + members) — only for Group chat type
    void setGroupInfo(const QString& desc, const QVector<GroupMember>& members);

    void appendMessage(const ChatMessage& msg);
    void appendSystemMessage(const QString& text);
    void clearUnread();
    bool hasUnread() const { return m_hasUnread; }

    /// Flash a brief status message in the input area (e.g. "Sent ✓")
    void flashStatus(const QString& text, int durationMs = 1500);

    /// Re-apply all stylesheets after theme change
    void restyle();

signals:
    void unreadChanged();

private slots:
    void onSendClicked();

private:
    void scrollToBottom();

    ChatType  m_chatType;
    int       m_targetId;
    QString   m_targetName;
    ProtocolClient* m_client;

    QLineEdit*   m_input;
    QPushButton* m_sendBtn;

    // Bubble display area
    QScrollArea* m_scrollArea;
    QWidget*     m_bubbleContainer;
    QVBoxLayout* m_bubbleLayout;

    // Group info header (only visible for Group type)
    QWidget*     m_groupHeader = nullptr;
    QLabel*      m_groupInfoLabel = nullptr;
    QListWidget* m_groupMemberList = nullptr;

    // Inline status feedback (e.g. "Sent ✓")
    QLabel*      m_statusLabel = nullptr;

    bool m_hasUnread = false;
};

#endif // CHAT_WIDGET_H_
