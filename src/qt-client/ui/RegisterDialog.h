#ifndef REGISTER_DIALOG_H_
#define REGISTER_DIALOG_H_

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class ProtocolClient;

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(ProtocolClient* client, QWidget* parent = nullptr);

    int assignedId() const { return m_assignedId; }

private slots:
    void onRegisterClicked();
    void onRegisterResult(bool success, int assignedId);

private:
    ProtocolClient* m_client;
    QLineEdit*   m_nameEdit;
    QLineEdit*   m_passwordEdit;
    QPushButton* m_registerBtn;
    QPushButton* m_cancelBtn;
    QLabel*      m_statusLabel;
    int          m_assignedId = 0;
};

#endif // REGISTER_DIALOG_H_
