#include "ui/ChatWidget.h"
#include "ui/BubbleWidget.h"
#include "Theme.h"
#include "network/ProtocolClient.h"

#include <QScrollBar>
#include <QDateTime>
#include <QTimer>
#include <QFrame>

ChatWidget::ChatWidget(ChatType type, int targetId, const QString& targetName,
                       ProtocolClient* client, QWidget* parent)
    : QWidget(parent)
    , m_chatType(type)
    , m_targetId(targetId)
    , m_targetName(targetName)
    , m_client(client)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Group info header (only for group chats) ----
    if (type == ChatType::Group) {
        m_groupHeader = new QWidget(this);
        auto* headerLayout = new QVBoxLayout(m_groupHeader);
        headerLayout->setContentsMargins(8, 4, 8, 4);

        m_groupInfoLabel = new QLabel(this);
        m_groupInfoLabel->setWordWrap(true);
        m_groupInfoLabel->setStyleSheet(
            QString("background:%1; padding:6px; border-radius:4px; "
            "font-size:12px; color:%2;").arg(Theme::groupHeaderBg(), Theme::textPrimary()));
        headerLayout->addWidget(m_groupInfoLabel);

        m_groupMemberList = new QListWidget(this);
        m_groupMemberList->setMaximumHeight(80);
        m_groupMemberList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_groupMemberList->setStyleSheet(
            QString("QListWidget { background:%1; font-size:11px; "
            "border:1px solid %2; }"
            "QListWidget::item { padding:2px 4px; }")
                .arg(Theme::bgInput(), Theme::borderCard()));
        headerLayout->addWidget(m_groupMemberList);

        mainLayout->addWidget(m_groupHeader);
    }

    // ---- Bubble display area ----
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; }").arg(Theme::bgChat()));

    m_bubbleContainer = new QWidget();
    m_bubbleContainer->setStyleSheet("background: transparent;");
    m_bubbleLayout = new QVBoxLayout(m_bubbleContainer);
    m_bubbleLayout->setContentsMargins(0, 8, 0, 8);
    m_bubbleLayout->setSpacing(2);
    m_bubbleLayout->addStretch();   // pushes bubbles upward

    m_scrollArea->setWidget(m_bubbleContainer);
    mainLayout->addWidget(m_scrollArea, 1);   // stretch 1

    // ---- Input area ----
    auto* inputWidget = new QWidget(this);
    inputWidget->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;")
            .arg(Theme::bgTopBar(), Theme::borderInput()));
    auto* inputOuter = new QVBoxLayout(inputWidget);
    inputOuter->setContentsMargins(8, 6, 8, 2);
    inputOuter->setSpacing(0);

    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("Type a message...");
    m_input->setStyleSheet(
        QString("QLineEdit {"
        "  background: white; border: 1px solid %1; border-radius: 4px;"
        "  padding: 8px; font-size: 13px;"
        "}"
        "QLineEdit:focus { border-color: %2; }")
            .arg(Theme::borderInput(), Theme::green()));
    inputRow->addWidget(m_input);

    m_sendBtn = new QPushButton("Send", this);
    m_sendBtn->setStyleSheet(
        QString("QPushButton {"
        "  background: %1; color: white; border: none; border-radius: 4px;"
        "  padding: 8px 20px; font-size: 13px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %3; }")
            .arg(Theme::green(), Theme::greenHover(), Theme::greenPressed()));
    inputRow->addWidget(m_sendBtn);
    inputOuter->addLayout(inputRow);

    // Inline status label (hidden by default, shown for "Sent ✓" etc.)
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; padding: 0 4px 2px 4px; "
        "border: none; background: transparent;").arg(Theme::textLight()));
    m_statusLabel->hide();
    inputOuter->addWidget(m_statusLabel);

    mainLayout->addWidget(inputWidget);

    connect(m_sendBtn, &QPushButton::clicked,
            this, &ChatWidget::onSendClicked);
    connect(m_input, &QLineEdit::returnPressed,
            this, &ChatWidget::onSendClicked);
}

void ChatWidget::setGroupInfo(const QString& desc,
                               const QVector<GroupMember>& members)
{
    if (m_chatType != ChatType::Group) return;

    m_groupInfoLabel->setText(
        QString("<b>Group ID:</b> %1 &nbsp;|&nbsp; <b>Description:</b> %2")
            .arg(m_targetId).arg(desc.toHtmlEscaped()));

    m_groupMemberList->clear();
    for (const auto& m : members) {
        QString stateIcon = (m.state == "online") ? "🟢" : "⚫";
        QString roleTag = (m.role == "creator") ? " [Creator]" : "";
        auto* item = new QListWidgetItem(
            QString("%1 %2%3 (ID:%4)")
                .arg(stateIcon, m.name, roleTag)
                .arg(m.id));
        item->setForeground(
            m.state == "online" ? QColor(Theme::green()) : QColor(Theme::textMuted()));
        m_groupMemberList->addItem(item);
    }
}

void ChatWidget::appendMessage(const ChatMessage& msg)
{
    bool isSelf = (msg.fromId == m_client->userId());

    // Determine which name to show above the bubble
    QString displayName;
    bool showName = false;
    if (m_chatType == ChatType::Group && !isSelf) {
        displayName = msg.fromName;
        showName = true;
    } else if (isSelf) {
        displayName = m_client->userName();
    }

    // Timestamp
    QString timestamp = msg.sendTime;
    if (timestamp.isEmpty())
        timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

    // Insert bubble before the trailing stretch
    auto* bubble = new BubbleWidget(m_bubbleContainer);
    bubble->setMessage(displayName, msg.message, timestamp, isSelf, showName);
    m_bubbleLayout->insertWidget(m_bubbleLayout->count() - 1, bubble);

    scrollToBottom();

    // Track unread if not visible
    if (!isVisible() && !m_hasUnread) {
        m_hasUnread = true;
        emit unreadChanged();
    }
}

void ChatWidget::appendSystemMessage(const QString& text)
{
    auto* label = new QLabel(text, m_bubbleContainer);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        QString("color:%1; font-size:11px; padding:4px 0;").arg(Theme::textLight()));
    label->setWordWrap(true);
    // Insert before the trailing stretch
    m_bubbleLayout->insertWidget(m_bubbleLayout->count() - 1, label);

    scrollToBottom();
}

void ChatWidget::clearUnread()
{
    m_hasUnread = false;
}

void ChatWidget::onSendClicked()
{
    QString msg = m_input->text().trimmed();
    if (msg.isEmpty()) return;

    if (m_chatType == ChatType::Private) {
        m_client->sendPrivateMessage(m_targetId, msg);
    } else {
        m_client->sendGroupMessage(m_targetId, msg);
    }

    // Echo own message locally
    ChatMessage self;
    self.msgId    = (m_chatType == ChatType::Private)
                    ? MessageType::ONE_CHAT_MSG : MessageType::GROUP_CHAT_MSG;
    self.fromId   = m_client->userId();
    self.fromName = m_client->userName();
    self.message  = msg;
    self.sendTime = QDateTime::currentDateTime().toString("hh:mm:ss");
    appendMessage(self);

    m_input->clear();
    flashStatus("Sent ✓");
}

void ChatWidget::flashStatus(const QString& text, int durationMs)
{
    m_statusLabel->setText(text);
    m_statusLabel->show();
    QTimer::singleShot(durationMs, this, [this]() {
        m_statusLabel->hide();
    });
}

void ChatWidget::scrollToBottom()
{
    QTimer::singleShot(20, this, [this]() {
        QScrollBar* sb = m_scrollArea->verticalScrollBar();
        sb->setValue(sb->maximum());
    });
}

void ChatWidget::restyle()
{
    // Group header (only for group chats)
    if (m_chatType == ChatType::Group && m_groupHeader) {
        m_groupInfoLabel->setStyleSheet(
            QString("background:%1; padding:6px; border-radius:4px; "
            "font-size:12px; color:%2;").arg(Theme::groupHeaderBg(), Theme::textPrimary()));
        m_groupMemberList->setStyleSheet(
            QString("QListWidget { background:%1; font-size:11px; "
            "border:1px solid %2; }"
            "QListWidget::item { padding:2px 4px; }")
                .arg(Theme::bgInput(), Theme::borderCard()));
    }

    // Scroll area (chat background)
    m_scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; }").arg(Theme::bgChat()));

    // Input widget (the container)
    QWidget* inputWidget = m_input->parentWidget()->parentWidget() ?
        qobject_cast<QWidget*>(m_input->parentWidget()->parentWidget()) : nullptr;
    // Actually, inputWidget is the direct parent layout's parent widget
    // Let's just re-style input and sendBtn directly

    // Input field
    m_input->setStyleSheet(
        QString("QLineEdit {"
        "  background: white; border: 1px solid %1; border-radius: 4px;"
        "  padding: 8px; font-size: 13px;"
        "}"
        "QLineEdit:focus { border-color: %2; }")
            .arg(Theme::borderInput(), Theme::green()));

    // Send button
    m_sendBtn->setStyleSheet(
        QString("QPushButton {"
        "  background: %1; color: white; border: none; border-radius: 4px;"
        "  padding: 8px 20px; font-size: 13px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %3; }")
            .arg(Theme::green(), Theme::greenHover(), Theme::greenPressed()));

    // Status label
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; padding: 0 4px 2px 4px; "
        "border: none; background: transparent;").arg(Theme::textLight()));

    // Input area background (traverse up to the container widget)
    QWidget* inputArea = m_input->parentWidget();
    if (inputArea) {
        inputArea->setStyleSheet(
            QString("background:%1; border-top:1px solid %2;")
                .arg(Theme::bgTopBar(), Theme::borderInput()));
    }

    // Restyle all existing bubble widgets in the chat area
    for (BubbleWidget* bw : m_bubbleContainer->findChildren<BubbleWidget*>()) {
        bw->restyle();
    }

    // Restyle system message labels
    for (QLabel* label : m_bubbleContainer->findChildren<QLabel*>()) {
        // Only restyle system-message labels (centered, not inside a BubbleWidget)
        if (label->alignment() == Qt::AlignCenter && !label->parentWidget()->inherits("BubbleWidget")) {
            label->setStyleSheet(
                QString("color:%1; font-size:11px; padding:4px 0;").arg(Theme::textLight()));
        }
    }
}
