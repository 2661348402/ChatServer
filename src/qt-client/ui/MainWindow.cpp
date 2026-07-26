#include "ui/MainWindow.h"
#include "ui/ChatWidget.h"
#include "ui/ContactPanel.h"
#include "ui/AddFriendDialog.h"
#include "ui/CreateGroupDialog.h"
#include "ui/JoinGroupDialog.h"
#include "Theme.h"
#include "network/ProtocolClient.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QStatusBar>

#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QLabel>

MainWindow::MainWindow(ProtocolClient *client, QWidget *parent)
    : QMainWindow(parent), m_client(client)
{
    setWindowTitle("ChatServer");
    resize(1295, 768);
    setMinimumSize(900, 560);

    // ---- Central widget ----
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ================================================================
    //  Left navigation bar
    // ================================================================
    auto *navBar = new QWidget(central);
    navBar->setFixedWidth(60);
    navBar->setStyleSheet(
        "QWidget { background: #EDEDED; }"
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  color: #666666;"
        "  border-radius: 6px;"
        "}"
        "QPushButton:hover { background: #DCDCDC; }"
        "QPushButton:disabled { color: #999999; }");

    auto *navLayout = new QVBoxLayout(navBar);
    navLayout->setContentsMargins(10, 14, 10, 14);
    navLayout->setSpacing(14);

    auto *avatar = new QLabel("C", navBar);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        "QLabel {"
        "  background: #07C160;"
        "  color: white;"
        "  border-radius: 6px;"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "}");
    navLayout->addWidget(avatar);

    auto addNavButton = [this, navBar, navLayout](QStyle::StandardPixmap icon,
                                                  const QString &tip)
    {
        auto *btn = new QPushButton(navBar);
        btn->setFixedSize(40, 40);
        btn->setIcon(style()->standardIcon(icon));
        btn->setIconSize(QSize(22, 22));
        btn->setToolTip(tip);
        navLayout->addWidget(btn);
        return btn;
    };

    auto *chatBtn = addNavButton(QStyle::SP_MessageBoxInformation, "聊天");
    chatBtn->setEnabled(false);

    auto *contactBtn = addNavButton(QStyle::SP_FileDialogListView, "联系人");
    contactBtn->setEnabled(false);

    auto *groupBtn = addNavButton(QStyle::SP_DirIcon, "群聊");
    groupBtn->setEnabled(false);

    navLayout->addStretch();

    auto *settingsBtn = addNavButton(QStyle::SP_ComputerIcon, "切换主题");
    connect(settingsBtn, &QPushButton::clicked,
            this, &MainWindow::onThemeToggled);

    rootLayout->addWidget(navBar);

    // ================================================================
    //  Middle panel — ContactPanel
    // ================================================================
    m_contactPanel = new ContactPanel(this);
    m_contactPanel->setFixedWidth(300);
    rootLayout->addWidget(m_contactPanel);

    // ================================================================
    //  Right panel — chat tabs
    // ================================================================
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(false);
    m_tabWidget->setDocumentMode(true);

    // 更像微信：隐藏上方 tab 栏
    m_tabWidget->tabBar()->hide();

    m_tabWidget->setStyleSheet(
        QString(
            "QTabWidget::pane {"
            "  border: none;"
            "  background: %1;"
            "}")
            .arg(Theme::bgChat()));

    rootLayout->addWidget(m_tabWidget, 1);

    // ---- Status bar ----
    m_statusLabel = new QLabel("Connected", this);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px;").arg(Theme::textSecondary()));
    statusBar()->addPermanentWidget(m_statusLabel);

    // ---- Populate trees ----
    refreshTrees();

    // ---- Signal wiring ----
    // ContactPanel → MainWindow
    connect(m_contactPanel, &ContactPanel::friendDoubleClicked,
            this, &MainWindow::onFriendDoubleClicked);
    connect(m_contactPanel, &ContactPanel::groupDoubleClicked,
            this, &MainWindow::onGroupDoubleClicked);
    connect(m_contactPanel, &ContactPanel::addFriendRequested,
            this, &MainWindow::onAddFriend);
    connect(m_contactPanel, &ContactPanel::createGroupRequested,
            this, &MainWindow::onCreateGroup);
    connect(m_contactPanel, &ContactPanel::joinGroupRequested,
            this, &MainWindow::onJoinGroup);
    connect(m_contactPanel, &ContactPanel::themeToggled,
            this, &MainWindow::onThemeToggled);

    // ProtocolClient signals
    connect(m_client, &ProtocolClient::privateMessageReceived,
            this, &MainWindow::onPrivateMessageReceived);
    connect(m_client, &ProtocolClient::groupMessageReceived,
            this, &MainWindow::onGroupMessageReceived);
    connect(m_client, &ProtocolClient::serverDisconnected,
            this, &MainWindow::onServerDisconnected);
    connect(m_client, &ProtocolClient::serverConnected,
            this, &MainWindow::onServerConnected);
    connect(m_client, &ProtocolClient::addFriendResult,
            this, &MainWindow::onAddFriendResult);
    connect(m_client, &ProtocolClient::createGroupResult,
            this, &MainWindow::onCreateGroupResult);
    connect(m_client, &ProtocolClient::joinGroupResult,
            this, &MainWindow::onJoinGroupResult);

    // Tab signals
    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::onTabCurrentChanged);

    // ---- Reconnect timer ----
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &MainWindow::onReconnectTimer);
}

// ====================================================================
//  Tree refresh (delegates to ContactPanel)
// ====================================================================

void MainWindow::refreshTrees()
{
    m_contactPanel->refreshFriendList(m_client->friends());
    m_contactPanel->refreshGroupList(m_client->groups());
}

// ====================================================================
//  Close event
// ====================================================================

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_client->sendLogout();
    QMainWindow::closeEvent(event);
}

// ---- Incoming messages ----

void MainWindow::onPrivateMessageReceived(const ChatMessage &msg)
{
    int peerId = (msg.fromId == m_client->userId()) ? msg.toId : msg.fromId;
    QString peerName = (msg.fromId == m_client->userId())
                           ? QString("User %1").arg(msg.toId)
                           : msg.fromName;

    ChatWidget *cw = findOrCreatePrivateChat(peerId, peerName);
    cw->appendMessage(msg);
    updateTabUnread(cw);
}

void MainWindow::onGroupMessageReceived(const ChatMessage &msg)
{
    QString groupName = QString("Group %1").arg(msg.groupId);
    for (const auto &g : m_client->groups())
    {
        if (g.id == msg.groupId)
        {
            groupName = g.name;
            break;
        }
    }

    ChatWidget *cw = findOrCreateGroupChat(msg.groupId, groupName);
    cw->appendMessage(msg);
    updateTabUnread(cw);
}

// ---- Connection state ----

void MainWindow::onServerDisconnected()
{
    m_statusLabel->setText("Disconnected — reconnecting...");
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px;").arg(Theme::textError()));
    m_reconnectRetries = 0;
    m_reconnectTimer->start();
}

void MainWindow::onServerConnected()
{
    m_reconnectTimer->stop();
    m_statusLabel->setText("Connected");
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px;").arg(Theme::textSecondary()));
    m_reconnectRetries = 0;
    refreshTrees();
}

void MainWindow::onReconnectTimer()
{
    ++m_reconnectRetries;
    if (m_reconnectRetries > MAX_RECONNECT_RETRIES)
    {
        m_reconnectTimer->stop();
        m_statusLabel->setText("Connection lost (max retries exceeded)");
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px;").arg(Theme::textError()));
        return;
    }
    m_statusLabel->setText(
        QString("Reconnecting (%1/%2)...")
            .arg(m_reconnectRetries)
            .arg(MAX_RECONNECT_RETRIES));
    m_client->connectToServer(m_client->host(), m_client->port());
}

// ---- UI interactions ----

void MainWindow::onFriendDoubleClicked(int friendId, const QString &name)
{
    findOrCreatePrivateChat(friendId, name);
}

void MainWindow::onGroupDoubleClicked(int groupId, const QString &name)
{
    findOrCreateGroupChat(groupId, name);
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *w = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    w->deleteLater();
}

void MainWindow::onTabCurrentChanged(int index)
{
    QWidget *w = m_tabWidget->widget(index);
    ChatWidget *cw = qobject_cast<ChatWidget *>(w);
    if (cw && cw->hasUnread())
    {
        cw->clearUnread();
        m_tabWidget->setTabText(index, cw->targetName());
    }
}

// ---- Dialog actions ----

void MainWindow::onAddFriend()
{
    AddFriendDialog dlg(m_client, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_statusLabel->setText("Processing...");
    }
}

void MainWindow::onCreateGroup()
{
    CreateGroupDialog dlg(m_client, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_statusLabel->setText("Processing...");
    }
}

void MainWindow::onJoinGroup()
{
    JoinGroupDialog dlg(m_client, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_statusLabel->setText("Processing...");
    }
}

// ---- Operation results ----

void MainWindow::onAddFriendResult(bool success, int friendId,
                                   const QString &friendName,
                                   const QString &friendState)
{
    if (!success)
    {
        m_statusLabel->setText("Failed to add friend");
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px;").arg(Theme::textError()));
        return;
    }
    m_statusLabel->setText(
        QString("Friend added: %1").arg(friendName));
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px;").arg(Theme::green()));
    refreshTrees();
}

void MainWindow::onCreateGroupResult(bool success, int groupId,
                                     const QString &groupName,
                                     const QString &groupDesc)
{
    if (!success)
    {
        m_statusLabel->setText("Failed to create group");
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px;").arg(Theme::textError()));
        return;
    }
    m_statusLabel->setText(
        QString("Group created: %1").arg(groupName));
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px;").arg(Theme::green()));
    refreshTrees();
}

void MainWindow::onJoinGroupResult(bool success, int groupId,
                                   const QString &groupName,
                                   const QString &groupDesc)
{
    if (!success)
    {
        m_statusLabel->setText("Failed to join group");
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px;").arg(Theme::textError()));
        return;
    }
    m_statusLabel->setText(
        QString("Joined group: %1").arg(groupName));
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px;").arg(Theme::green()));
    refreshTrees();
}

// ---- Tab management ----

ChatWidget *MainWindow::findChatWidget(ChatType type, int targetId) const
{
    for (int i = 0; i < m_tabWidget->count(); ++i)
    {
        ChatWidget *cw = qobject_cast<ChatWidget *>(m_tabWidget->widget(i));
        if (cw && cw->chatType() == type && cw->targetId() == targetId)
            return cw;
    }
    return nullptr;
}

ChatWidget *MainWindow::findOrCreatePrivateChat(int friendId,
                                                const QString &friendName)
{
    ChatWidget *cw = findChatWidget(ChatType::Private, friendId);
    if (cw)
        return cw;

    cw = new ChatWidget(ChatType::Private, friendId, friendName,
                        m_client, this);
    int idx = m_tabWidget->addTab(cw, friendName);
    m_tabWidget->setCurrentIndex(idx);
    connect(cw, &ChatWidget::unreadChanged, this, [this, cw]()
            { updateTabUnread(cw); });
    return cw;
}

ChatWidget *MainWindow::findOrCreateGroupChat(int groupId,
                                              const QString &groupName)
{
    ChatWidget *cw = findChatWidget(ChatType::Group, groupId);
    if (cw)
        return cw;

    cw = new ChatWidget(ChatType::Group, groupId, groupName,
                        m_client, this);

    for (const auto &g : m_client->groups())
    {
        if (g.id == groupId)
        {
            cw->setGroupInfo(g.desc, g.members);
            break;
        }
    }

    int idx = m_tabWidget->addTab(cw, groupName);
    m_tabWidget->setCurrentIndex(idx);
    connect(cw, &ChatWidget::unreadChanged, this, [this, cw]()
            { updateTabUnread(cw); });
    return cw;
}

void MainWindow::showOfflineMessages()
{
    const auto &msgs = m_client->offlineMessages();
    if (msgs.isEmpty())
        return;

    for (const auto &msg : msgs)
    {
        if (msg.msgId == MessageType::ONE_CHAT_MSG || msg.toId != 0)
        {
            int peerId = (msg.fromId == m_client->userId()) ? msg.toId : msg.fromId;
            QString peerName = (msg.fromId == m_client->userId())
                                   ? QString("User %1").arg(msg.toId)
                                   : msg.fromName;
            ChatWidget *cw = findOrCreatePrivateChat(peerId, peerName);
            cw->appendMessage(msg);
            updateTabUnread(cw);
        }
        else if (msg.msgId == MessageType::GROUP_CHAT_MSG || msg.groupId != 0)
        {
            QString groupName = QString("Group %1").arg(msg.groupId);
            for (const auto &g : m_client->groups())
            {
                if (g.id == msg.groupId)
                {
                    groupName = g.name;
                    break;
                }
            }
            ChatWidget *cw = findOrCreateGroupChat(msg.groupId, groupName);
            cw->appendMessage(msg);
            updateTabUnread(cw);
        }
    }

    m_statusLabel->setText(
        QString("Connected — %1 offline messages loaded").arg(msgs.size()));
}

void MainWindow::updateTabUnread(ChatWidget *cw)
{
    if (!cw->hasUnread())
        return;
    for (int i = 0; i < m_tabWidget->count(); ++i)
    {
        if (m_tabWidget->widget(i) == static_cast<QWidget *>(cw))
        {
            if (m_tabWidget->currentIndex() != i)
            {
                m_tabWidget->setTabText(i, "● " + cw->targetName());
            }
            break;
        }
    }
}

// ====================================================================
//  Theme toggle
// ====================================================================

void MainWindow::onThemeToggled()
{
    Theme::toggle();
    restyle();
}

void MainWindow::restyle()
{
    // Global app stylesheet (scrollbars + font)
    QString fontCss = QStringLiteral("font-family: sans-serif;");
    qApp->setStyleSheet(
        QString(
            "QWidget { %1 }"
            "QScrollBar:vertical {"
            "  background: transparent; width: 6px; margin: 0;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: %2; border-radius: 3px; min-height: 30px;"
            "}"
            "QScrollBar::handle:vertical:hover { background: %3; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "  height: 0; border: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "  background: transparent;"
            "}")
            .arg(fontCss, Theme::scrollHandle(), Theme::scrollHandleHover()));

    // Tab widget
    m_tabWidget->setStyleSheet(
        QString("QTabWidget::pane { border: 1px solid %1; background: white; }"
                "QTabBar::tab { padding: 8px 16px; border: 1px solid %1; "
                "  border-bottom: none; background: %2; margin-right: 2px; }"
                "QTabBar::tab:selected { background: white; border-bottom: 2px solid %3; }"
                "QTabBar::tab:hover { background: %4; }"
                "QTabBar::close-button { "
                "  subcontrol-position: right; padding: 2px; }"
                "QTabBar::close-button:hover { background: #FFCDD2; border-radius: 2px; }")
            .arg(Theme::borderChat(), Theme::bgTopBar(),
                 Theme::green(), Theme::hoverBg()));

    // Status label
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px;").arg(m_reconnectTimer->isActive() ? Theme::textError() : Theme::textSecondary()));

    // Contact panel
    m_contactPanel->restyle();

    // All open chat tabs
    for (int i = 0; i < m_tabWidget->count(); ++i)
    {
        ChatWidget *cw = qobject_cast<ChatWidget *>(m_tabWidget->widget(i));
        if (cw)
            cw->restyle();
    }

    // Refresh group tree items (custom widgets with inline styles)
    refreshTrees();
}
