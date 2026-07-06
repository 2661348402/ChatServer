#ifndef LOGIN_DIALOG_H_
#define LOGIN_DIALOG_H_

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class ProtocolClient;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(ProtocolClient* client, QWidget* parent = nullptr);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onServerConnected();
    void onServerDisconnected();
    void onServerError(const QString& errorString);
    void onLoginResult(bool success, int errnoCode, const QString& errMsg,
                       int userId, const QString& userName,
                       const QVector<struct ChatMessage>& offlineMessages,
                       const QVector<struct FriendInfo>& friends,
                       const QVector<struct GroupInfo>& groups);
    void onRegisterResult(bool success, int assignedId);

private:
    ProtocolClient* m_client;

    QLineEdit*   m_idEdit;
    QLineEdit*   m_passwordEdit;
    QPushButton* m_loginBtn;
    QPushButton* m_registerBtn;
    QLabel*      m_statusLabel;
};

#endif // LOGIN_DIALOG_H_
