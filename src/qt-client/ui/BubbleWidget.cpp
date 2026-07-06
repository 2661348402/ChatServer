#include "ui/BubbleWidget.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QResizeEvent>

BubbleWidget::BubbleWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(12, 4, 12, 4);

    // Left spacer (used for right-aligned self messages)
    m_leftSpacer = new QWidget(this);
    m_leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Right spacer (used for left-aligned other messages)
    m_rightSpacer = new QWidget(this);
    m_rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // ---- Bubble frame ----
    m_bubble = new QFrame(this);
    m_bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    auto* bubbleLayout = new QVBoxLayout(m_bubble);
    bubbleLayout->setContentsMargins(12, 8, 12, 6);
    bubbleLayout->setSpacing(4);

    // Name label (hidden by default, shown for group chat)
    m_nameLabel = new QLabel(m_bubble);
    m_nameLabel->setStyleSheet(
        QString("font-size:11px; color:%1;").arg(Theme::textMuted()));
    m_nameLabel->hide();
    bubbleLayout->addWidget(m_nameLabel);

    // Message text
    m_textLabel = new QLabel(m_bubble);
    m_textLabel->setWordWrap(true);
    m_textLabel->setStyleSheet("font-size:14px; padding:2px 0;");
    m_textLabel->setTextFormat(Qt::PlainText);
    m_textLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    bubbleLayout->addWidget(m_textLabel);

    // Timestamp
    m_timeLabel = new QLabel(m_bubble);
    m_timeLabel->setStyleSheet(
        QString("font-size:10px; color:%1;").arg(Theme::textLight()));
    m_timeLabel->setAlignment(Qt::AlignRight);
    bubbleLayout->addWidget(m_timeLabel);

    // Assemble: [leftSpacer] [bubble] [rightSpacer]
    outerLayout->addWidget(m_leftSpacer);
    outerLayout->addWidget(m_bubble);
    outerLayout->addWidget(m_rightSpacer);

    // Store spacer references for alignment switching
    // We'll use the spacers via show/hide in setMessage
}

void BubbleWidget::setMessage(const QString& name, const QString& text,
                               const QString& time, bool isSelf, bool showName)
{
    // Set text content
    m_textLabel->setText(text);
    m_timeLabel->setText(time);
    m_isSelf = isSelf;

    // Show/hide name
    if (showName && !name.isEmpty()) {
        m_nameLabel->setText(name);
        m_nameLabel->show();
    } else {
        m_nameLabel->hide();
    }

    // ---- Style and alignment per sender ----
    if (isSelf) {
        // Right-aligned: show left spacer, hide right spacer
        m_leftSpacer->show();
        m_rightSpacer->hide();

        m_bubble->setStyleSheet(
            QString("QFrame {"
            "  background: %1;"
            "  border: none;"
            "  border-radius: 8px;"
            "}").arg(Theme::bubbleSelf()));
        m_nameLabel->setStyleSheet(
            QString("font-size:11px; color:%1;").arg(Theme::bubbleSelfName()));
        m_timeLabel->setStyleSheet(
            QString("font-size:10px; color:%1;").arg(Theme::bubbleSelfTime()));
    } else {
        // Left-aligned: hide left spacer, show right spacer
        m_leftSpacer->hide();
        m_rightSpacer->show();

        m_bubble->setStyleSheet(
            QString("QFrame {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "}").arg(Theme::bubbleOther(), Theme::borderBubble()));
        m_nameLabel->setStyleSheet(
            QString("font-size:11px; color:%1;").arg(Theme::textMuted()));
        m_timeLabel->setStyleSheet(
            QString("font-size:10px; color:%1;").arg(Theme::bubbleOtherTime()));
    }
}

void BubbleWidget::restyle()
{
    if (m_isSelf) {
        m_bubble->setStyleSheet(
            QString("QFrame {"
            "  background: %1;"
            "  border: none;"
            "  border-radius: 8px;"
            "}").arg(Theme::bubbleSelf()));
        m_nameLabel->setStyleSheet(
            QString("font-size:11px; color:%1;").arg(Theme::bubbleSelfName()));
        m_timeLabel->setStyleSheet(
            QString("font-size:10px; color:%1;").arg(Theme::bubbleSelfTime()));
    } else {
        m_bubble->setStyleSheet(
            QString("QFrame {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "}").arg(Theme::bubbleOther(), Theme::borderBubble()));
        m_nameLabel->setStyleSheet(
            QString("font-size:11px; color:%1;").arg(Theme::textMuted()));
        m_timeLabel->setStyleSheet(
            QString("font-size:10px; color:%1;").arg(Theme::bubbleOtherTime()));
    }
}

void BubbleWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Bubble max width = 65% of the available chat width, clamped to [180, 480]
    int newMax = qBound(180, static_cast<int>(width() * 0.65), 480);
    m_bubble->setMaximumWidth(newMax);
}
