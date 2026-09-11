#include "GameEngine.h"
#include "LanSession.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>

#include <algorithm>

using Snapszer::AiAction;
using Snapszer::AiActionType;
using Snapszer::AiDifficulty;
using Snapszer::RoundEndReason;

namespace {
const int kLanProtocolVersion = 1;
}

GameEngine::GameEngine(QObject* parent)
    : QObject(parent)
{
    m_aiTimer.setSingleShot(true);
    m_trickPauseTimer.setSingleShot(true);
    m_visualWatchdog.setSingleShot(true);
    connect(&m_aiTimer, &QTimer::timeout, this, &GameEngine::performAiMove);
    connect(&m_trickPauseTimer, &QTimer::timeout, this, [this]() {
        if (m_visualPhase != TrickPause || !m_core.trickPending())
            return;
        setVisualPhase(TrickFlight);
        emit stateChanged();
        emit trickAnimationRequested(m_core.pendingTrickWinner());
        if (!m_animationsEnabled)
            QTimer::singleShot(0, this, &GameEngine::completeTrickAnimation);
        else
            m_visualWatchdog.start(7000);
    });
    connect(&m_visualWatchdog, &QTimer::timeout, this, &GameEngine::recoverVisualTimeout);

    m_session = new LanSession(this);
    connect(m_session, &LanSession::peerJoined, this, &GameEngine::onPeerConnectedChanged);
    connect(m_session, &LanSession::peerLost, this, &GameEngine::onPeerLost);
    connect(m_session, &LanSession::connectionFailed, this, &GameEngine::onConnectionFailed);
    connect(m_session, &LanSession::messageReceived, this,
            [this](int, const QVariantMap& message) { onNetworkMessage(message); });

    loadSettings();
    m_freshGame = !restoreGame();
    if (m_freshGame)
        m_core.newMatch(freshSeed());
    else
        normalizeRestoredState();
}

GameEngine::~GameEngine()
{
    persistGame();
}

std::uint32_t GameEngine::freshSeed() const
{
    return static_cast<std::uint32_t>(QDateTime::currentMSecsSinceEpoch() & UINT64_C(0xffffffff));
}

QString GameEngine::settingsFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + QStringLiteral("/.config/org.edp17/harbour-snapszer");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/harbour-snapszer.conf");
}

void GameEngine::loadSettings()
{
    QSettings settings(settingsFilePath(), QSettings::NativeFormat);
    m_playerName = settings.value(QStringLiteral("ui/playerName"), QStringLiteral("Player")).toString();
    m_opponentName = settings.value(QStringLiteral("ui/opponentName"), QStringLiteral("AI")).toString();
    m_cardStyle = settings.value(QStringLiteral("ui/cardStyle"), QStringLiteral("Piatnik")).toString();
    if (m_cardStyle != QStringLiteral("Piatnik") && m_cardStyle != QStringLiteral("Betyar"))
        m_cardStyle = QStringLiteral("Piatnik");
    m_aiDifficulty = qBound(0, settings.value(QStringLiteral("ai/difficulty"), 1).toInt(), 3);
    m_aiPlayDelay = qBound(150, settings.value(QStringLiteral("ai/playDelay"), 650).toInt(), 2000);
    m_animationsEnabled = settings.value(QStringLiteral("ui/animationsEnabled"), true).toBool();
    m_animationSpeed = qBound(0.5, settings.value(QStringLiteral("ui/animationSpeed"), 1.0).toDouble(), 2.0);
    m_lanAddress = settings.value(QStringLiteral("lan/lastAddress")).toString();
}

void GameEngine::saveSettings()
{
    QSettings settings(settingsFilePath(), QSettings::NativeFormat);
    settings.setValue(QStringLiteral("ui/playerName"), m_playerName);
    settings.setValue(QStringLiteral("ui/opponentName"), m_opponentName);
    settings.setValue(QStringLiteral("ui/cardStyle"), m_cardStyle);
    settings.setValue(QStringLiteral("ai/difficulty"), m_aiDifficulty);
    settings.setValue(QStringLiteral("ai/playDelay"), m_aiPlayDelay);
    settings.setValue(QStringLiteral("ui/animationsEnabled"), m_animationsEnabled);
    settings.setValue(QStringLiteral("ui/animationSpeed"), m_animationSpeed);
    settings.setValue(QStringLiteral("lan/lastAddress"), m_lanAddress);
    settings.sync();
}

void GameEngine::setPlayerName(const QString& value)
{
    // Keep the live editor value exactly as entered. In particular, an empty
    // intermediate value must remain empty while the user replaces the whole
    // name; otherwise the bound TextField immediately jumps back to "Player".
    if (value == m_playerName) return;
    m_playerName = value; saveSettings(); emit settingsChanged(); emit stateChanged();
}

void GameEngine::setOpponentName(const QString& value)
{
    // See setPlayerName(): empty text is a valid editing state and must not be
    // replaced with the default "AI" while the TextField has focus.
    if (value == m_opponentName) return;
    m_opponentName = value; saveSettings(); emit settingsChanged(); emit stateChanged();
}

void GameEngine::setCardStyle(const QString& value)
{
    const QString v = value == QStringLiteral("Betyar") ? QStringLiteral("Betyar") : QStringLiteral("Piatnik");
    if (v == m_cardStyle) return;
    m_cardStyle = v; saveSettings(); emit settingsChanged(); emit stateChanged();
}

void GameEngine::setAiDifficulty(int value)
{
    value = qBound(0, value, 3);
    if (value == m_aiDifficulty) return;
    m_aiDifficulty = value; saveSettings(); emit settingsChanged();
}

void GameEngine::setAiPlayDelay(int value)
{
    value = qBound(150, value, 2000);
    if (value == m_aiPlayDelay) return;
    m_aiPlayDelay = value; saveSettings(); emit settingsChanged();
}

void GameEngine::setAnimationsEnabled(bool value)
{
    if (value == m_animationsEnabled) return;
    m_animationsEnabled = value; saveSettings(); emit settingsChanged();
}

void GameEngine::setAnimationSpeed(double value)
{
    value = qBound(0.5, value, 2.0);
    if (qFuzzyCompare(value, m_animationSpeed)) return;
    m_animationSpeed = value; saveSettings(); emit settingsChanged();
}

void GameEngine::setLanAddress(const QString& value)
{
    if (value == m_lanAddress) return;
    m_lanAddress = value; saveSettings(); emit settingsChanged();
}

QString GameEngine::opponentDisplayName() const
{
    return networkGame() ? m_remoteName : m_opponentName;
}

bool GameEngine::lanBusy() const
{
    return m_mode == Mode::Ai && m_session->role() != LanSession::None;
}

QString GameEngine::cardId(const Snapszer::Card& card)
{
    return QString::number(card.suit) + QLatin1Char('_') + QString::number(card.rank);
}

QVariantMap GameEngine::cardMap(const Snapszer::Card& card)
{
    QVariantMap result;
    result.insert(QStringLiteral("id"), cardId(card));
    result.insert(QStringLiteral("suit"), card.suit);
    result.insert(QStringLiteral("rank"), card.rank);
    result.insert(QStringLiteral("points"), card.points());
    result.insert(QStringLiteral("playedBy"), card.playedBy);
    return result;
}

QVariantList GameEngine::cardsToList(const std::vector<Snapszer::Card>& cards)
{
    QVariantList result;
    for (const Snapszer::Card& card : cards)
        result.append(cardMap(card));
    return result;
}

QVariantList GameEngine::playerHand() const { return cardsToList(m_core.hand(0)); }
QVariantList GameEngine::cpuHand() const { return cardsToList(m_core.hand(1)); }
QVariantList GameEngine::trickCards() const { return cardsToList(m_core.trick()); }
QVariantList GameEngine::playerWonCards() const { return cardsToList(m_core.wonCards(0)); }
QVariantList GameEngine::cpuWonCards() const { return cardsToList(m_core.wonCards(1)); }

QVariantMap GameEngine::trumpCard() const
{
    return m_core.hasTrumpCard() ? cardMap(m_core.trumpCard()) : QVariantMap();
}

QVariantList GameEngine::initialDealList() const
{
    QVariantList result;
    int order = 0;
    for (int player = 0; player < 2; ++player) {
        for (const Snapszer::Card& card : m_core.hand(player)) {
            QVariantMap value = cardMap(card);
            value.insert(QStringLiteral("player"), player);
            value.insert(QStringLiteral("order"), order++);
            result.append(value);
        }
    }
    return result;
}

QVariantList GameEngine::drawList(const std::vector<Snapszer::DrawnCard>& cards)
{
    QVariantList result;
    for (const Snapszer::DrawnCard& draw : cards) {
        QVariantMap value = cardMap(draw.card);
        value.insert(QStringLiteral("player"), draw.player);
        value.insert(QStringLiteral("order"), draw.order);
        result.append(value);
    }
    return result;
}

QString GameEngine::status() const
{
    if (m_core.matchOver())
        return m_core.matchWinner() == 0 ? tr("You won the match") : tr("%1 won the match").arg(opponentDisplayName());
    if (m_core.roundOver())
        return tr("Round finished");
    if (m_visualPhase != Idle)
        return tr("Cards are moving…");
    if (m_core.talonClosed())
        return m_core.turn() == 0 ? tr("Your turn — talon closed") : tr("%1's turn — talon closed").arg(opponentDisplayName());
    if (m_core.strictPlay())
        return m_core.turn() == 0 ? tr("Your turn — strict play") : tr("%1's turn — strict play").arg(opponentDisplayName());
    return m_core.turn() == 0 ? tr("Your turn") : tr("%1's turn").arg(opponentDisplayName());
}

QString GameEngine::roundResult() const
{
    if (!m_core.roundOver())
        return QString();
    const QString winner = m_core.roundWinner() == 0 ? m_playerName : opponentDisplayName();
    QString reason;
    switch (m_core.roundEndReason()) {
    case RoundEndReason::Claim66: reason = tr("66 reached"); break;
    case RoundEndReason::LastTrick: reason = tr("last trick"); break;
    case RoundEndReason::ClosedTalonFailed: reason = tr("closed talon failed"); break;
    default: break;
    }
    return tr("%1 wins the round: %2 game point(s) — %3")
        .arg(winner).arg(m_core.roundAward()).arg(reason);
}

bool GameEngine::playerInputEnabled() const
{
    return m_visualPhase == Idle && !m_awaitingHost && !m_core.roundOver() && m_core.turn() == 0
        && !m_core.trickPending();
}

bool GameEngine::isPlayerCardPlayable(int handIndex) const
{
    return playerInputEnabled() && m_core.isLegalMove(0, handIndex);
}

int GameEngine::marriagePointsForCard(int handIndex) const
{
    return m_visualPhase == Idle && !m_awaitingHost ? m_core.marriagePointsForCard(0, handIndex) : 0;
}


void GameEngine::setPaused(bool value)
{
    if (value == m_paused)
        return;
    m_paused = value;
    if (m_paused)
        m_aiTimer.stop();
    else
        scheduleAiMove();
    emit pausedChanged();
}

void GameEngine::setVisualPhase(VisualPhase phase)
{
    if (m_visualPhase == phase)
        return;
    m_visualPhase = phase;
    emit visualPhaseChanged();
}

void GameEngine::start()
{
    if (m_started)
        return;
    m_started = true;
    emit settingsChanged();
    emit stateChanged();
    if (m_freshGame) {
        m_freshGame = false;
        startDealAnimation(initialDealList(), 1 - m_core.dealer());
    } else {
        finishIdle();
    }
}

void GameEngine::newMatch()
{
    if (m_mode == Mode::LanGuest) {
        if (!m_awaitingHost)
            sendRequest(QStringLiteral("newMatch"));
        return;
    }
    m_started = true;
    m_aiTimer.stop();
    m_trickPauseTimer.stop();
    m_visualWatchdog.stop();
    m_core.newMatch(freshSeed());
    persistGame();
    if (m_mode == Mode::LanHost) {
        m_remoteQueue.clear();
        sendToGuest(QStringLiteral("deal"), QVariantMap());
    }
    emit stateChanged();
    startDealAnimation(initialDealList(), 1 - m_core.dealer());
}

void GameEngine::nextRound()
{
    if (!m_core.roundOver() || m_core.matchOver())
        return;
    if (m_mode == Mode::LanGuest) {
        if (!m_awaitingHost)
            sendRequest(QStringLiteral("nextRound"));
        return;
    }
    m_aiTimer.stop();
    m_core.newRound();
    persistGame();
    if (m_mode == Mode::LanHost)
        sendToGuest(QStringLiteral("deal"), QVariantMap());
    emit stateChanged();
    startDealAnimation(initialDealList(), 1 - m_core.dealer());
}

void GameEngine::playCard(int handIndex, bool declareMarriage)
{
    if (!isPlayerCardPlayable(handIndex))
        return;
    if (declareMarriage && m_core.marriagePointsForCard(0, handIndex) == 0)
        return;
    if (m_mode == Mode::LanGuest) {
        sendRequest(QStringLiteral("play"), handIndex, declareMarriage);
        return;
    }
    startPlay(0, handIndex, declareMarriage);
}

bool GameEngine::startPlay(int player, int handIndex, bool declareMarriage)
{
    if (handIndex < 0 || handIndex >= static_cast<int>(m_core.hand(player).size()))
        return false;
    const Snapszer::Card card = m_core.hand(player)[static_cast<std::size_t>(handIndex)];
    const QString before = QString::fromStdString(m_core.serializeState());
    if (!m_core.playCard(player, handIndex, declareMarriage))
        return false;
    if (m_mode == Mode::LanHost) {
        QVariantMap message;
        message.insert(QStringLiteral("op"), QStringLiteral("play"));
        message.insert(QStringLiteral("p"), player);
        message.insert(QStringLiteral("i"), handIndex);
        message.insert(QStringLiteral("m"), declareMarriage);
        message.insert(QStringLiteral("pre"), before);
        sendToGuest(QStringLiteral("act"), message);
    }
    persistGame();
    setVisualPhase(CardFlight);
    emit stateChanged();
    emit cardAnimationRequested(cardId(card), player, handIndex);
    if (!m_animationsEnabled)
        QTimer::singleShot(0, this, &GameEngine::completeCardAnimation);
    else
        m_visualWatchdog.start(5000);
    return true;
}

bool GameEngine::applyAction(int player, const QString& op)
{
    const QString before = QString::fromStdString(m_core.serializeState());
    bool applied = false;
    if (op == QLatin1String("exchange"))
        applied = m_core.exchangeTrump(player);
    else if (op == QLatin1String("close"))
        applied = m_core.closeTalon(player);
    else if (op == QLatin1String("claim"))
        applied = m_core.claim66(player);
    if (!applied)
        return false;
    if (op == QLatin1String("claim"))
        m_aiTimer.stop();
    if (m_mode == Mode::LanHost) {
        QVariantMap message;
        message.insert(QStringLiteral("op"), op);
        message.insert(QStringLiteral("p"), player);
        message.insert(QStringLiteral("pre"), before);
        sendToGuest(QStringLiteral("act"), message);
    }
    persistGame();
    emit stateChanged();
    return true;
}

void GameEngine::exchangeTrump()
{
    if (!canExchangeTrump())
        return;
    if (m_mode == Mode::LanGuest)
        sendRequest(QStringLiteral("exchange"));
    else
        applyAction(0, QStringLiteral("exchange"));
}

void GameEngine::closeTalon()
{
    if (!canCloseTalon())
        return;
    if (m_mode == Mode::LanGuest)
        sendRequest(QStringLiteral("close"));
    else
        applyAction(0, QStringLiteral("close"));
}

void GameEngine::claim66()
{
    if (!canClaim66())
        return;
    if (m_mode == Mode::LanGuest)
        sendRequest(QStringLiteral("claim"));
    else
        applyAction(0, QStringLiteral("claim"));
}

void GameEngine::completeCardAnimation()
{
    if (m_visualPhase != CardFlight)
        return;
    m_visualWatchdog.stop();

    if (m_core.trickPending()) {
        beginTrickResolution();
        return;
    }

    // A marriage lead that reaches 66 ends the round immediately; there is no
    // useful reason to force an extra confirmation before the reply. This is
    // derived from the shared state alone, so both LAN devices do it in step.
    if (m_core.trick().size() == 1) {
        const int leader = m_core.trick().front().playedBy;
        if (m_core.canClaim66(leader)) {
            m_core.claim66(leader);
            persistGame();
            finishIdle();
            return;
        }
    }
    finishIdle();
}

void GameEngine::beginTrickResolution()
{
    setVisualPhase(TrickPause);
    emit stateChanged();
    if (m_animationsEnabled)
        m_trickPauseTimer.start(qMax(120, static_cast<int>(420.0 / m_animationSpeed)));
    else
        m_trickPauseTimer.start(1);
}

void GameEngine::completeTrickAnimation()
{
    if (m_visualPhase != TrickFlight)
        return;
    m_visualWatchdog.stop();
    const Snapszer::TrickCommitResult result = m_core.commitTrick();
    persistGame();
    emit stateChanged();
    if (result.roundOver) {
        finishIdle();
        return;
    }
    const QVariantList dealt = drawList(result.drawn);
    if (!dealt.isEmpty()) {
        startDealAnimation(dealt, result.winner);
        return;
    }
    finishIdle();
}

void GameEngine::startDealAnimation(const QVariantList& cards, int firstPlayer)
{
    setVisualPhase(Deal);
    emit stateChanged();
    emit dealAnimationRequested(cards, firstPlayer);
    if (!m_animationsEnabled)
        QTimer::singleShot(0, this, &GameEngine::completeDealAnimation);
    else
        m_visualWatchdog.start(9000);
}

void GameEngine::completeDealAnimation()
{
    if (m_visualPhase != Deal)
        return;
    m_visualWatchdog.stop();
    persistGame();
    finishIdle();
}

void GameEngine::finishIdle()
{
    setVisualPhase(Idle);
    emit stateChanged();
    persistGame();
    scheduleAiMove();
    processRemoteQueue();
}

void GameEngine::scheduleAiMove()
{
    m_aiTimer.stop();
    if (m_mode != Mode::Ai || m_paused || m_visualPhase != Idle || m_core.roundOver() || m_core.turn() != 1 || m_core.trickPending())
        return;
    m_aiTimer.start(m_aiPlayDelay);
}

void GameEngine::performAiMove()
{
    if (m_mode != Mode::Ai || m_paused || m_visualPhase != Idle || m_core.roundOver() || m_core.turn() != 1)
        return;
    const AiAction action = m_core.chooseAiAction(1, static_cast<AiDifficulty>(m_aiDifficulty));
    switch (action.type) {
    case AiActionType::Claim:
        applyAction(1, QStringLiteral("claim"));
        return;
    case AiActionType::ExchangeTrump:
        if (applyAction(1, QStringLiteral("exchange"))) scheduleAiMove();
        return;
    case AiActionType::CloseTalon:
        if (applyAction(1, QStringLiteral("close"))) scheduleAiMove();
        return;
    case AiActionType::Play:
        startPlay(1, action.handIndex, action.declareMarriage);
        return;
    default:
        return;
    }
}

void GameEngine::recoverVisualTimeout()
{
    switch (m_visualPhase) {
    case CardFlight: completeCardAnimation(); break;
    case TrickPause: beginTrickResolution(); break;
    case TrickFlight: completeTrickAnimation(); break;
    case Deal: completeDealAnimation(); break;
    default: break;
    }
}

void GameEngine::persistGame()
{
    // The autosave slot belongs to the match against the AI, which resumes
    // when a LAN game ends.
    if (m_mode != Mode::Ai)
        return;
    QSettings settings(settingsFilePath(), QSettings::NativeFormat);
    const std::string saved = m_core.serializeState();
    const QByteArray bytes(saved.data(), static_cast<int>(saved.size()));
    settings.setValue(QStringLiteral("game/autosave-v1"), QString::fromLatin1(bytes.toBase64()));
    settings.sync();
}

bool GameEngine::restoreGame()
{
    QSettings settings(settingsFilePath(), QSettings::NativeFormat);
    const QString encoded = settings.value(QStringLiteral("game/autosave-v1")).toString();
    if (encoded.isEmpty())
        return false;
    const QByteArray decoded = QByteArray::fromBase64(encoded.toLatin1());
    if (decoded.isEmpty())
        return false;
    return m_core.restoreState(std::string(decoded.constData(), static_cast<std::size_t>(decoded.size())));
}

void GameEngine::clearSavedGame()
{
    QSettings settings(settingsFilePath(), QSettings::NativeFormat);
    settings.remove(QStringLiteral("game/autosave-v1"));
    settings.sync();
}

void GameEngine::normalizeRestoredState()
{
    // A process may have been killed after the second card logically landed but
    // before QML acknowledged the trick animation. Resolve that visual-only
    // intermediate state immediately so restart never deadlocks.
    if (m_core.trickPending())
        m_core.commitTrick();
    persistGame();
}

// ---------------------------------------------------------------------------
// LAN play
//
// The host's GameCore is authoritative. Every state change the host makes is
// sent to the guest together with the state it was applied to ("pre"), and the
// guest replays it with the same animation path. Trick commits after an
// animation are deterministic, so they are not transmitted. Both sides only
// consume network messages while the table is idle, which keeps the two
// animation sequences in step regardless of device speed. Each host message
// carries a sequence number; a guest request based on an older number is
// answered with "nack" instead of being applied to a state the guest never saw.

void GameEngine::hostLanGame()
{
    if (networkGame())
        return;
    const QString name = m_playerName.trimmed().isEmpty() ? tr("Player") : m_playerName.trimmed();
    QString error;
    if (m_session->startHosting(name, 2, 1, &error))
        m_networkStatus = tr("Waiting for an opponent…");
    else
        m_networkStatus = tr("Cannot host a game: %1").arg(error);
    emit networkChanged();
}

void GameEngine::joinLanGame(const QString& address)
{
    const QString trimmed = address.trimmed();
    if (networkGame() || trimmed.isEmpty())
        return;
    setLanAddress(trimmed);
    m_session->joinHost(trimmed);
    m_networkStatus = tr("Connecting to %1…").arg(trimmed);
    emit networkChanged();
}

void GameEngine::cancelLan()
{
    if (networkGame()) {
        QVariantMap bye;
        bye.insert(QStringLiteral("t"), QStringLiteral("bye"));
        m_session->send(bye);
        returnToAiGame(QString());
        return;
    }
    m_session->stop();
    m_networkStatus.clear();
    emit networkChanged();
}

void GameEngine::onPeerConnectedChanged()
{
    if (!m_session->peerConnected() || networkGame())
        return;
    if (m_session->role() == LanSession::Guest) {
        QVariantMap hello;
        hello.insert(QStringLiteral("t"), QStringLiteral("hello"));
        hello.insert(QStringLiteral("v"), kLanProtocolVersion);
        hello.insert(QStringLiteral("name"), m_playerName.trimmed().isEmpty() ? tr("Player") : m_playerName.trimmed());
        m_session->send(hello);
        m_networkStatus = tr("Connected, waiting for the host…");
    } else {
        m_networkStatus = tr("Opponent found, starting…");
    }
    emit networkChanged();
}

void GameEngine::onPeerLost()
{
    if (networkGame()) {
        returnToAiGame(tr("Connection to %1 lost").arg(m_remoteName));
        return;
    }
    if (m_session->role() == LanSession::Host) {
        m_networkStatus = tr("Waiting for an opponent…");
    } else {
        m_session->stop();
        m_networkStatus = tr("Connection lost");
    }
    emit networkChanged();
}

void GameEngine::onConnectionFailed(const QString& reason)
{
    m_networkStatus = tr("Could not connect: %1").arg(reason);
    emit networkChanged();
}

void GameEngine::onNetworkMessage(const QVariantMap& message)
{
    const QString type = message.value(QStringLiteral("t")).toString();
    const LanSession::Role role = m_session->role();

    if (role == LanSession::Host && type == QLatin1String("hello")) {
        if (networkGame())
            return;
        if (message.value(QStringLiteral("v")).toInt() != kLanProtocolVersion) {
            QVariantMap reply;
            reply.insert(QStringLiteral("t"), QStringLiteral("version"));
            m_session->send(reply);
            m_session->stop();
            m_networkStatus = tr("The other phone has an incompatible Snapszer version");
            emit networkChanged();
            return;
        }
        const QString name = message.value(QStringLiteral("name")).toString().trimmed().left(32);
        m_remoteName = name.isEmpty() ? tr("Guest") : name;
        m_mode = Mode::LanHost;
        m_remoteQueue.clear();
        m_awaitingHost = false;
        m_netSeq = 0;
        m_networkStatus = tr("Playing against %1").arg(m_remoteName);
        m_aiTimer.stop();
        m_trickPauseTimer.stop();
        m_visualWatchdog.stop();
        m_started = true;
        m_freshGame = false;
        m_core.newMatch(freshSeed());

        QVariantMap welcome;
        welcome.insert(QStringLiteral("v"), kLanProtocolVersion);
        welcome.insert(QStringLiteral("name"), m_playerName.trimmed().isEmpty() ? tr("Player") : m_playerName.trimmed());
        sendToGuest(QStringLiteral("welcome"), welcome);

        emit resetVisuals();
        emit networkChanged();
        emit stateChanged();
        startDealAnimation(initialDealList(), 1 - m_core.dealer());
        return;
    }

    if (role == LanSession::Guest && type == QLatin1String("welcome")) {
        if (networkGame())
            return;
        m_mode = Mode::LanGuest;
        if (!adoptRemoteState(message.value(QStringLiteral("state")))) {
            returnToAiGame(tr("The LAN game could not be started"));
            return;
        }
        const QString name = message.value(QStringLiteral("name")).toString().trimmed().left(32);
        m_remoteName = name.isEmpty() ? tr("Host") : name;
        m_remoteQueue.clear();
        m_awaitingHost = false;
        m_netSeq = message.value(QStringLiteral("s")).toInt();
        m_networkStatus = tr("Playing against %1").arg(m_remoteName);
        m_aiTimer.stop();
        m_trickPauseTimer.stop();
        m_visualWatchdog.stop();
        m_started = true;
        m_freshGame = false;

        emit resetVisuals();
        emit networkChanged();
        emit stateChanged();
        startDealAnimation(initialDealList(), 1 - m_core.dealer());
        return;
    }

    if (role == LanSession::Guest && !networkGame()
        && (type == QLatin1String("busy") || type == QLatin1String("version"))) {
        m_session->stop();
        m_networkStatus = type == QLatin1String("busy")
                ? tr("That phone is already in a game")
                : tr("The other phone has an incompatible Snapszer version");
        emit networkChanged();
        return;
    }

    if (!networkGame())
        return;
    if (type == QLatin1String("bye")) {
        returnToAiGame(tr("%1 left the game").arg(m_remoteName));
        return;
    }
    m_remoteQueue.append(message);
    processRemoteQueue();
}

void GameEngine::processRemoteQueue()
{
    if (m_processingRemote)
        return;
    m_processingRemote = true;
    while (!m_remoteQueue.isEmpty() && m_visualPhase == Idle && networkGame()) {
        const QVariantMap message = m_remoteQueue.takeFirst();
        if (m_mode == Mode::LanHost)
            hostHandleRequest(message);
        else
            guestHandleMessage(message);
    }
    m_processingRemote = false;
}

void GameEngine::hostHandleRequest(const QVariantMap& message)
{
    if (message.value(QStringLiteral("t")).toString() != QLatin1String("req"))
        return;
    if (message.value(QStringLiteral("s")).toInt() != m_netSeq) {
        sendToGuest(QStringLiteral("nack"), QVariantMap());
        return;
    }
    const QString op = message.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("play")) {
        if (!startPlay(1, message.value(QStringLiteral("i")).toInt(),
                       message.value(QStringLiteral("m")).toBool()))
            sendSync();
    } else if (op == QLatin1String("exchange") || op == QLatin1String("close")
               || op == QLatin1String("claim")) {
        if (!applyAction(1, op))
            sendSync();
    } else if (op == QLatin1String("nextRound") && m_core.roundOver() && !m_core.matchOver()) {
        nextRound();
    } else if (op == QLatin1String("newMatch")) {
        newMatch();
    } else {
        sendToGuest(QStringLiteral("nack"), QVariantMap());
    }
}

void GameEngine::guestHandleMessage(const QVariantMap& message)
{
    const QString type = message.value(QStringLiteral("t")).toString();
    const QString outOfSync = tr("LAN game ended: the phones got out of sync");
    m_netSeq = message.value(QStringLiteral("s")).toInt();

    if (type == QLatin1String("act")) {
        if (!adoptRemoteState(message.value(QStringLiteral("pre")))) {
            returnToAiGame(outOfSync);
            return;
        }
        const int player = 1 - message.value(QStringLiteral("p")).toInt();
        if (player == 0)
            m_awaitingHost = false;
        const QString op = message.value(QStringLiteral("op")).toString();
        const bool applied = op == QLatin1String("play")
                ? startPlay(player, message.value(QStringLiteral("i")).toInt(),
                            message.value(QStringLiteral("m")).toBool())
                : applyAction(player, op);
        if (!applied)
            returnToAiGame(outOfSync);
    } else if (type == QLatin1String("deal")) {
        if (!adoptRemoteState(message.value(QStringLiteral("state")))) {
            returnToAiGame(outOfSync);
            return;
        }
        m_awaitingHost = false;
        emit stateChanged();
        startDealAnimation(initialDealList(), 1 - m_core.dealer());
    } else if (type == QLatin1String("sync")) {
        if (!adoptRemoteState(message.value(QStringLiteral("state")))) {
            returnToAiGame(outOfSync);
            return;
        }
        m_awaitingHost = false;
        emit resetVisuals();
        finishIdle();
    } else if (type == QLatin1String("nack")) {
        m_awaitingHost = false;
        emit stateChanged();
    }
}

void GameEngine::sendToGuest(const QString& type, QVariantMap message)
{
    if (m_mode != Mode::LanHost)
        return;
    if (type != QLatin1String("nack"))
        ++m_netSeq;
    message.insert(QStringLiteral("t"), type);
    message.insert(QStringLiteral("s"), m_netSeq);
    if (type == QLatin1String("welcome") || type == QLatin1String("deal") || type == QLatin1String("sync"))
        message.insert(QStringLiteral("state"), QString::fromStdString(m_core.serializeState()));
    m_session->send(message);
}

void GameEngine::sendSync()
{
    sendToGuest(QStringLiteral("sync"), QVariantMap());
}

void GameEngine::sendRequest(const QString& op, int handIndex, bool declareMarriage)
{
    if (m_mode != Mode::LanGuest || m_awaitingHost)
        return;
    QVariantMap message;
    message.insert(QStringLiteral("t"), QStringLiteral("req"));
    message.insert(QStringLiteral("s"), m_netSeq);
    message.insert(QStringLiteral("op"), op);
    message.insert(QStringLiteral("i"), handIndex);
    message.insert(QStringLiteral("m"), declareMarriage);
    m_session->send(message);
    m_awaitingHost = true;
    emit stateChanged();
}

bool GameEngine::adoptRemoteState(const QVariant& encoded)
{
    Snapszer::GameCore core(1);
    if (!core.restoreState(encoded.toString().toStdString()))
        return false;
    if (m_mode == Mode::LanGuest)
        core.swapPlayers();
    m_core = core;
    return true;
}

void GameEngine::returnToAiGame(const QString& notice)
{
    const bool wasNetwork = networkGame();
    m_session->stop();
    m_mode = Mode::Ai;
    m_remoteQueue.clear();
    m_awaitingHost = false;
    m_networkStatus = notice;
    if (wasNetwork) {
        m_remoteName.clear();
        m_aiTimer.stop();
        m_trickPauseTimer.stop();
        m_visualWatchdog.stop();
        emit resetVisuals();
        if (restoreGame()) {
            normalizeRestoredState();
            finishIdle();
        } else {
            m_core.newMatch(freshSeed());
            persistGame();
            emit stateChanged();
            startDealAnimation(initialDealList(), 1 - m_core.dealer());
        }
    }
    emit networkChanged();
    if (!notice.isEmpty())
        emit networkNotice(notice);
}
