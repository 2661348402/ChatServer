#ifndef CREATE_GROUP_DIALOG_H_
#define CREATE_GROUP_DIALOG_H_

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class ProtocolClient;

class CreateGroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateGroupDialog(ProtocolClient* client, QWidget* parent = nullptr);

private slots:
    void onCreateClicked();

private:
    ProtocolClient* m_client;
    QLineEdit*   m_nameEdit;
    QLineEdit*   m_descEdit;
    QPushButton* m_createBtn;
    QPushButton* m_cancelBtn;
};

#endif // CREATE_GROUP_DIALOG_H_
