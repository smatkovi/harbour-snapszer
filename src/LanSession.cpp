#include "LanSession.h"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QUdpSocket>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

namespace {

// Version 1 is the two-player protocol of the first LAN release; hosts of a
// two-player game still answer it so older apps keep finding them.
const QByteArray kProbeV1 = QByteArrayLiteral("SNAPSZER-DISCOVER 1");
const QByteArray kProbeV2 = QByteArrayLiteral("SNAPSZER-DISCOVER 2");
const QByteArray kReplyV1 = QByteArrayLiteral("SNAPSZER-HOST 1 ");
const QByteArray kReplyV2 = QByteArrayLiteral("SNAPSZER-HOST 2 ");
const int kMaxBuffer = 256 * 1024;
const int kPingInterval = 10000;
// A peer that has sent nothing (not even a ping) for this long is gone, e.g.
// it left the WLAN. Kept generous because phones briefly stall on wake-up.
const qint64 kIdleTimeout = 45000;
const int kConnectTimeout = 8000;

#ifdef Q_OS_ANDROID
// Many Android Wi-Fi drivers drop broadcast packets unless an app holds a
// multicast lock, which would break automatic discovery.
QJniObject s_multicastLock;
int s_multicastUsers = 0;

void acquireMulticastLock()
{
    if (s_multicastUsers++ > 0)
        return;
    QJniObject context(QNativeInterface::QAndroidApplication::context().object());
    QJniObject service = QJniObject::fromString(QStringLiteral("wifi"));
    QJniObject manager = context.callObjectMethod("getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;", service.object<jstring>());
    if (!manager.isValid())
        return;
    QJniObject lock = manager.callObjectMethod("createMulticastLock",
            "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
            QJniObject::fromString(QStringLiteral("snapszer")).object<jstring>());
    if (!lock.isValid())
        return;
    lock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
    lock.callMethod<void>("acquire");
    s_multicastLock = lock;
}

void releaseMulticastLock()
{
    if (s_multicastUsers <= 0 || --s_multicastUsers > 0)
        return;
    if (s_multicastLock.isValid())
        s_multicastLock.callMethod<void>("release");
    s_multicastLock = QJniObject();
}
#else
void acquireMulticastLock() {}
void releaseMulticastLock() {}
#endif

// Newer Android versions hide most interface details from apps. Asking the
// kernel which source address it would use for an outside route still works
// and sends no packet.
QHostAddress routedAddress(const QString& outside)
{
    QUdpSocket probe;
    probe.connectToHost(QHostAddress(outside), 53);
    const QHostAddress address = probe.localAddress();
    probe.abort();
    return address;
}

QHostAddress routedLocalAddress()
{
    const QHostAddress address = routedAddress(QStringLiteral("8.8.8.8"));
    if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback())
        return QHostAddress();
    return address;
}

// Global unicast IPv6 (2000::/3), i.e. not link-local, unique-local or loopback.
bool isGlobalIPv6(const QHostAddress& address)
{
    if (address.protocol() != QAbstractSocket::IPv6Protocol)
        return false;
    const Q_IPV6ADDR bytes = address.toIPv6Address();
    return (bytes[0] & 0xe0) == 0x20;
}

QString plainAddress(const QHostAddress& address)
{
    bool ok = false;
    const quint32 ipv4 = address.toIPv4Address(&ok);
    return ok ? QHostAddress(ipv4).toString() : address.toString();
}

} // namespace

// --- LanSession ------------------------------------------------------------------

LanSession::LanSession(QObject* parent)
    : QObject(parent)
{
    m_pingTimer.setInterval(kPingInterval);
    m_connectTimer.setSingleShot(true);

    connect(&m_server, &QTcpServer::newConnection, this, &LanSession::acceptConnections);
    connect(&m_pingTimer, &QTimer::timeout, this, [this]() {
        const QByteArray ping = QByteArrayLiteral("{\"t\":\"ping\"}\n");
        for (const Peer& peer : m_peers)
            peer.socket->write(ping);
        checkIdlePeers();
    });
    connect(&m_connectTimer, &QTimer::timeout, this, [this]() {
        if (m_role != Guest || peerConnected())
            return;
        stop();
        emit connectionFailed(tr("No answer from that address"));
    });
}

LanSession::~LanSession()
{
    // Receivers may already be half-destroyed during application shutdown.
    blockSignals(true);
    stop();
}

bool LanSession::startHosting(const QString& hostName, int players, int maxPeers, QString* error)
{
    stop();
    m_hostName = hostName;
    m_players = players;
    m_maxPeers = maxPeers;
    m_accepting = true;
    if (!m_server.listen(QHostAddress::Any, GamePort)) {
        if (error)
            *error = m_server.errorString();
        return false;
    }
    m_role = Host;
    acquireMulticastLock();
    m_pingTimer.start();

    m_responder = new QUdpSocket(this);
    if (m_responder->bind(QHostAddress(QHostAddress::AnyIPv4), DiscoveryPort,
                          QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        connect(m_responder, &QUdpSocket::readyRead, this, &LanSession::answerDiscovery);
    }
    // A failed responder only disables automatic discovery; joining by
    // address still works, so hosting continues.
    return true;
}

void LanSession::setAcceptingGuests(bool accepting)
{
    m_accepting = accepting;
}

void LanSession::joinHost(const QString& address)
{
    stop();
    m_role = Guest;
    QTcpSocket* socket = new QTcpSocket(this);
    m_pendingSocket = socket;
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        m_connectTimer.stop();
        m_pendingSocket = nullptr;
        m_nextPeerId = 0;
        addPeer(socket);
        m_pingTimer.start();
        emit peerJoined(0);
        emit peerConnectedChanged();
    });
    auto onError = [this, socket](QAbstractSocket::SocketError) {
        if (!m_connectTimer.isActive() || peerConnected())
            return;
        const QString reason = socket->errorString();
        stop();
        emit connectionFailed(reason);
    };
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(socket, &QAbstractSocket::errorOccurred, this, onError);
#else
    connect(socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, onError);
#endif
    m_connectTimer.start(kConnectTimeout);
    socket->connectToHost(normalizeAddress(address), GamePort);
}

void LanSession::stop()
{
    const bool hadPeers = peerConnected();
    const bool wasActive = m_role != None;
    m_connectTimer.stop();
    m_pingTimer.stop();
    if (m_pendingSocket) {
        m_pendingSocket->disconnect(this);
        m_pendingSocket->abort();
        m_pendingSocket->deleteLater();
        m_pendingSocket = nullptr;
    }
    while (!m_peers.isEmpty()) {
        // Detach first: a synchronous disconnected() during flush() must not
        // re-enter removePeer() or emit peerLost while shutting down.
        const Peer peer = m_peers.takeFirst();
        peer.socket->disconnect(this);
        peer.socket->flush();
        peer.socket->abort();
        peer.socket->deleteLater();
    }
    m_server.close();
    if (m_responder) {
        m_responder->close();
        m_responder->deleteLater();
        m_responder = nullptr;
    }
    if (m_role == Host && wasActive)
        releaseMulticastLock();
    m_role = None;
    if (hadPeers)
        emit peerConnectedChanged();
}

void LanSession::writeLine(QTcpSocket* socket, const QVariantMap& message)
{
    QByteArray line = QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact);
    line.append('\n');
    socket->write(line);
}

void LanSession::send(const QVariantMap& message)
{
    for (const Peer& peer : m_peers)
        writeLine(peer.socket, message);
}

void LanSession::sendTo(int peer, const QVariantMap& message)
{
    if (Peer* target = findPeer(peer))
        writeLine(target->socket, message);
}

void LanSession::dropPeer(int peer)
{
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].id != peer)
            continue;
        // Close gracefully so a last message (e.g. why the peer is dropped)
        // still reaches it.
        QTcpSocket* socket = m_peers[i].socket;
        m_peers.removeAt(i);
        socket->disconnect(this);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        socket->disconnectFromHost();
        if (socket->state() == QAbstractSocket::UnconnectedState)
            socket->deleteLater();
        emit peerConnectedChanged();
        return;
    }
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
    if (result.isEmpty()) {
        const QHostAddress routed = routedLocalAddress();
        if (!routed.isNull())
            result.append(routed.toString());
    }
    return result;
}

QStringList LanSession::internetAddresses()
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
            if (isGlobalIPv6(entry.ip()))
                result.append(entry.ip().toString());
        }
    }
    const QHostAddress routed = routedAddress(QStringLiteral("2001:4860:4860::8888"));
    if (isGlobalIPv6(routed) && !result.contains(routed.toString()))
        result.prepend(routed.toString()); // the address actually used for outgoing traffic
    while (result.size() > 3)
        result.removeLast();
    return result;
}

QString LanSession::normalizeAddress(const QString& address)
{
    QString result = address.trimmed();
    if (result.startsWith(QLatin1Char('[')) && result.endsWith(QLatin1Char(']')))
        result = result.mid(1, result.size() - 2);
    return result;
}

void LanSession::acceptConnections()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket* socket = m_server.nextPendingConnection();
        if (!m_accepting || m_peers.size() >= m_maxPeers) {
            QVariantMap busy;
            busy.insert(QStringLiteral("t"), QStringLiteral("busy"));
            writeLine(socket, busy);
            socket->disconnectFromHost();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            continue;
        }
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        const Peer* peer = addPeer(socket);
        emit peerJoined(peer->id);
        emit peerConnectedChanged();
    }
}

LanSession::Peer* LanSession::addPeer(QTcpSocket* socket)
{
    Peer peer;
    peer.id = m_nextPeerId++;
    peer.socket = socket;
    peer.lastSeen = QDateTime::currentMSecsSinceEpoch();
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    const int id = peer.id;
    connect(socket, &QTcpSocket::readyRead, this, [this, id]() { readPeer(id); });
    connect(socket, &QTcpSocket::disconnected, this, [this, id]() {
        if (!findPeer(id))
            return;
        removePeer(id, true);
    });
    m_peers.append(peer);
    return &m_peers.last();
}

LanSession::Peer* LanSession::findPeer(int id)
{
    for (Peer& peer : m_peers) {
        if (peer.id == id)
            return &peer;
    }
    return nullptr;
}

void LanSession::removePeer(int id, bool notify)
{
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].id != id)
            continue;
        QTcpSocket* socket = m_peers[i].socket;
        m_peers.removeAt(i);
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
        if (notify) {
            emit peerLost(id);
            emit peerConnectedChanged();
        }
        return;
    }
}

void LanSession::checkIdlePeers()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<int> idle;
    for (const Peer& peer : m_peers) {
        if (now - peer.lastSeen > kIdleTimeout)
            idle.append(peer.id);
    }
    for (int id : idle)
        removePeer(id, true);
}

void LanSession::readPeer(int id)
{
    Peer* peer = findPeer(id);
    if (!peer)
        return;
    peer->lastSeen = QDateTime::currentMSecsSinceEpoch();
    peer->buffer.append(peer->socket->readAll());
    if (peer->buffer.size() > kMaxBuffer) {
        removePeer(id, true);
        return;
    }
    while (true) {
        peer = findPeer(id); // a handler may have removed it or changed the list
        if (!peer)
            return;
        const int newline = peer->buffer.indexOf('\n');
        if (newline < 0)
            return;
        const QByteArray line = peer->buffer.left(newline);
        peer->buffer.remove(0, newline + 1);
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject())
            continue;
        const QVariantMap message = document.object().toVariantMap();
        if (message.value(QStringLiteral("t")).toString() == QLatin1String("ping"))
            continue;
        emit messageReceived(id, message);
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
        const int openSeats = m_accepting ? m_maxPeers - m_peers.size() : 0;
        if (m_role != Host || openSeats <= 0)
            continue;
        datagram = datagram.trimmed();
        if (datagram == kProbeV2) {
            m_responder->writeDatagram(kReplyV2 + QByteArray::number(m_players) + ' '
                                       + QByteArray::number(openSeats) + ' ' + m_hostName.toUtf8(),
                                       sender, senderPort);
        } else if (datagram == kProbeV1 && m_players == 2) {
            m_responder->writeDatagram(kReplyV1 + m_hostName.toUtf8(), sender, senderPort);
        }
    }
}

// --- LanBrowser ------------------------------------------------------------------

LanBrowser::LanBrowser(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(600);
    m_finishTimer.setSingleShot(true);
    m_finishTimer.setInterval(900);
    connect(&m_timer, &QTimer::timeout, this, &LanBrowser::sendProbes);
    connect(&m_finishTimer, &QTimer::timeout, this, &LanBrowser::finish);
}

LanBrowser::~LanBrowser()
{
    if (m_lockHeld)
        releaseMulticastLock();
}

bool LanBrowser::ensureSocket()
{
    if (m_socket)
        return true;
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress(QHostAddress::AnyIPv4), 0)) {
        m_socket->deleteLater();
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &LanBrowser::readReplies);
    return true;
}

void LanBrowser::copyToClipboard(const QString& text)
{
    if (QClipboard* clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void LanBrowser::search()
{
    m_hosts.clear();
    emit hostsChanged();
    if (!ensureSocket())
        return;
    if (!m_lockHeld) {
        acquireMulticastLock();
        m_lockHeld = true;
    }
    m_finishTimer.stop();
    m_probesLeft = 5;
    if (!m_searching) {
        m_searching = true;
        emit searchingChanged();
    }
    sendProbes();
    m_timer.start();
}

void LanBrowser::sendProbes()
{
    if (!m_socket || m_probesLeft <= 0)
        return;
    --m_probesLeft;
    auto sendTo = [this](const QHostAddress& target) {
        m_socket->writeDatagram(kProbeV2, target, LanSession::DiscoveryPort);
        m_socket->writeDatagram(kProbeV1, target, LanSession::DiscoveryPort);
    };
    {
        sendTo(QHostAddress(QHostAddress::Broadcast));
        bool directed = false;
        const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& iface : interfaces) {
            const QNetworkInterface::InterfaceFlags flags = iface.flags();
            if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::CanBroadcast)
                || (flags & QNetworkInterface::IsLoopBack))
                continue;
            const QList<QNetworkAddressEntry> entries = iface.addressEntries();
            for (const QNetworkAddressEntry& entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull()) {
                    sendTo(entry.broadcast());
                    directed = true;
                }
            }
        }
        if (!directed) {
            // No interface details available: assume the usual /24 home network.
            const QHostAddress routed = routedLocalAddress();
            if (!routed.isNull())
                sendTo(QHostAddress((routed.toIPv4Address() & 0xffffff00U) | 0xffU));
        }
    }
    if (m_probesLeft <= 0) {
        m_timer.stop();
        m_finishTimer.start(); // leave time for the last answers
    }
}

void LanBrowser::finish()
{
    if (m_lockHeld) {
        releaseMulticastLock();
        m_lockHeld = false;
    }
    if (m_searching) {
        m_searching = false;
        emit searchingChanged();
    }
}

void LanBrowser::readReplies()
{
    const QStringList own = LanSession::localAddresses();
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        int players = 2;
        int openSeats = 1;
        QString name;
        if (datagram.startsWith(kReplyV2)) {
            const QList<QByteArray> parts = datagram.mid(kReplyV2.size()).split(' ');
            if (parts.size() < 3)
                continue;
            players = parts[0].toInt();
            openSeats = parts[1].toInt();
            name = QString::fromUtf8(datagram.mid(kReplyV2.size() + parts[0].size() + parts[1].size() + 2)).trimmed();
        } else if (datagram.startsWith(kReplyV1)) {
            name = QString::fromUtf8(datagram.mid(kReplyV1.size())).trimmed();
        } else {
            continue;
        }
        const QString address = plainAddress(sender);
        if (own.contains(address))
            continue;
        bool known = false;
        for (int i = 0; i < m_hosts.size(); ++i) {
            QVariantMap host = m_hosts[i].toMap();
            if (host.value(QStringLiteral("address")).toString() != address)
                continue;
            known = true;
            // A version 2 answer carries more detail than a version 1 one.
            if (datagram.startsWith(kReplyV2)) {
                host.insert(QStringLiteral("players"), players);
                host.insert(QStringLiteral("openSeats"), openSeats);
                m_hosts[i] = host;
            }
        }
        if (!known) {
            QVariantMap host;
            host.insert(QStringLiteral("address"), address);
            host.insert(QStringLiteral("name"), name.left(32));
            host.insert(QStringLiteral("players"), players);
            host.insert(QStringLiteral("openSeats"), openSeats);
            m_hosts.append(host);
        }
        emit hostsChanged();
    }
}
