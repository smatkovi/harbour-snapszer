#pragma once

#include "GameCore.h"

#include <QList>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class QSettings;
class LanSession;

class GameEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString roundResult READ roundResult NOTIFY stateChanged)
    Q_PROPERTY(bool roundOver READ roundOver NOTIFY stateChanged)
    Q_PROPERTY(bool matchOver READ matchOver NOTIFY stateChanged)
    Q_PROPERTY(int turnPlayer READ turnPlayer NOTIFY stateChanged)
    Q_PROPERTY(int dealer READ dealer NOTIFY stateChanged)
    Q_PROPERTY(int trumpSuit READ trumpSuit NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap trumpCard READ trumpCard NOTIFY stateChanged)
    Q_PROPERTY(bool hasTrumpCard READ hasTrumpCard NOTIFY stateChanged)
    Q_PROPERTY(int talonSize READ talonSize NOTIFY stateChanged)
    Q_PROPERTY(int faceDownStockSize READ faceDownStockSize NOTIFY stateChanged)
    Q_PROPERTY(bool talonClosed READ talonClosed NOTIFY stateChanged)
    Q_PROPERTY(bool strictPlay READ strictPlay NOTIFY stateChanged)
    Q_PROPERTY(QVariantList playerHand READ playerHand NOTIFY stateChanged)
    Q_PROPERTY(QVariantList cpuHand READ cpuHand NOTIFY stateChanged)
    Q_PROPERTY(QVariantList trickCards READ trickCards NOTIFY stateChanged)
    Q_PROPERTY(QVariantList playerWonCards READ playerWonCards NOTIFY stateChanged)
    Q_PROPERTY(QVariantList cpuWonCards READ cpuWonCards NOTIFY stateChanged)
    Q_PROPERTY(int playerPoints READ playerPoints NOTIFY stateChanged)
    Q_PROPERTY(int cpuPoints READ cpuPoints NOTIFY stateChanged)
    Q_PROPERTY(int playerGamePoints READ playerGamePoints NOTIFY stateChanged)
    Q_PROPERTY(int cpuGamePoints READ cpuGamePoints NOTIFY stateChanged)
    Q_PROPERTY(bool playerInputEnabled READ playerInputEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool canExchangeTrump READ canExchangeTrump NOTIFY stateChanged)
    Q_PROPERTY(bool canCloseTalon READ canCloseTalon NOTIFY stateChanged)
    Q_PROPERTY(bool canClaim66 READ canClaim66 NOTIFY stateChanged)
    Q_PROPERTY(int visualPhase READ visualPhase NOTIFY visualPhaseChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY settingsChanged)
    Q_PROPERTY(QString opponentName READ opponentName WRITE setOpponentName NOTIFY settingsChanged)
    Q_PROPERTY(QString cardStyle READ cardStyle WRITE setCardStyle NOTIFY settingsChanged)
    Q_PROPERTY(int aiDifficulty READ aiDifficulty WRITE setAiDifficulty NOTIFY settingsChanged)
    Q_PROPERTY(int aiPlayDelay READ aiPlayDelay WRITE setAiPlayDelay NOTIFY settingsChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(double animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY settingsChanged)
    Q_PROPERTY(QString opponentDisplayName READ opponentDisplayName NOTIFY stateChanged)

    Q_PROPERTY(bool networkGame READ networkGame NOTIFY networkChanged)
    Q_PROPERTY(bool lanGuest READ lanGuest NOTIFY networkChanged)
    Q_PROPERTY(bool lanBusy READ lanBusy NOTIFY networkChanged)
    Q_PROPERTY(QString networkStatus READ networkStatus NOTIFY networkChanged)
    Q_PROPERTY(QString lanAddress READ lanAddress WRITE setLanAddress NOTIFY settingsChanged)

public:
    explicit GameEngine(QObject* parent = nullptr);
    ~GameEngine() override;

    enum VisualPhase { Idle = 0, CardFlight = 1, TrickPause = 2, TrickFlight = 3, Deal = 4 };
    // Qt 5 only: moc 4.7 (MeeGo build) swallows the declaration that follows
    // a Q_ENUMS(), and QML never needs the enum by name.
#if QT_VERSION >= 0x050500
    Q_ENUM(VisualPhase)
#endif

    Q_INVOKABLE void start();
    Q_INVOKABLE void newMatch();
    Q_INVOKABLE void nextRound();
    Q_INVOKABLE void playCard(int handIndex, bool declareMarriage = false);
    Q_INVOKABLE bool isPlayerCardPlayable(int handIndex) const;
    Q_INVOKABLE int marriagePointsForCard(int handIndex) const;
    Q_INVOKABLE void exchangeTrump();
    Q_INVOKABLE void closeTalon();
    Q_INVOKABLE void claim66();
    Q_INVOKABLE void completeCardAnimation();
    Q_INVOKABLE void completeTrickAnimation();
    Q_INVOKABLE void completeDealAnimation();

    Q_INVOKABLE void hostLanGame();
    Q_INVOKABLE void joinLanGame(const QString& address);
    Q_INVOKABLE void cancelLan();

    QString status() const;
    QString roundResult() const;
    bool roundOver() const { return m_core.roundOver(); }
    bool matchOver() const { return m_core.matchOver(); }
    int turnPlayer() const { return m_core.turn(); }
    int dealer() const { return m_core.dealer(); }
    int trumpSuit() const { return m_core.trumpSuit(); }
    QVariantMap trumpCard() const;
    bool hasTrumpCard() const { return m_core.hasTrumpCard(); }
    int talonSize() const { return m_core.talonSize(); }
    int faceDownStockSize() const { return m_core.faceDownStockSize(); }
    bool talonClosed() const { return m_core.talonClosed(); }
    bool strictPlay() const { return m_core.strictPlay(); }
    QVariantList playerHand() const;
    QVariantList cpuHand() const;
    QVariantList trickCards() const;
    QVariantList playerWonCards() const;
    QVariantList cpuWonCards() const;
    int playerPoints() const { return m_core.totalPoints(0); }
    int cpuPoints() const { return m_core.totalPoints(1); }
    int playerGamePoints() const { return m_core.gamePoints(0); }
    int cpuGamePoints() const { return m_core.gamePoints(1); }
    bool playerInputEnabled() const;
    bool canExchangeTrump() const { return m_visualPhase == Idle && !m_awaitingHost && m_core.canExchangeTrump(0); }
    bool canCloseTalon() const { return m_visualPhase == Idle && !m_awaitingHost && m_core.canCloseTalon(0); }
    bool canClaim66() const { return m_visualPhase == Idle && !m_awaitingHost && m_core.canClaim66(0); }
    int visualPhase() const { return static_cast<int>(m_visualPhase); }
    bool paused() const { return m_paused; }
    void setPaused(bool value);

    QString playerName() const { return m_playerName; }
    QString opponentName() const { return m_opponentName; }
    QString cardStyle() const { return m_cardStyle; }
    int aiDifficulty() const { return m_aiDifficulty; }
    int aiPlayDelay() const { return m_aiPlayDelay; }
    bool animationsEnabled() const { return m_animationsEnabled; }
    double animationSpeed() const { return m_animationSpeed; }

    void setPlayerName(const QString& value);
    void setOpponentName(const QString& value);
    void setCardStyle(const QString& value);
    void setAiDifficulty(int value);
    void setAiPlayDelay(int value);
    void setAnimationsEnabled(bool value);
    void setAnimationSpeed(double value);

    QString opponentDisplayName() const;
    bool networkGame() const { return m_mode != Mode::Ai; }
    bool lanGuest() const { return m_mode == Mode::LanGuest; }
    bool lanBusy() const;
    QString networkStatus() const { return m_networkStatus; }
    QString lanAddress() const { return m_lanAddress; }
    void setLanAddress(const QString& value);

    // Shared with MultiEngine, which keeps its data in the same file.
    static QString settingsFilePath();

signals:
    void stateChanged();
    void settingsChanged();
    void networkChanged();
    void networkNotice(const QString& text);
    void resetVisuals();
    // The host at `address` runs a table of a different size; join it with
    // the engine for `players`.
    void lanRedirect(const QString& address, int players);
    void visualPhaseChanged();
    void pausedChanged();
    void cardAnimationRequested(const QString& cardId, int playedBy, int oldHandIndex);
    void trickAnimationRequested(int winnerPlayer);
    void dealAnimationRequested(const QVariantList& cards, int firstPlayer);

private slots:
    // Named slots rather than lambdas: the MeeGo build (Qt 4.7) connects by
    // signature and cannot connect to lambdas.
    void performAiMove();
    void onTrickPause();
    void recoverVisualTimeout();
    void onPeerConnectedChanged();
    void onPeerLost();
    void onConnectionFailed(const QString& reason);
    void onNetworkMessage(int peer, const QVariantMap& message);

private:
    // Ai: local match against the computer. LanHost: this device owns the
    // authoritative GameCore and the remote guest is player 1. LanGuest: the
    // core mirrors the host's state with players swapped, so the local player
    // is still player 0; local actions are sent to the host as requests and
    // only applied once the host echoes them back.
    enum class Mode { Ai, LanHost, LanGuest };

    static QString cardId(const Snapszer::Card& card);
    static QVariantMap cardMap(const Snapszer::Card& card);
    static QVariantList cardsToList(const std::vector<Snapszer::Card>& cards);
    QVariantList initialDealList() const;
    static QVariantList drawList(const std::vector<Snapszer::DrawnCard>& cards);

    void setVisualPhase(VisualPhase phase);
    bool startPlay(int player, int handIndex, bool declareMarriage);
    bool applyAction(int player, const QString& op);
    void scheduleAiMove();
    void beginTrickResolution();
    void finishIdle();
    void startDealAnimation(const QVariantList& cards, int firstPlayer);
    void loadSettings();
    void saveSettings();
    void persistGame();
    bool restoreGame();
    void clearSavedGame();
    void normalizeRestoredState();
    std::uint32_t freshSeed() const;

    void processRemoteQueue();
    void hostHandleRequest(const QVariantMap& message);
    void guestHandleMessage(const QVariantMap& message);
    void sendToGuest(const QString& type, QVariantMap message);
    void sendRequest(const QString& op, int handIndex = -1, bool declareMarriage = false);
    void sendSync();
    bool adoptRemoteState(const QVariant& encoded);
    void returnToAiGame(const QString& notice);

    Snapszer::GameCore m_core{1};
    QTimer m_aiTimer;
    QTimer m_trickPauseTimer;
    QTimer m_visualWatchdog;
    VisualPhase m_visualPhase = Idle;
    bool m_started = false;
    bool m_paused = false;
    bool m_freshGame = true;

    Mode m_mode = Mode::Ai;
    LanSession* m_session = nullptr;
    QList<QVariantMap> m_remoteQueue;
    bool m_processingRemote = false;
    bool m_awaitingHost = false;
    int m_netSeq = 0;
    QString m_remoteName;
    QString m_networkStatus;

    QString m_playerName = QStringLiteral("Player");
    QString m_opponentName = QStringLiteral("AI");
    QString m_cardStyle = QStringLiteral("Piatnik");
    int m_aiDifficulty = 1;
    int m_aiPlayDelay = 650;
    bool m_animationsEnabled = true;
    double m_animationSpeed = 1.0;
    QString m_lanAddress;
};
