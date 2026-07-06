#ifndef FRAMED_TCP_CLIENT_H_
#define FRAMED_TCP_CLIENT_H_

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>

/// Low-level TCP client with 4-byte big-endian length-prefix framing.
/// Mirrors the server's ChatServer::onMessage() framing logic exactly.
class FramedTcpClient : public QObject
{
    Q_OBJECT

public:
    explicit FramedTcpClient(QObject* parent = nullptr);
    ~FramedTcpClient() override;

    void connectToHost(const QString& host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    /// Send a framed message: [4-byte BE length][payload]
    void sendFrame(const QByteArray& payload);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorString);
    void frameReceived(const QByteArray& frame);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();

private:
    QTcpSocket* m_socket;
    QByteArray  m_buffer;   // accumulation buffer for incomplete frames
};

#endif // FRAMED_TCP_CLIENT_H_
