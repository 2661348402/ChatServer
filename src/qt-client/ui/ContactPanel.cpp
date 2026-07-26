#include "ui/ContactPanel.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>

ContactPanel::ContactPanel(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(300);
    setStyleSheet("background: #F7F7F7;");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ---- Top bar: search + plus button ----
    auto *topBar = new QWidget(this);
    topBar->setFixedHeight(64);
    topBar->setStyleSheet("background:#F7F7F7;");
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(14, 12, 10, 10);
    topLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(topBar);
    m_searchEdit->setPlaceholderText("搜索");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedHeight(34);
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background: #FFFFFF;"
        "  border: none;"
        "  border-radius: 5px;"
        "  padding: 0 10px;"
        "  font-size: 13px;"
        "  color: #333333;"
        "}"
        "QLineEdit:focus { background: #FFFFFF; }");
    topLayout->addWidget(m_searchEdit);

    m_plusBtn = new QPushButton("+", topBar);
    m_plusBtn->setFixedSize(32, 32);
    m_plusBtn->setStyleSheet(
        "QPushButton {"
        "  background: #E2E2E2;"
        "  border: none;"
        "  border-radius: 16px;"
        "  font-size: 20px;"
        "  color: #333333;"
        "}"
        "QPushButton:hover { background: #D0D0D0; }"
        "QPushButton:pressed { background: #C8C8C8; }");
    topLayout->addWidget(m_plusBtn);

    // Plus button menu
    m_plusMenu = new QMenu(this);
    auto *addFriendAct = m_plusMenu->addAction("➕  Add Friend");
    auto *joinGroupAct = m_plusMenu->addAction("👥  Join Group");
    auto *createGroupAct = m_plusMenu->addAction("✨  Create Group");
    connect(addFriendAct, &QAction::triggered,
            this, &ContactPanel::addFriendRequested);
    connect(joinGroupAct, &QAction::triggered,
            this, &ContactPanel::joinGroupRequested);
    connect(createGroupAct, &QAction::triggered,
            this, &ContactPanel::createGroupRequested);

    // Dark mode toggle
    m_plusMenu->addSeparator();
    m_darkModeAction = m_plusMenu->addAction("🌙  Dark Mode");
    connect(m_darkModeAction, &QAction::triggered, this, [this]()
            { emit themeToggled(); });

    layout->addWidget(topBar);

    // ---- Section: Friends ----
    m_friendsHeader = new QLabel("  ▼  Friends", this);
    m_friendsHeader->setStyleSheet(
        "QLabel {"
        "  font-size: 12px;"
        "  color: #999999;"
        "  padding: 6px 12px 4px 12px;"
        "  background: #F7F7F7;"
        "}");
    layout->addWidget(m_friendsHeader);

    m_friendsTree = new QTreeWidget(this);
    m_friendsTree->setHeaderHidden(true);
    m_friendsTree->setRootIsDecorated(false);
    m_friendsTree->setIndentation(0);
    m_friendsTree->setIconSize(QSize(40, 40));
    m_friendsTree->setStyleSheet(
        "QTreeWidget {"
        "  border: none;"
        "  background: #F7F7F7;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  height: 64px;"
        "  padding: 0 12px;"
        "  border-bottom: 1px solid #EAEAEA;"
        "}"
        "QTreeWidget::item:hover { background: #EDEDED; }"
        "QTreeWidget::item:selected { background: #DCDCDC; }");
    layout->addWidget(m_friendsTree, 3); // stretch 3

    // ---- Section: Groups ----
    m_groupsHeader = new QLabel("  ▼  Groups", this);
    m_groupsHeader->setStyleSheet(
        "QLabel {"
        "  font-size: 12px;"
        "  color: #999999;"
        "  padding: 6px 12px 4px 12px;"
        "  background: #F7F7F7;"
        "}");
    layout->addWidget(m_groupsHeader);

    m_groupsTree = new QTreeWidget(this);
    m_groupsTree->setHeaderHidden(true);
    m_groupsTree->setRootIsDecorated(false);
    m_groupsTree->setIndentation(0);
    m_groupsTree->setStyleSheet(
        "QTreeWidget {"
        "  border: none;"
        "  background: #F7F7F7;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  height: 64px;"
        "  padding: 0px;"
        "  border-bottom: 1px solid #EAEAEA;"
        "}"
        "QTreeWidget::item:hover { background: #EDEDED; }"
        "QTreeWidget::item:selected { background: #DCDCDC; }");

    layout->addWidget(m_groupsTree, 2); // stretch 2

    // ---- Signal wiring ----
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ContactPanel::onSearchTextChanged);

    connect(m_plusBtn, &QPushButton::clicked, this, [this]()
            { m_plusMenu->exec(m_plusBtn->mapToGlobal(
                  QPoint(0, m_plusBtn->height()))); });

    connect(m_friendsTree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int /*col*/)
            {
        int friendId = item->data(0, Qt::UserRole).toInt();
        emit friendDoubleClicked(friendId, item->text(0)); });

    connect(m_groupsTree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int /*col*/)
            {
        int groupId = item->data(0, Qt::UserRole).toInt();
        // Resolve group name from the custom widget or stored data
        QString name = item->data(0, Qt::UserRole + 1).toString();
        if (name.isEmpty()) name = QString("Group %1").arg(groupId);
        emit groupDoubleClicked(groupId, name); });
}

// ====================================================================
//  Group item custom widget factory
// ====================================================================

QWidget *ContactPanel::createGroupItemWidget(int groupId,
                                             const QString &groupName,
                                             const QVector<GroupMember> &members,
                                             const QString &groupDesc) const
{
    auto *w = new QWidget();
    auto *hbox = new QHBoxLayout(w);
    w->setStyleSheet("background: transparent;");
    hbox->setContentsMargins(12, 8, 8, 8);
    hbox->setSpacing(10);

    // Group icon placeholder (first letter)
    auto *avatar = new QLabel(groupName.left(1).toUpper(), w);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        "background: #07C160;"
        "color: white;"
        "border-radius: 5px;"
        "font-size: 16px;"
        "font-weight: bold;");

    // Name + member count
    int count = members.size();
    QString labelText = QString("%1  <span style='color:%2;font-size:11px;'>(%3)</span>")
                            .arg(groupName.toHtmlEscaped())
                            .arg(Theme::textMuted())
                            .arg(count == 1 ? "1 member" : QString("%1 members").arg(count));
    auto *label = new QLabel(labelText, w);
    label->setStyleSheet(
        "font-size: 14px;"
        "color: #222222;"
        "background: transparent;");

    hbox->addWidget(avatar);
    hbox->addWidget(label, 1);

    // "···" button
    auto *dotsBtn = new QPushButton("···", w);
    dotsBtn->setFixedSize(28, 28);
    dotsBtn->setFlat(true);
    dotsBtn->setStyleSheet(
        "QPushButton {"
        "  font-size: 14px;"
        "  color: #999999;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:hover {"
        "  background: #EDEDED;"
        "  border-radius: 4px;"
        "}");

    // Build popup menu for group details
    auto *menu = new QMenu(w);
    menu->addAction(QString("Group ID: %1").arg(groupId))->setEnabled(false);
    menu->addAction(QString("Description: %1").arg(groupDesc))->setEnabled(false);
    menu->addSeparator();
    for (const auto &m : members)
    {
        QString stateIcon = (m.state == "online") ? "🟢" : "⚫";
        QString roleTag = (m.role == "creator") ? " [Creator]" : "";
        menu->addAction(QString("%1 %2%3 (ID:%4)")
                            .arg(stateIcon, m.name, roleTag)
                            .arg(m.id))
            ->setEnabled(false);
    }

    QObject::connect(dotsBtn, &QPushButton::clicked, w, [menu, dotsBtn]()
                     { menu->exec(dotsBtn->mapToGlobal(QPoint(0, dotsBtn->height()))); });

    hbox->addWidget(dotsBtn);

    return w;
}

// ====================================================================
//  Search filter
// ====================================================================

void ContactPanel::onSearchTextChanged(const QString &text)
{
    // Filter friends
    for (int i = 0; i < m_friendsTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_friendsTree->topLevelItem(i);
        bool match = text.isEmpty() ||
                     item->text(0).contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }

    // Filter groups — item text lives inside a custom widget, use stored data
    for (int i = 0; i < m_groupsTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_groupsTree->topLevelItem(i);
        QString groupName = item->data(0, Qt::UserRole + 1).toString();
        bool match = text.isEmpty() ||
                     groupName.contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

// ====================================================================
//  Public refresh helpers
// ====================================================================

void ContactPanel::refreshFriendList(const QVector<FriendInfo> &friends)
{
    m_friendsTree->clear();

    for (const auto &f : friends)
    {
        auto *item = new QTreeWidgetItem(m_friendsTree);
        item->setText(0, f.name);
        item->setData(0, Qt::UserRole, f.id);
        item->setData(0, Qt::UserRole + 1, f.name);
        item->setSizeHint(0, QSize(0, 64));
        item->setIcon(0, style()->standardIcon(
                             f.state == "online" ? QStyle::SP_ComputerIcon
                                                 : QStyle::SP_DriveNetIcon));
    }
}

void ContactPanel::refreshGroupList(const QVector<GroupInfo> &groups)
{
    m_groupsTree->clear();

    for (const auto &g : groups)
    {
        auto *item = new QTreeWidgetItem(m_groupsTree);
        item->setData(0, Qt::UserRole, g.id);
        item->setData(0, Qt::UserRole + 1, g.name);
        item->setSizeHint(0, QSize(0, 64));

        m_groupsTree->setItemWidget(
            item, 0, createGroupItemWidget(g.id, g.name, g.members, g.desc));
    }
}

// ====================================================================
//  Theme restyle
// ====================================================================

void ContactPanel::restyle()
{
    // Update dark mode action label
    if (m_darkModeAction)
    {
        m_darkModeAction->setText(Theme::isDark() ? "☀️  Light Mode" : "🌙  Dark Mode");
    }

    // Search input

    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background: #FFFFFF;"
        "  border: none;"
        "  border-radius: 5px;"
        "  padding: 0 10px;"
        "  font-size: 13px;"
        "  color: #333333;"
        "}"
        "QLineEdit:focus { background: #FFFFFF; }");

    // Plus button
    m_plusBtn->setStyleSheet(
        "QPushButton {"
        "  background: #E2E2E2;"
        "  border: none;"
        "  border-radius: 16px;"
        "  font-size: 20px;"
        "  color: #333333;"
        "}"
        "QPushButton:hover { background: #D0D0D0; }"
        "QPushButton:pressed { background: #C8C8C8; }");

    // Section headers
    m_friendsHeader->setStyleSheet(
        "QLabel {"
        "  font-size: 12px;"
        "  color: #999999;"
        "  padding: 6px 12px 4px 12px;"
        "  background: #F7F7F7;"
        "}");
    m_groupsHeader->setStyleSheet(
        "QLabel {"
        "  font-size: 12px;"
        "  color: #999999;"
        "  padding: 6px 12px 4px 12px;"
        "  background: #F7F7F7;"
        "}");

    // Friend tree
    m_friendsTree->setStyleSheet(
        "QTreeWidget {"
        "  border: none;"
        "  background: #F7F7F7;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  height: 64px;"
        "  padding: 0 12px;"
        "  border-bottom: 1px solid #EAEAEA;"
        "}"
        "QTreeWidget::item:hover { background: #EDEDED; }"
        "QTreeWidget::item:selected { background: #DCDCDC; }");

    // Group tree
    m_groupsTree->setStyleSheet(
        "QTreeWidget {"
        "  border: none;"
        "  background: #F7F7F7;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  height: 64px;"
        "  padding: 0px;"
        "  border-bottom: 1px solid #EAEAEA;"
        "}"
        "QTreeWidget::item:hover { background: #EDEDED; }"
        "QTreeWidget::item:selected { background: #DCDCDC; }");

    // Refresh group items (they contain custom widgets with inline styles)
    for (int i = 0; i < m_groupsTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_groupsTree->topLevelItem(i);
        int groupId = item->data(0, Qt::UserRole).toInt();
        QString name = item->data(0, Qt::UserRole + 1).toString();
        // Rebuild the custom widget with current theme colors
        // We don't have member data here, so just clear and the caller can refresh
        // For now, this is handled by MainWindow::refreshTrees() after restyle
    }
}
