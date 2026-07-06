#ifndef JOIN_GROUP_DIALOG_H_
#define JOIN_GROUP_DIALOG_H_

#include <QDialog>
#include <QSpinBox>
#include <QPushButton>

class ProtocolClient;

class JoinGroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JoinGroupDialog(ProtocolClient* client, QWidget* parent = nullptr);

private slots:
    void onJoinClicked();

private:
    ProtocolClient* m_client;
    QSpinBox*    m_idSpin;
    QPushButton* m_joinBtn;
    QPushButton* m_cancelBtn;
};

#endif // JOIN_GROUP_DIALOG_H_
