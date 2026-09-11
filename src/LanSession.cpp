#include "LanSession.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QUdpSocket>

namespace {

const QByteArray kProbe = QByteArrayLiteral("SNAPSZER-DISCOVER 1");
const QByteArray kReplyPrefix = QByteArrayLiteral("SNAPSZER-HOST 1 ");
const int kMaxBuffer = 256 * 1024;
const int kPingInterval = 10000;
// A peer that has sent nothing (not even a ping) for this long is gone, e.g.
// it left the WLAN. Kept generous because phones briefly stall on wake-up.
const int kIdleTimeout = 45000;
const int kConnectTimeout = 8000;

} // namespace

LanSession::LanSession(QObject* parent)
    : QObject(parent)
{
    m_pingTimer.setInterval(kPingInterval);
    m_idleTimer.setSingleShot(true);
    m_connectTimer.setSingleShot(true);
    m_probeTimer.setInterval(600);

    connect(&m_server, &QTcpServer::newConnection, this, &LanSession::acceptConnection);
    connect(&m_pingTimer, &QTimer::timeout, this, [this]() {
        sendRaw(QByteArrayLiteral("{\"t\":\"ping\"}\n"));
    });
    connect(&m_idleTimer, &QTimer::timeout, this, [this]() {
        dropSocket();
        emit peerLost();
    });
    connect(&m_connectTimer, &QTimer::timeout, this, [this]() {
        if (m_role != Guest || peerConnected())
            return;
        dropSocket();
        m_role = None;
        emit connectionFailed(tr("No answer from that address"));
    });
    connect(&m_probeTimer, &QTimer::timeout, this, &LanSession::sendDiscoveryProbe);
}

LanSession::~LanSession()
{
    // Receivers may already be half-destroyed during application shutdown.
    blockSignals(true);
    stop();
}

bool LanSession::peerConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

QString LanSession::peerAddress() const
{
    if (!m_socket)
        return QString();
    QHostAddress address = m_socket->peerAddress();
    bool ok = false;
    const quint32 ipv4 = address.toIPv4Address(&ok);
    return ok ? QHostAddress(ipv4).toString() : address.toString();
}

bool LanSession::startHosting(const QString& hostName, QString* error)
{
    stop();
    m_hostName = hostName;
    if (!m_server.listen(QHostAddress::Any, GamePort)) {
        if (error)
            *error = m_server.errorString();
        return false;
    }
    m_role = Host;

    m_responder = new QUdpSocket(this);
    if (m_responder->bind(QHostAddress::AnyIPv4, DiscoveryPort,
                          QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        connect(m_responder, &QUdpSocket::readyRead, this, &LanSession::answerDiscovery);
    }
    // A failed responder only disables automatic discovery; joining by
    // address still works, so hosting continues.
    return true;
}

void LanSession::joinHost(const QString& address)
{
    stop();
    m_role = Guest;
    QTcpSocket* socket = new QTcpSocket(this);
    attachSocket(socket);
    connect(socket, &QTcpSocket::connected, this, [this]() {
        m_connectTimer.stop();
        m_idleTimer.start(kIdleTimeout);
        m_pingTimer.start();
        emit peerConnectedChanged();
    });
    m_connectTimer.start(kConnectTimeout);
    socket->connectToHost(address.trimmed(), GamePort);
}

void LanSession::discoverHosts()
{
    if (!m_probe) {
        m_probe = new QUdpSocket(this);
        if (!m_probe->bind(QHostAddress(QHostAddress::AnyIPv4), 0)) {
            m_probe->deleteLater();
            m_probe = nullptr;
            return;
        }
        connect(m_probe, &QUdpSocket::readyRead, this, &LanSession::readDiscoveryReplies);
    }
    m_probesLeft = 4;
    sendDiscoveryProbe();
    m_probeTimer.start();
}

void LanSession::stop()
{
    const bool wasConnected = peerConnected();
    m_connectTimer.stop();
    m_probeTimer.stop();
    if (wasConnected)
        m_socket->flush();
    dropSocket();
    m_server.close();
    if (m_responder) {
        m_responder->close();
        m_responder->deleteLater();
        m_responder = nullptr;
    }
    if (m_probe) {
        m_probe->close();
        m_probe->deleteLater();
        m_probe = nullptr;
    }
    m_role = None;
    if (wasConnected)
        emit peerConnectedChanged();
}

void LanSession::send(const QVariantMap& message)
{
    QByteArray line = QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact);
    line.append('\n');
    sendRaw(line);
}

void LanSession::sendRaw(const QByteArray& line)
{
    if (peerConnected())
        m_socket->write(line);
}

QStringList LanSession::localAddresses()
{
    QStringList result;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                result.append(entry.ip().toString());
        }
    }
    return result;
}

void LanSession::acceptConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket* socket = m_server.nextPendingConnection();
        if (m_socket) {
            socket->write(QByteArrayLiteral("{\"t\":\"busy\"}\n"));
            socket->disconnectFromHost();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            continue;
        }
        attachSocket(socket);
        m_idleTimer.start(kIdleTimeout);
        m_pingTimer.start();
        emit peerConnectedChanged();
    }
}

void LanSession::attachSocket(QTcpSocket* socket)
{
    m_socket = socket;
    m_buffer.clear();
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    connect(socket, &QTcpSocket::readyRead, this, &LanSession::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        if (socket != m_socket)
            return;
        dropSocket();
        emit peerLost();
    });
    connect(socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, [this, socket](QAbstractSocket::SocketError) {
        if (socket != m_socket)
            return;
        const bool wasConnecting = m_connectTimer.isActive();
        const QString reason = socket->errorString();
        m_connectTimer.stop();
        dropSocket();
        if (wasConnecting) {
            m_role = None;
            emit connectionFailed(reason);
        } else {
            emit peerLost();
        }
    });
}

void LanSession::dropSocket()
{
    m_pingTimer.stop();
    m_idleTimer.stop();
    m_buffer.clear();
    if (!m_socket)
        return;
    QTcpSocket* socket = m_socket;
    m_socket = nullptr;
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
}

void LanSession::readSocket()
{
    if (!m_socket)
        return;
    m_idleTimer.start(kIdleTimeout);
    m_buffer.append(m_socket->readAll());
    if (m_buffer.size() > kMaxBuffer) {
        dropSocket();
        emit peerLost();
        return;
    }
    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject())
            continue;
        const QVariantMap message = document.object().toVariantMap();
        if (message.value(QStringLiteral("t")).toString() == QLatin1String("ping"))
            continue;
        emit messageReceived(message);
        // A handler may have closed the session.
        if (!m_socket)
            return;
    }
}

void LanSession::answerDiscovery()
{
    while (m_responder && m_responder->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_responder->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_responder->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        if (datagram.trimmed() != kProbe || m_role != Host || m_socket)
            continue;
        m_responder->writeDatagram(kReplyPrefix + m_hostName.toUtf8(), sender, senderPort);
    }
}

void LanSession::sendDiscoveryProbe()
{
    if (!m_probe || m_probesLeft <= 0) {
        m_probeTimer.stop();
        return;
    }
    --m_probesLeft;
    m_probe->writeDatagram(kProbe, QHostAddress::Broadcast, DiscoveryPort);
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::CanBroadcast)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull())
                m_probe->writeDatagram(kProbe, entry.broadcast(), DiscoveryPort);
        }
    }
}

void LanSession::readDiscoveryReplies()
{
    while (m_probe && m_probe->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_probe->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_probe->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        if (!datagram.startsWith(kReplyPrefix))
            continue;
        const QString name = QString::fromUtf8(datagram.mid(kReplyPrefix.size())).trimmed();
        bool ok = false;
        const quint32 ipv4 = sender.toIPv4Address(&ok);
        emit hostDiscovered(ok ? QHostAddress(ipv4).toString() : sender.toString(), name);
    }
}
