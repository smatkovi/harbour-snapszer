#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class QTcpSocket;
class QUdpSocket;

// Transport for LAN matches. One device hosts (TCP server plus a UDP
// responder so other devices can find it), the others join. Messages are
// compact JSON objects, one per line. The session knows nothing about the
// game rules. A host accepts up to `maxPeers` guests; a guest has exactly one
// peer, the host, with id 0.
class LanSession : public QObject
{
    Q_OBJECT

public:
    enum Role { None = 0, Host = 1, Guest = 2 };

    static const quint16 GamePort = 45465;
    static const quint16 DiscoveryPort = 45466;

    explicit LanSession(QObject* parent = nullptr);
    ~LanSession() override;

    Role role() const { return m_role; }
    bool peerConnected() const { return !m_peers.isEmpty(); }
    int peerCount() const { return m_peers.size(); }

    // `players` is the table size shown to searching devices (2 for the
    // classic game), `maxPeers` the number of guests accepted.
    bool startHosting(const QString& hostName, int players, int maxPeers, QString* error);
    void setAcceptingGuests(bool accepting);
    void joinHost(const QString& address);
    void stop();
    void send(const QVariantMap& message);
    void sendTo(int peer, const QVariantMap& message);
    void dropPeer(int peer);

    static QStringList localAddresses();

signals:
    void peerConnectedChanged();
    void peerJoined(int peer);
    void peerLost(int peer);
    void messageReceived(int peer, const QVariantMap& message);
    void connectionFailed(const QString& reason);

private:
    struct Peer {
        int id = -1;
        QTcpSocket* socket = nullptr;
        QByteArray buffer;
        qint64 lastSeen = 0;
    };

    void acceptConnections();
    Peer* addPeer(QTcpSocket* socket);
    Peer* findPeer(int id);
    void removePeer(int id, bool notify);
    void readPeer(int id);
    void checkIdlePeers();
    void answerDiscovery();
    static void writeLine(QTcpSocket* socket, const QVariantMap& message);

    Role m_role = None;
    QString m_hostName;
    int m_players = 2;
    int m_maxPeers = 1;
    bool m_accepting = true;
    int m_nextPeerId = 0;
    QTcpServer m_server;
    QList<Peer> m_peers;
    QUdpSocket* m_responder = nullptr;
    QTcpSocket* m_pendingSocket = nullptr;
    QTimer m_pingTimer;
    QTimer m_connectTimer;
};

// Finds hosted games in the local network by UDP broadcast, or asks a single
// address directly.
class LanBrowser : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList hosts READ hosts NOTIFY hostsChanged)
    Q_PROPERTY(QString localAddresses READ localAddresses NOTIFY hostsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)

public:
    explicit LanBrowser(QObject* parent = nullptr);
    ~LanBrowser() override;

    QVariantList hosts() const { return m_hosts; }
    QString localAddresses() const { return LanSession::localAddresses().join(QStringLiteral(", ")); }
    bool searching() const { return m_searching; }

    Q_INVOKABLE void search();
    Q_INVOKABLE void probe(const QString& address);

signals:
    void hostsChanged();
    void searchingChanged();
    void hostFound(const QString& address, const QString& name, int players, int openSeats);

private:
    bool ensureSocket();
    void startProbing();
    void sendProbes();
    void readReplies();
    void finish();

    QUdpSocket* m_socket = nullptr;
    QTimer m_timer;
    QTimer m_finishTimer;
    int m_probesLeft = 0;
    bool m_searching = false;
    bool m_lockHeld = false;
    QString m_directAddress;
    QVariantList m_hosts;
};
