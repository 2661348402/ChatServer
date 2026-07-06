#ifndef BUBBLE_WIDGET_H_
#define BUBBLE_WIDGET_H_

#include <QWidget>
#include <QLabel>
#include <QFrame>

/// A single WeChat-style chat bubble.
/// Self messages: green (#95EC69) right-aligned.
/// Other messages: white left-aligned.
class BubbleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BubbleWidget(QWidget* parent = nullptr);

    /// Populate the bubble with message data.
    /// @param name     Sender display name (shown above bubble for group chat)
    /// @param text     Message body text
    /// @param time     Formatted timestamp
    /// @param isSelf   true → green right-aligned; false → white left-aligned
    /// @param showName Whether to display the name label above the bubble
    void setMessage(const QString& name, const QString& text,
                    const QString& time, bool isSelf, bool showName);

    /// Re-apply bubble colors after theme change
    void restyle();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QFrame*  m_bubble      = nullptr;
    QLabel*  m_nameLabel   = nullptr;
    QLabel*  m_textLabel   = nullptr;
    QLabel*  m_timeLabel   = nullptr;
    QWidget* m_leftSpacer  = nullptr;
    QWidget* m_rightSpacer = nullptr;
    bool     m_isSelf      = false;
};

#endif // BUBBLE_WIDGET_H_
