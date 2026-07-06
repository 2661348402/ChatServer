#ifndef CONTACT_PANEL_H_
#define CONTACT_PANEL_H_

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QLabel>
#include <QMenu>
#include "models/ChatData.h"

/// Left panel: search bar, + menu, friends tree, groups tree.
/// Emits signals for the main window to handle tab creation.
class ContactPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ContactPanel(QWidget* parent = nullptr);

    /// Refresh friend list from data
    void refreshFriendList(const QVector<FriendInfo>& friends);

    /// Refresh group list from data
    void refreshGroupList(const QVector<GroupInfo>& groups);

    /// Re-apply all stylesheets after theme change
    void restyle();

signals:
    void friendDoubleClicked(int friendId, const QString& name);
    void groupDoubleClicked(int groupId, const QString& name);
    void addFriendRequested();
    void createGroupRequested();
    void joinGroupRequested();
    void themeToggled();

private slots:
    void onSearchTextChanged(const QString& text);

private:
    /// Build a custom widget for a group tree item row
    QWidget* createGroupItemWidget(int groupId, const QString& groupName,
                                   const QVector<GroupMember>& members,
                                   const QString& groupDesc) const;

    // Search bar & plus button
    QLineEdit*   m_searchEdit;
    QPushButton* m_plusBtn;
    QMenu*       m_plusMenu;
    QAction*     m_darkModeAction = nullptr;

    // Section headers
    QLabel* m_friendsHeader = nullptr;
    QLabel* m_groupsHeader  = nullptr;

    // Tree widgets
    QTreeWidget* m_friendsTree;
    QTreeWidget* m_groupsTree;
};

#endif // CONTACT_PANEL_H_
