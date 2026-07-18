#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include "Theme.h"
#include "network/ProtocolClient.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ChatClientQt");
    app.setApplicationVersion("1.1.0");

    // Global font — try preferred CJK families, fall back to system default
    static const char* kPreferredFonts[] = {
        "WenQuanYi Micro Hei",
        "Noto Sans CJK SC",
        "Noto Sans SC",
        "Source Han Sans SC",
        "Microsoft YaHei",
        "SimHei",
        "PingFang SC",
    };
    QString fontFamily;
    QFontDatabase fontDb;
    for (const char* name : kPreferredFonts) {
        if (fontDb.families().contains(QLatin1String(name))) {
            fontFamily = QLatin1String(name);
            break;
        }
    }
    QFont globalFont = app.font();
    if (!fontFamily.isEmpty()) {
        globalFont.setFamily(fontFamily);
        globalFont.setPointSize(10);
    } // else keep the system default font and size
    app.setFont(globalFont);

    // ---- Programmatic application icon (green chat bubble) ----
    {
        QPixmap pix(64, 64);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        // Bubble body
        p.setBrush(QColor(Theme::green()));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(4, 6, 56, 38, 18, 18);
        // Bubble tail
        QPointF tail[] = {{16, 44}, {6, 56}, {28, 48}};
        p.drawPolygon(tail, 3);
        // "C" letter
        p.setPen(QPen(Qt::white, 3));
        p.setFont(QFont("Arial", 22, QFont::Bold));
        p.drawText(QRect(4, 6, 56, 38), Qt::AlignCenter, "C");
        p.end();
        app.setWindowIcon(QIcon(pix));
    }

    // Global stylesheet — same fallback chain in CSS syntax
    QString fontCss = fontFamily.isEmpty()
        ? QStringLiteral("font-family: sans-serif;")
        : QString("font-family: '%1', 'Noto Sans CJK SC', 'Microsoft YaHei', sans-serif;")
              .arg(fontFamily);

    // Global stylesheet
    app.setStyleSheet(
        QString(
            "QWidget { %1 }"
            "QScrollBar:vertical {"
            "  background: transparent; width: 6px; margin: 0;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: %2; border-radius: 3px; min-height: 30px;"
            "}"
            "QScrollBar::handle:vertical:hover { background: %3; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "  height: 0; border: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "  background: transparent;"
            "}"
        ).arg(fontCss, Theme::scrollHandle(), Theme::scrollHandleHover()));

    // Parse command line
    QCommandLineParser parser;
    parser.setApplicationDescription("Qt GUI Client for Cluster Chat Server");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"H", "host"}, "Server host", "host", "127.0.0.1"});
    parser.addOption({{"P", "port"}, "Server port", "port", "6001"});
    parser.process(app);

    QString host = parser.value("host");
    quint16 port = static_cast<quint16>(parser.value("port").toUInt());

    // Create protocol client (shared across dialogs)
    ProtocolClient client;

    // Connect to server
    client.connectToServer(host, port);

    // Show login dialog
    LoginDialog loginDlg(&client);
    if (loginDlg.exec() != QDialog::Accepted)
        return 0;   // user closed login dialog

    // Login successful — show main window
    MainWindow mainWin(&client);
    mainWin.show();
    mainWin.showOfflineMessages();

    int ret = app.exec();

    // Clean shutdown
    client.sendLogout();

    return ret;
}
