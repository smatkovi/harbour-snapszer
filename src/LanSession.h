#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTimer>
#include <QVariantMap>

class QTcpSocket;
class QUdpSocket;

// Transport for a two-device LAN match. One device hosts (TCP server plus a
// UDP responder so guests can find it), the other joins. Messages are compact
// JSON objects, one per line. The session knows nothing about the game rules.
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
    bool peerConnected() const;
    QString peerAddress() const;

    bool startHosting(const QString& hostName, QString* error);
    void joinHost(const QString& address);
    void discoverHosts();
    void stop();
    void send(const QVariantMap& message);

    static QStringList localAddresses();

signals:
    void peerConnectedChanged();
    void messageReceived(const QVariantMap& message);
    void connectionFailed(const QString& reason);
    void peerLost();
    void hostDiscovered(const QString& address, const QString& name);

private:
    void acceptConnection();
    void attachSocket(QTcpSocket* socket);
    void dropSocket();
    void readSocket();
    void answerDiscovery();
    void readDiscoveryReplies();
    void sendDiscoveryProbe();
    void sendRaw(const QByteArray& line);

    Role m_role = None;
    QString m_hostName;
    QTcpServer m_server;
    QPointer<QTcpSocket> m_socket;
    QUdpSocket* m_responder = nullptr;
    QUdpSocket* m_probe = nullptr;
    QByteArray m_buffer;
    QTimer m_pingTimer;
    QTimer m_idleTimer;
    QTimer m_connectTimer;
    QTimer m_probeTimer;
    int m_probesLeft = 0;
};
