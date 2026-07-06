#include "network/FramedTcpClient.h"

#include <arpa/inet.h>
#include <cstring>

FramedTcpClient::FramedTcpClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected,
            this, &FramedTcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &FramedTcpClient::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &FramedTcpClient::onError);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &FramedTcpClient::onReadyRead);
}

FramedTcpClient::~FramedTcpClient()
{
}

void FramedTcpClient::connectToHost(const QString& host, quint16 port)
{
    m_buffer.clear();
    m_socket->connectToHost(host, port);
}

void FramedTcpClient::disconnectFromHost()
{
    m_socket->disconnectFromHost();
}

bool FramedTcpClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void FramedTcpClient::sendFrame(const QByteArray& payload)
{
    // 4-byte big-endian length prefix + payload
    uint32_t beLen = htonl(static_cast<uint32_t>(payload.size()));
    QByteArray packet;
    packet.resize(static_cast<int>(sizeof(beLen)) + payload.size());
    std::memcpy(packet.data(), &beLen, sizeof(beLen));
    std::memcpy(packet.data() + sizeof(beLen), payload.constData(),
                static_cast<size_t>(payload.size()));
    m_socket->write(packet);
}

// ---- private slots ----

void FramedTcpClient::onConnected()
{
    emit connected();
}

void FramedTcpClient::onDisconnected()
{
    m_buffer.clear();
    emit disconnected();
}

void FramedTcpClient::onError(QAbstractSocket::SocketError /*error*/)
{
    emit errorOccurred(m_socket->errorString());
}

void FramedTcpClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (true) {
        // Need at least 4 bytes for the length prefix
        if (m_buffer.size() < 4)
            break;

        // Read big-endian 32-bit length (same as ntohl on the wire)
        uint32_t beLen;
        std::memcpy(&beLen, m_buffer.constData(), sizeof(beLen));
        uint32_t msgLen = ntohl(beLen);

        // Safety: reject oversized frames (1 MiB limit, matches server)
        if (msgLen > 1024 * 1024) {
            m_socket->abort();
            return;
        }

        // Check if complete frame is available
        if (static_cast<quint32>(m_buffer.size()) < 4 + msgLen)
            break;  // incomplete frame, wait for more data

        // Extract payload (skip 4-byte header)
        QByteArray frame = m_buffer.mid(4, static_cast<int>(msgLen));
        m_buffer.remove(0, 4 + static_cast<int>(msgLen));

        emit frameReceived(frame);
    }
}
