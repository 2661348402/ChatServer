#ifndef ADD_FRIEND_DIALOG_H_
#define ADD_FRIEND_DIALOG_H_

#include <QDialog>
#include <QSpinBox>
#include <QPushButton>

class ProtocolClient;

class AddFriendDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddFriendDialog(ProtocolClient* client, QWidget* parent = nullptr);

private slots:
    void onAddClicked();

private:
    ProtocolClient* m_client;
    QSpinBox*    m_idSpin;
    QPushButton* m_addBtn;
    QPushButton* m_cancelBtn;
};

#endif // ADD_FRIEND_DIALOG_H_
