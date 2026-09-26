#pragma once

#include "MultiCore.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class GameEngine;
class LanSession;

// QML facade for the three- and four-player games. Seat 0 is always the
// local player; the other seats are computer players or, in a LAN match,
// people on other devices. As in the two-player LAN game, the host's core is
// authoritative: the host sends every action together with the state it was
// applied to, and guests replay it on a copy rotated to their own seat.
class MultiEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool canResume READ canResume NOTIFY stateChanged)
    Q_PROPERTY(int players READ players NOTIFY stateChanged)
    Q_PROPERTY(int variant READ variant NOTIFY stateChanged)
    Q_PROPERTY(QString rulesName READ rulesName NOTIFY stateChanged)
    Q_PROPERTY(int phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QVariantList seats READ seats NOTIFY stateChanged)
    Q_PROPERTY(QVariantList hand READ hand NOTIFY stateChanged)
    Q_PROPERTY(QVariantList trick READ trick NOTIFY stateChanged)
    Q_PROPERTY(int trumpSuit READ trumpSuit NOTIFY stateChanged)
    Q_PROPERTY(QString calledCard READ calledCard NOTIFY stateChanged)
    Q_PROPERTY(QString contractName READ contractName NOTIFY stateChanged)
    Q_PROPERTY(QVariantList options READ options NOTIFY stateChanged)
    Q_PROPERTY(bool choosingCard READ choosingCard NOTIFY stateChanged)
    Q_PROPERTY(bool discarding READ discarding NOTIFY stateChanged)
    Q_PROPERTY(bool canClaim READ canClaim NOTIFY stateChanged)
    Q_PROPERTY(bool roundOver READ roundOver NOTIFY stateChanged)
    Q_PROPERTY(bool matchOver READ matchOver NOTIFY stateChanged)
    Q_PROPERTY(QString roundResult READ roundResult NOTIFY stateChanged)
    Q_PROPERTY(int visualPhase READ visualPhase NOTIFY visualPhaseChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

    Q_PROPERTY(int rules3 READ rules3 WRITE setRules3 NOTIFY settingsChanged)
    Q_PROPERTY(int rules4 READ rules4 WRITE setRules4 NOTIFY settingsChanged)

    Q_PROPERTY(bool networkGame READ networkGame NOTIFY networkChanged)
    Q_PROPERTY(bool lanGuest READ lanGuest NOTIFY networkChanged)
    Q_PROPERTY(bool lanHosting READ lanHosting NOTIFY networkChanged)
    Q_PROPERTY(bool lanBusy READ lanBusy NOTIFY networkChanged)
    Q_PROPERTY(QString networkStatus READ networkStatus NOTIFY networkChanged)
    // Whether guests can come over Bluetooth while hosting, and why not.
    Q_PROPERTY(bool bluetoothHosting READ bluetoothHosting NOTIFY networkChanged)
    Q_PROPERTY(QString bluetoothError READ bluetoothError NOTIFY networkChanged)
    Q_PROPERTY(QVariantList lobby READ lobby NOTIFY networkChanged)

public:
    enum VisualPhase { Idle = 0, CardFlight = 1, TrickPause = 2, TrickFlight = 3 };
    // Qt 5 only: moc 4.7 (MeeGo build) swallows the declaration that follows
    // a Q_ENUMS(), and QML never needs the enum by name.
#if QT_VERSION >= 0x050500
    Q_ENUM(VisualPhase)
#endif

    explicit MultiEngine(GameEngine* settings, QObject* parent = nullptr);
    ~MultiEngine() override;

    Q_INVOKABLE void startMatch(int players);
    Q_INVOKABLE void resume();
    Q_INVOKABLE void act(const QString& type, int value = -1, bool marriage = false);
    Q_INVOKABLE void claim();
    Q_INVOKABLE void nextRound();
    Q_INVOKABLE void newMatch();
    Q_INVOKABLE void completeCardAnimation();
    Q_INVOKABLE void completeTrickAnimation();
    Q_INVOKABLE QString rulesNameFor(int players, int rules) const;
    Q_INVOKABLE QString contractLabel(int contract) const;

    Q_INVOKABLE void hostLanGame(int players);
    Q_INVOKABLE void startLanMatch();
    Q_INVOKABLE void joinLanGame(const QString& address);
    // Same table, reached over Bluetooth: `address` is a device address.
    Q_INVOKABLE void joinBluetoothGame(const QString& address);
    Q_INVOKABLE void cancelLan();

    bool active() const { return m_active; }
    bool canResume() const;
    int players() const { return m_core.players(); }
    int variant() const { return static_cast<int>(m_core.variant()); }
    QString rulesName() const;
    int phase() const { return static_cast<int>(m_core.phase()); }
    QString status() const;
    QVariantList seats() const;
    QVariantList hand() const;
    QVariantList trick() const;
    int trumpSuit() const { return m_core.trumpSuit(); }
    QString calledCard() const;
    QString contractName() const;
    QVariantList options() const;
    bool choosingCard() const;
    bool discarding() const;
    bool canClaim() const;
    bool roundOver() const { return m_active && m_core.roundOver(); }
    bool matchOver() const { return m_active && m_core.matchOver(); }
    QString roundResult() const;
    int visualPhase() const { return static_cast<int>(m_visualPhase); }
    bool paused() const { return m_paused; }
    void setPaused(bool value);

    int rules3() const { return m_rules3; }
    int rules4() const { return m_rules4; }
    void setRules3(int value);
    void setRules4(int value);

    bool networkGame() const { return m_mode != Mode::Local; }
    bool lanGuest() const { return m_mode == Mode::Guest; }
    bool lanHosting() const { return m_lobbyOpen; }
    bool lanBusy() const;
    QString networkStatus() const { return m_networkStatus; }
    bool bluetoothHosting() const;
    QString bluetoothError() const;
    QVariantList lobby() const;

    // Test hook: the seat that joined players occupy in the host's numbering.
    int localSeatOnHost() const { return m_hostSeat; }
    const Snapszer::MultiCore& core() const { return m_core; }

signals:
    void stateChanged();
    void settingsChanged();
    void visualPhaseChanged();
    void pausedChanged();
    void networkChanged();
    void networkNotice(const QString& text);
    void resetVisuals();
    void matchStarted();
    // The host at `address` runs a table of a different size; join it with
    // the engine for `players`.
    void lanRedirect(const QString& address, int players);
    // The same for a table joined over Bluetooth; see GameEngine.
    void btRedirect(const QString& address, int players);
    void cardAnimationRequested(const QString& cardId, int seat);
    void trickAnimationRequested(int winnerSeat);

private slots:
    // Named slots rather than lambdas: the MeeGo build (Qt 4.7) connects by
    // signature and cannot connect to lambdas.
    void runComputer();
    void onTrickPause();
    void recoverVisualTimeout();
    void onPeerJoined(int peer);
    void onPeerLost(int peer);
    void onConnectionFailed(const QString& reason);
    void onMessage(int peer, const QVariantMap& message);

private:
    enum class Mode { Local, Host, Guest };

    static QString cardId(const Snapszer::Card& card);
    static QString cardId(int key);
    QString seatName(int seat) const;
    Snapszer::MultiVariant variantFor(int players) const;
    bool seatIsComputer(int seat) const;
    bool localDecision() const;

    bool perform(int seat, const Snapszer::MultiAction& action);
    void setVisualPhase(VisualPhase phase);
    void finishIdle();
    void scheduleComputer();

    void loadSettings();
    void saveSettings();
    void persist();

    // LAN
    void processRemoteQueue();
    void hostHandle(int peer, const QVariantMap& message);
    void guestHandle(const QVariantMap& message);
    void sendLobby();
    void sendToGuests(const QString& type, QVariantMap message);
    void sendRequest(const Snapszer::MultiAction& action);
    bool adoptState(const QVariant& encoded);
    void leaveNetwork(const QString& notice);
    QString localName() const;

    GameEngine* m_settings;
    Snapszer::MultiCore m_core;
    bool m_active = false;
    bool m_paused = false;
    VisualPhase m_visualPhase = Idle;
    QTimer m_aiTimer;
    QTimer m_trickPauseTimer;
    QTimer m_watchdog;
    int m_rules3 = 0;
    int m_rules4 = 1;

    Mode m_mode = Mode::Local;
    LanSession* m_session = nullptr;
    bool m_lobbyOpen = false;
    int m_hostPlayers = 3;
    QList<int> m_seatPeers;      // host: peer id per seat, -1 local or computer
    QStringList m_seatNames;     // in local seat numbering
    QList<bool> m_seatHuman;     // in local seat numbering
    int m_hostSeat = 0;          // guest: my seat in the host's numbering
    QList<QPair<int, QVariantMap>> m_remoteQueue;
    bool m_processingRemote = false;
    bool m_awaitingHost = false;
    int m_netSeq = 0;
    QString m_networkStatus;
    QString m_joinAddress;
    bool m_joinOverBluetooth = false;
};
