#include "ui/RegisterDialog.h"
#include "network/ProtocolClient.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

RegisterDialog::RegisterDialog(ProtocolClient* client, QWidget* parent)
    : QDialog(parent)
    , m_client(client)
{
    setWindowTitle("ChatServer — Register");
    setFixedSize(400, 300);
    setStyleSheet(QString("QDialog { background: %1; }").arg(Theme::bgDialog()));

    // Outer layout centers the card
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setAlignment(Qt::AlignCenter);

    // Card frame
    auto* card = new QFrame(this);
    card->setMinimumWidth(330);
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
    auto* titleLabel = new QLabel("Create Account", card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QString("font-size: 20px; font-weight: bold; color: %1; "
        "padding-bottom: 8px; border: none; background: transparent;").arg(Theme::textPrimary()));
    cardLayout->addWidget(titleLabel);

    // Name input
    m_nameEdit = new QLineEdit(card);
    m_nameEdit->setPlaceholderText("Your name");
    m_nameEdit->setMinimumWidth(270);
    m_nameEdit->setStyleSheet(
        QString("QLineEdit {"
        "  border: 1px solid %1; border-radius: 6px;"
        "  padding: 10px 14px; font-size: 14px; background: %2;"
        "}"
        "QLineEdit:focus {"
        "  border-color: %3; background: %4;"
        "}").arg(Theme::borderInput(), Theme::bgInput(), Theme::green(), Theme::bgCard()));
    cardLayout->addWidget(m_nameEdit);

    // Password input
    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setPlaceholderText("Password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMinimumWidth(270);
    m_passwordEdit->setStyleSheet(m_nameEdit->styleSheet());
    cardLayout->addWidget(m_passwordEdit);

    // Register button
    m_registerBtn = new QPushButton("Register", card);
    m_registerBtn->setDefault(true);
    m_registerBtn->setCursor(Qt::PointingHandCursor);
    m_registerBtn->setStyleSheet(
        QString("QPushButton {"
        "  background: %1; color: %2; border: none; border-radius: 6px;"
        "  padding: 10px; font-size: 15px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }"
        "QPushButton:disabled { background: %5; }").arg(Theme::green(), Theme::bgCard(), Theme::greenHover(), Theme::greenPressed(), Theme::btnDisabled()));
    cardLayout->addWidget(m_registerBtn);

    // Cancel link
    m_cancelBtn = new QPushButton("Cancel", card);
    m_cancelBtn->setFlat(true);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setStyleSheet(
        QString("QPushButton {"
        "  color: %1; border: none; font-size: 13px;"
        "  background: transparent;"
        "}"
        "QPushButton:hover { color: %2; }").arg(Theme::textMuted(), Theme::textSecondary()));
    cardLayout->addWidget(m_cancelBtn);

    // Status label
    m_statusLabel = new QLabel(card);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px; padding-top: 2px; "
        "border: none; background: transparent;").arg(Theme::textError()));
    cardLayout->addWidget(m_statusLabel);

    outerLayout->addWidget(card);
    setLayout(outerLayout);

    connect(m_registerBtn, &QPushButton::clicked,
            this, &RegisterDialog::onRegisterClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(m_client, &ProtocolClient::registerResult,
            this, &RegisterDialog::onRegisterResult);
}

void RegisterDialog::onRegisterClicked()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_statusLabel->setText("Please enter a name");
        return;
    }
    QString pwd = m_passwordEdit->text();
    if (pwd.isEmpty()) {
        m_statusLabel->setText("Please enter a password");
        return;
    }

    m_registerBtn->setEnabled(false);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px; border: none; background: transparent;").arg(Theme::textMuted()));
    m_statusLabel->setText("Registering...");
    m_client->sendRegister(name, pwd);
}

void RegisterDialog::onRegisterResult(bool success, int assignedId)
{
    if (success) {
        m_assignedId = assignedId;
        accept();
    } else {
        m_registerBtn->setEnabled(true);
        m_statusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; border: none; background: transparent;").arg(Theme::textError()));
        m_statusLabel->setText("Registration failed. Try a different name.");
    }
}
