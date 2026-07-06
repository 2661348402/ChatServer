#include "ui/JoinGroupDialog.h"
#include "network/ProtocolClient.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>

JoinGroupDialog::JoinGroupDialog(ProtocolClient* client, QWidget* parent)
    : QDialog(parent)
    , m_client(client)
{
    setWindowTitle("ChatServer — Join Group");
    setFixedSize(400, 260);
    setStyleSheet(QString("QDialog { background: %1; }").arg(Theme::bgDialog()));

    // ---- Outer layout centers the card ----
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setAlignment(Qt::AlignCenter);

    // ---- Card frame ----
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
    auto* titleLabel = new QLabel("Join Group", card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QString("font-size: 20px; font-weight: bold; color: %1; "
        "padding-bottom: 8px; border: none; background: transparent;")
        .arg(Theme::textPrimary()));
    cardLayout->addWidget(titleLabel);

    // Group ID input
    m_idSpin = new QSpinBox(card);
    m_idSpin->setRange(1, 999999);
    m_idSpin->setMinimumWidth(270);
    m_idSpin->setPrefix("ID: ");
    m_idSpin->setStyleSheet(
        QString("QSpinBox {"
        "  border: 1px solid %1; border-radius: 6px;"
        "  padding: 10px 14px; font-size: 14px; background: %2;"
        "}"
        "QSpinBox:focus {"
        "  border-color: %3; background: %4;"
        "}").arg(Theme::borderInput(), Theme::bgInput(),
                 Theme::green(), Theme::bgCard()));
    cardLayout->addWidget(m_idSpin);

    // Button row
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(12);

    m_cancelBtn = new QPushButton("Cancel", card);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setStyleSheet(
        QString("QPushButton {"
        "  background: %1; color: %2; border: none; border-radius: 6px;"
        "  padding: 10px; font-size: 14px;"
        "}"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }")
        .arg(Theme::btnSecondary(), Theme::textPrimary(),
             Theme::btnSecondaryHover(), Theme::btnSecondaryPressed()));
    btnLayout->addWidget(m_cancelBtn);

    m_joinBtn = new QPushButton("Join", card);
    m_joinBtn->setDefault(true);
    m_joinBtn->setCursor(Qt::PointingHandCursor);
    m_joinBtn->setStyleSheet(
        QString("QPushButton {"
        "  background: %1; color: white; border: none; border-radius: 6px;"
        "  padding: 10px; font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %3; }")
        .arg(Theme::green(), Theme::greenHover(), Theme::greenPressed()));
    btnLayout->addWidget(m_joinBtn);

    cardLayout->addLayout(btnLayout);
    outerLayout->addWidget(card);
    setLayout(outerLayout);

    connect(m_joinBtn, &QPushButton::clicked,
            this, &JoinGroupDialog::onJoinClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
}

void JoinGroupDialog::onJoinClicked()
{
    m_client->sendJoinGroup(m_idSpin->value());
    accept();
}
