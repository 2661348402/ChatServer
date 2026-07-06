#include "ui/LoginDialog.h"
#include "ui/RegisterDialog.h"
#include "Theme.h"
#include "network/ProtocolClient.h"
#include "models/ChatData.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QIntValidator>
#include <QFrame>

LoginDialog::LoginDialog(ProtocolClient* client, QWidget* parent)
    : QDialog(parent)
    , m_client(client)
{
    setWindowTitle("ChatServer — Login");
    setFixedSize(440, 370);
    setStyleSheet(QString("QDialog { background: %1; }").arg(Theme::bgDialog()));

    // ---- Outer layout centers the card ----
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setAlignment(Qt::AlignCenter);

    // ---- Card frame ----
    auto* card = new QFrame(this);
    card->setMinimumWidth(370);
    card->setStyleSheet(
        QString("QFrame {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}").arg(Theme::bgCard(), Theme::borderCard()));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(14);

    // Title
    auto* titleLabel = new QLabel("ChatServer", card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QString("font-size: 22px; font-weight: bold; color: %1; "
        "padding-bottom: 4px; border: none; background: transparent;")
            .arg(Theme::green()));
    cardLayout->addWidget(titleLabel);

    // Subtitle
    auto* subLabel = new QLabel("Sign in to your account", card);
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setStyleSheet(
        QString("font-size: 13px; color: %1; padding-bottom: 8px; "
        "border: none; background: transparent;").arg(Theme::textMuted()));
    cardLayout->addWidget(subLabel);

    // ID input
    m_idEdit = new QLineEdit(card);
    m_idEdit->setPlaceholderText("User ID");
    m_idEdit->setValidator(new QIntValidator(1, 999999, this));
    m_idEdit->setMinimumWidth(300);
    m_idEdit->setStyleSheet(
        QString("QLineEdit {"
        "  border: 1px solid %1; border-radius: 6px;"
        "  padding: 10px 14px; font-size: 14px; background: %2;"
        "}"
        "QLineEdit:focus {"
        "  border-color: %3; background: white;"
        "}").arg(Theme::borderInput(), Theme::bgInput(), Theme::green()));
    cardLayout->addWidget(m_idEdit);

    // Password input
    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setPlaceholderText("Password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMinimumWidth(300);
    m_passwordEdit->setStyleSheet(m_idEdit->styleSheet());
    cardLayout->addWidget(m_passwordEdit);

    // Login button
    m_loginBtn = new QPushButton("Log In", card);
    m_loginBtn->setDefault(true);
    m_loginBtn->setEnabled(false);
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setStyleSheet(
        QString("QPushButton {"
        "  background: %1; color: white; border: none; border-radius: 6px;"
        "  padding: 10px; font-size: 15px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %3; }"
        "QPushButton:disabled { background: %4; }")
            .arg(Theme::green(), Theme::greenHover(),
                 Theme::greenPressed(), Theme::btnDisabled()));
    cardLayout->addWidget(m_loginBtn);

    // Register link
    m_registerBtn = new QPushButton("Create new account", card);
    m_registerBtn->setFlat(true);
    m_registerBtn->setCursor(Qt::PointingHandCursor);
    m_registerBtn->setStyleSheet(
        QString("QPushButton {"
        "  color: %1; border: none; font-size: 13px;"
        "  background: transparent;"
        "}"
        "QPushButton:hover { color: %2; }")
            .arg(Theme::textLink(), Theme::green()));
    cardLayout->addWidget(m_registerBtn);

    // Status label
    m_statusLabel = new QLabel("Connecting to server...", card);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px; padding-top: 4px; "
        "border: none; background: transparent;").arg(Theme::textError()));
    cardLayout->addWidget(m_statusLabel);

    outerLayout->addWidget(card);
    setLayout(outerLayout);

    // Wire signals
    connect(m_loginBtn, &QPushButton::clicked,
            this, &LoginDialog::onLoginClicked);
    connect(m_registerBtn, &QPushButton::clicked,
            this, &LoginDialog::onRegisterClicked);
    connect(m_client, &ProtocolClient::serverConnected,
            this, &LoginDialog::onServerConnected);
    connect(m_client, &ProtocolClient::serverDisconnected,
            this, &LoginDialog::onServerDisconnected);
    connect(m_client, &ProtocolClient::serverError,
            this, &LoginDialog::onServerError);
    connect(m_client, &ProtocolClient::loginResult,
            this, &LoginDialog::onLoginResult);
    connect(m_client, &ProtocolClient::registerResult,
            this, &LoginDialog::onRegisterResult);
}

void LoginDialog::onLoginClicked()
{
    bool ok;
    int id = m_idEdit->text().toInt(&ok);
    if (!ok || id <= 0) {
        m_statusLabel->setText("Please enter a valid numeric ID");
        return;
    }
    QString pwd = m_passwordEdit->text();
    if (pwd.isEmpty()) {
        m_statusLabel->setText("Please enter a password");
        return;
    }

    m_loginBtn->setEnabled(false);
    m_statusLabel->setText("Logging in...");
    m_client->sendLogin(id, pwd);
}

void LoginDialog::onRegisterClicked()
{
    RegisterDialog dlg(m_client, this);
    if (dlg.exec() == QDialog::Accepted) {
        int assignedId = dlg.assignedId();
        m_idEdit->setText(QString::number(assignedId));
        m_passwordEdit->clear();
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; border: none; background: transparent;").arg(Theme::green()));
        m_statusLabel->setText(
            QString("Registration successful! Your ID is %1. Please log in.")
                .arg(assignedId));
    }
}

void LoginDialog::onServerConnected()
{
    m_loginBtn->setEnabled(true);
    m_statusLabel->setStyleSheet(
        "color: #07C160; font-size: 12px; border: none; background: transparent;");
    m_statusLabel->setText("Connected to server. Please log in.");
}

void LoginDialog::onServerDisconnected()
{
    m_loginBtn->setEnabled(false);
    m_statusLabel->setStyleSheet(
        "color: #E53935; font-size: 12px; border: none; background: transparent;");
    m_statusLabel->setText("Disconnected from server");
}

void LoginDialog::onServerError(const QString& errorString)
{
    m_statusLabel->setStyleSheet(
        "color: #E53935; font-size: 12px; border: none; background: transparent;");
    m_statusLabel->setText("Connection error: " + errorString);
}

void LoginDialog::onLoginResult(bool success, int errnoCode,
                                 const QString& errMsg,
                                 int /*userId*/, const QString& /*userName*/,
                                 const QVector<ChatMessage>& /*offlineMessages*/,
                                 const QVector<FriendInfo>& /*friends*/,
                                 const QVector<GroupInfo>& /*groups*/)
{
    if (success) {
        accept();   // close dialog with QDialog::Accepted
    } else {
        m_loginBtn->setEnabled(true);
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; border: none; background: transparent;").arg(Theme::textError()));
        m_statusLabel->setText(
            QString("Login failed [%1]: %2").arg(errnoCode).arg(errMsg));
    }
}

void LoginDialog::onRegisterResult(bool success, int assignedId)
{
    (void)success;
    (void)assignedId;
}
