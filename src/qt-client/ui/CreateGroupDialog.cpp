#include "ui/CreateGroupDialog.h"
#include "network/ProtocolClient.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>

CreateGroupDialog::CreateGroupDialog(ProtocolClient* client, QWidget* parent)
    : QDialog(parent)
    , m_client(client)
{
    setWindowTitle("ChatServer — Create Group");
    setFixedSize(420, 330);
    setStyleSheet(QString("QDialog { background: %1; }").arg(Theme::bgDialog()));

    // ---- Outer layout centers the card ----
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setAlignment(Qt::AlignCenter);

    // ---- Card frame ----
    auto* card = new QFrame(this);
    card->setMinimumWidth(350);
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
    auto* titleLabel = new QLabel("Create Group", card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QString("font-size: 20px; font-weight: bold; color: %1; "
        "padding-bottom: 8px; border: none; background: transparent;")
        .arg(Theme::textPrimary()));
    cardLayout->addWidget(titleLabel);

    // Group name input
    m_nameEdit = new QLineEdit(card);
    m_nameEdit->setPlaceholderText("Group name");
    m_nameEdit->setMinimumWidth(290);
    m_nameEdit->setStyleSheet(
        QString("QLineEdit {"
        "  border: 1px solid %1; border-radius: 6px;"
        "  padding: 10px 14px; font-size: 14px; background: %2;"
        "}"
        "QLineEdit:focus {"
        "  border-color: %3; background: %4;"
        "}").arg(Theme::borderInput(), Theme::bgInput(),
                 Theme::green(), Theme::bgCard()));
    cardLayout->addWidget(m_nameEdit);

    // Description input
    m_descEdit = new QLineEdit(card);
    m_descEdit->setPlaceholderText("Description (optional)");
    m_descEdit->setMinimumWidth(290);
    m_descEdit->setStyleSheet(m_nameEdit->styleSheet());
    cardLayout->addWidget(m_descEdit);

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

    m_createBtn = new QPushButton("Create", card);
    m_createBtn->setDefault(true);
    m_createBtn->setCursor(Qt::PointingHandCursor);
    m_createBtn->setStyleSheet(
        QString("QPushButton {"
        "  background: %1; color: %2; border: none; border-radius: 6px;"
        "  padding: 10px; font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }")
        .arg(Theme::green(), Theme::bgCard(),
             Theme::greenHover(), Theme::greenPressed()));
    btnLayout->addWidget(m_createBtn);

    cardLayout->addLayout(btnLayout);
    outerLayout->addWidget(card);
    setLayout(outerLayout);

    connect(m_createBtn, &QPushButton::clicked,
            this, &CreateGroupDialog::onCreateClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
}

void CreateGroupDialog::onCreateClicked()
{
    m_client->sendCreateGroup(m_nameEdit->text(), m_descEdit->text());
    accept();
}
