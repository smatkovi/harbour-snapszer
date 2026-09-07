#include "GameEngine.h"

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

QString GameEngine::settingsFilePath() const
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
        return m_core.matchWinner() == 0 ? tr("You won the match") : tr("%1 won the match").arg(m_opponentName);
    if (m_core.roundOver())
        return tr("Round finished");
    if (m_visualPhase != Idle)
        return tr("Cards are moving…");
    if (m_core.talonClosed())
        return m_core.turn() == 0 ? tr("Your turn — talon closed") : tr("%1's turn — talon closed").arg(m_opponentName);
    if (m_core.strictPlay())
        return m_core.turn() == 0 ? tr("Your turn — strict play") : tr("%1's turn — strict play").arg(m_opponentName);
    return m_core.turn() == 0 ? tr("Your turn") : tr("%1's turn").arg(m_opponentName);
}

QString GameEngine::roundResult() const
{
    if (!m_core.roundOver())
        return QString();
    const QString winner = m_core.roundWinner() == 0 ? m_playerName : m_opponentName;
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
    return m_visualPhase == Idle && !m_core.roundOver() && m_core.turn() == 0
        && !m_core.trickPending();
}

bool GameEngine::isPlayerCardPlayable(int handIndex) const
{
    return playerInputEnabled() && m_core.isLegalMove(0, handIndex);
}

int GameEngine::marriagePointsForCard(int handIndex) const
{
    return m_visualPhase == Idle ? m_core.marriagePointsForCard(0, handIndex) : 0;
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
    m_started = true;
    m_aiTimer.stop();
    m_trickPauseTimer.stop();
    m_visualWatchdog.stop();
    m_core.newMatch(freshSeed());
    persistGame();
    emit stateChanged();
    startDealAnimation(initialDealList(), 1 - m_core.dealer());
}

void GameEngine::nextRound()
{
    if (!m_core.roundOver() || m_core.matchOver())
        return;
    m_aiTimer.stop();
    m_core.newRound();
    persistGame();
    emit stateChanged();
    startDealAnimation(initialDealList(), 1 - m_core.dealer());
}

void GameEngine::playCard(int handIndex, bool declareMarriage)
{
    if (!isPlayerCardPlayable(handIndex))
        return;
    const Snapszer::Card card = m_core.hand(0)[static_cast<std::size_t>(handIndex)];
    if (!m_core.playCard(0, handIndex, declareMarriage))
        return;
    persistGame();
    setVisualPhase(CardFlight);
    emit stateChanged();
    emit cardAnimationRequested(cardId(card), 0, handIndex);
    if (!m_animationsEnabled)
        QTimer::singleShot(0, this, &GameEngine::completeCardAnimation);
    else
        m_visualWatchdog.start(5000);
}

void GameEngine::exchangeTrump()
{
    if (m_visualPhase != Idle || !m_core.exchangeTrump(0))
        return;
    persistGame();
    emit stateChanged();
}

void GameEngine::closeTalon()
{
    if (m_visualPhase != Idle || !m_core.closeTalon(0))
        return;
    persistGame();
    emit stateChanged();
}

void GameEngine::claim66()
{
    if (m_visualPhase != Idle || !m_core.claim66(0))
        return;
    m_aiTimer.stop();
    persistGame();
    emit stateChanged();
}

void GameEngine::completeCardAnimation()
{
    if (m_visualPhase != CardFlight)
        return;
    m_visualWatchdog.stop();

    if (m_pendingAiMarriageClaim && m_core.canClaim66(1)) {
        m_pendingAiMarriageClaim = false;
        m_core.claim66(1);
        persistGame();
        finishIdle();
        return;
    }
    m_pendingAiMarriageClaim = false;

    if (m_core.trickPending()) {
        beginTrickResolution();
        return;
    }

    // A human marriage that reaches 66 is ended immediately; there is no useful
    // reason to force the player through an extra confirmation before the reply.
    if (m_core.canClaim66(0) && !m_core.trick().empty()) {
        m_core.claim66(0);
        persistGame();
        finishIdle();
        return;
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
}

void GameEngine::scheduleAiMove()
{
    m_aiTimer.stop();
    if (m_paused || m_visualPhase != Idle || m_core.roundOver() || m_core.turn() != 1 || m_core.trickPending())
        return;
    m_aiTimer.start(m_aiPlayDelay);
}

void GameEngine::performAiMove()
{
    if (m_paused || m_visualPhase != Idle || m_core.roundOver() || m_core.turn() != 1)
        return;
    const AiAction action = m_core.chooseAiAction(1, static_cast<AiDifficulty>(m_aiDifficulty));
    switch (action.type) {
    case AiActionType::Claim:
        if (m_core.claim66(1)) { persistGame(); emit stateChanged(); }
        return;
    case AiActionType::ExchangeTrump:
        if (m_core.exchangeTrump(1)) { persistGame(); emit stateChanged(); scheduleAiMove(); }
        return;
    case AiActionType::CloseTalon:
        if (m_core.closeTalon(1)) { persistGame(); emit stateChanged(); scheduleAiMove(); }
        return;
    case AiActionType::Play: {
        if (action.handIndex < 0 || action.handIndex >= static_cast<int>(m_core.hand(1).size()))
            return;
        const Snapszer::Card card = m_core.hand(1)[static_cast<std::size_t>(action.handIndex)];
        if (!m_core.playCard(1, action.handIndex, action.declareMarriage))
            return;
        m_pendingAiMarriageClaim = action.declareMarriage && m_core.canClaim66(1);
        persistGame();
        setVisualPhase(CardFlight);
        emit stateChanged();
        emit cardAnimationRequested(cardId(card), 1, action.handIndex);
        if (!m_animationsEnabled)
            QTimer::singleShot(0, this, &GameEngine::completeCardAnimation);
        else
            m_visualWatchdog.start(5000);
        return;
    }
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
