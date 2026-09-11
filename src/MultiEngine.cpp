#include "MultiEngine.h"

#include "GameEngine.h"
#include "LanSession.h"

#include <QDateTime>
#include <QSettings>

#include <algorithm>

using Snapszer::Contract;
using Snapszer::MultiAction;
using Snapszer::MultiActionType;
using Snapszer::MultiCore;
using Snapszer::MultiEndReason;
using Snapszer::MultiPhase;
using Snapszer::MultiVariant;

namespace {

const int kProtocolVersion = 2;

struct ActionName {
    MultiActionType type;
    const char* name;
};

const ActionName kActionNames[] = {
    {MultiActionType::ChooseTrump, "trump"},
    {MultiActionType::CallCard, "call"},
    {MultiActionType::Bid, "bid"},
    {MultiActionType::Pass, "pass"},
    {MultiActionType::Discard, "discard"},
    {MultiActionType::Snapszer, "snapszer"},
    {MultiActionType::Double, "double"},
    {MultiActionType::Play, "play"},
    {MultiActionType::Claim, "claim"},
};

MultiActionType actionType(const QString& name)
{
    for (const ActionName& entry : kActionNames) {
        if (name == QLatin1String(entry.name))
            return entry.type;
    }
    return MultiActionType::None;
}

std::uint32_t freshSeed()
{
    return static_cast<std::uint32_t>(QDateTime::currentMSecsSinceEpoch() & UINT64_C(0xffffffff));
}

} // namespace

MultiEngine::MultiEngine(GameEngine* settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_aiTimer.setSingleShot(true);
    m_trickPauseTimer.setSingleShot(true);
    m_watchdog.setSingleShot(true);
    connect(&m_aiTimer, &QTimer::timeout, this, &MultiEngine::runComputer);
    connect(&m_watchdog, &QTimer::timeout, this, &MultiEngine::recoverVisualTimeout);
    connect(&m_trickPauseTimer, &QTimer::timeout, this, [this]() {
        if (m_visualPhase != TrickPause || !m_core.trickPending())
            return;
        setVisualPhase(TrickFlight);
        emit stateChanged();
        emit trickAnimationRequested(m_core.pendingTrickWinner());
        if (!m_settings->animationsEnabled())
            QTimer::singleShot(0, this, &MultiEngine::completeTrickAnimation);
        else
            m_watchdog.start(7000);
    });
    // Seat names follow the player's name and the table redraws when the
    // card style changes.
    connect(m_settings, &GameEngine::settingsChanged, this, &MultiEngine::stateChanged);

    m_session = new LanSession(this);
    connect(m_session, &LanSession::peerJoined, this, &MultiEngine::onPeerJoined);
    connect(m_session, &LanSession::peerLost, this, &MultiEngine::onPeerLost);
    connect(m_session, &LanSession::connectionFailed, this, &MultiEngine::onConnectionFailed);
    connect(m_session, &LanSession::messageReceived, this, &MultiEngine::onMessage);

    loadSettings();
}

MultiEngine::~MultiEngine()
{
    persist();
}

// --- settings and persistence ------------------------------------------------------

void MultiEngine::loadSettings()
{
    QSettings settings(GameEngine::settingsFilePath(), QSettings::NativeFormat);
    m_rules3 = qBound(0, settings.value(QStringLiteral("multi/rules3"), 1).toInt(), 1);
    m_rules4 = qBound(0, settings.value(QStringLiteral("multi/rules4"), 1).toInt(), 1);
}

void MultiEngine::saveSettings()
{
    QSettings settings(GameEngine::settingsFilePath(), QSettings::NativeFormat);
    settings.setValue(QStringLiteral("multi/rules3"), m_rules3);
    settings.setValue(QStringLiteral("multi/rules4"), m_rules4);
    settings.sync();
}

void MultiEngine::setRules3(int value)
{
    value = qBound(0, value, 1);
    if (value == m_rules3) return;
    m_rules3 = value; saveSettings(); emit settingsChanged();
}

void MultiEngine::setRules4(int value)
{
    value = qBound(0, value, 1);
    if (value == m_rules4) return;
    m_rules4 = value; saveSettings(); emit settingsChanged();
}

void MultiEngine::persist()
{
    // Only matches against the computer are saved; a LAN match cannot be
    // continued without the other devices.
    if (m_mode != Mode::Local || !m_active)
        return;
    QSettings settings(GameEngine::settingsFilePath(), QSettings::NativeFormat);
    if (m_core.matchOver())
        settings.remove(QStringLiteral("multi/autosave-v1"));
    else
        settings.setValue(QStringLiteral("multi/autosave-v1"),
                          QString::fromLatin1(QByteArray::fromStdString(m_core.serializeState()).toBase64()));
    settings.sync();
}

bool MultiEngine::canResume() const
{
    if (m_active)
        return true;
    if (networkGame())
        return false;
    QSettings settings(GameEngine::settingsFilePath(), QSettings::NativeFormat);
    return settings.contains(QStringLiteral("multi/autosave-v1"));
}

MultiVariant MultiEngine::variantFor(int players) const
{
    if (players >= 4)
        return m_rules4 == 0 ? MultiVariant::HungarianFour : MultiVariant::AustrianFour;
    return m_rules3 == 0 ? MultiVariant::HungarianThree : MultiVariant::AustrianThree;
}

QString MultiEngine::rulesNameFor(int players, int rules) const
{
    if (players >= 4)
        return rules == 0 ? tr("Hungarian four-player snapszer") : tr("Bauernschnapsen");
    return rules == 0 ? tr("Hungarian three-player snapszer") : tr("Dreierschnapsen");
}

QString MultiEngine::rulesName() const
{
    switch (m_core.variant()) {
    case MultiVariant::HungarianThree: return rulesNameFor(3, 0);
    case MultiVariant::AustrianThree: return rulesNameFor(3, 1);
    case MultiVariant::HungarianFour: return rulesNameFor(4, 0);
    case MultiVariant::AustrianFour: return rulesNameFor(4, 1);
    }
    return QString();
}

QString MultiEngine::localName() const
{
    const QString name = m_settings->playerName().trimmed();
    return name.isEmpty() ? tr("Player") : name.left(32);
}

QString MultiEngine::seatName(int seat) const
{
    if (seat == 0)
        return localName();
    if (seat >= 0 && seat < m_seatNames.size() && !m_seatNames[seat].isEmpty())
        return m_seatNames[seat];
    return tr("Computer %1").arg(seat);
}

bool MultiEngine::seatIsComputer(int seat) const
{
    return m_mode != Mode::Guest && seat > 0 && (seat >= m_seatHuman.size() || !m_seatHuman[seat]);
}

// --- starting and running a match --------------------------------------------------

void MultiEngine::startMatch(int players)
{
    if (networkGame())
        return;
    players = players >= 4 ? 4 : 3;
    m_mode = Mode::Local;
    m_seatNames.clear();
    m_seatHuman.clear();
    for (int seat = 0; seat < players; ++seat) {
        m_seatNames.append(QString());
        m_seatHuman.append(seat == 0);
    }
    m_aiTimer.stop();
    m_trickPauseTimer.stop();
    m_watchdog.stop();
    m_core.newMatch(variantFor(players), freshSeed());
    m_active = true;
    emit resetVisuals();
    emit matchStarted();
    persist();
    finishIdle();
}

void MultiEngine::resume()
{
    if (m_active) {
        emit matchStarted(); // back to the running table
        return;
    }
    if (networkGame())
        return;
    QSettings settings(GameEngine::settingsFilePath(), QSettings::NativeFormat);
    const QByteArray saved = QByteArray::fromBase64(settings.value(QStringLiteral("multi/autosave-v1")).toString().toLatin1());
    MultiCore core;
    if (saved.isEmpty() || !core.restoreState(saved.toStdString()))
        return;
    if (core.trickPending())
        core.commitTrick();
    m_core = core;
    m_mode = Mode::Local;
    m_seatNames.clear();
    m_seatHuman.clear();
    for (int seat = 0; seat < m_core.players(); ++seat) {
        m_seatNames.append(QString());
        m_seatHuman.append(seat == 0);
    }
    m_active = true;
    emit resetVisuals();
    emit matchStarted();
    finishIdle();
}

void MultiEngine::act(const QString& type, int value, bool marriage)
{
    if (!m_active || m_visualPhase != Idle || m_awaitingHost)
        return;
    const MultiAction action{actionType(type), value, marriage};
    if (!m_core.isLegal(0, action))
        return;
    if (m_mode == Mode::Guest)
        sendRequest(action);
    else
        perform(0, action);
}

void MultiEngine::claim()
{
    act(QStringLiteral("claim"));
}

bool MultiEngine::perform(int seat, const MultiAction& action)
{
    const QString before = QString::fromStdString(m_core.serializeState());
    if (!m_core.apply(seat, action))
        return false;
    if (m_mode == Mode::Host) {
        QVariantMap message;
        message.insert(QStringLiteral("seat"), seat);
        message.insert(QStringLiteral("type"), static_cast<int>(action.type));
        message.insert(QStringLiteral("value"), action.value);
        message.insert(QStringLiteral("m"), action.marriage);
        message.insert(QStringLiteral("pre"), before);
        sendToGuests(QStringLiteral("act"), message);
    }
    persist();
    if (action.type == MultiActionType::Play) {
        setVisualPhase(CardFlight);
        emit stateChanged();
        emit cardAnimationRequested(cardId(action.value), seat);
        if (!m_settings->animationsEnabled())
            QTimer::singleShot(0, this, &MultiEngine::completeCardAnimation);
        else
            m_watchdog.start(5000);
    } else {
        finishIdle();
    }
    return true;
}

void MultiEngine::completeCardAnimation()
{
    if (m_visualPhase != CardFlight)
        return;
    m_watchdog.stop();
    if (m_mode != Mode::Guest && m_core.trick().size() == 1) {
        const int leader = m_core.trick().front().playedBy;
        if (m_core.canClaim(leader)) {
            setVisualPhase(Idle);
            perform(leader, {MultiActionType::Claim, -1, false});
            return;
        }
    }
    if (m_core.trickPending()) {
        setVisualPhase(TrickPause);
        emit stateChanged();
        const double speed = qMax(0.5, m_settings->animationSpeed());
        m_trickPauseTimer.start(m_settings->animationsEnabled() ? qMax(150, static_cast<int>(700.0 / speed)) : 1);
        return;
    }
    finishIdle();
}

void MultiEngine::completeTrickAnimation()
{
    if (m_visualPhase != TrickFlight)
        return;
    m_watchdog.stop();
    m_core.commitTrick();
    persist();
    finishIdle();
}

void MultiEngine::recoverVisualTimeout()
{
    if (m_visualPhase == CardFlight)
        completeCardAnimation();
    else if (m_visualPhase == TrickFlight)
        completeTrickAnimation();
}

void MultiEngine::setVisualPhase(VisualPhase phase)
{
    if (phase == m_visualPhase)
        return;
    m_visualPhase = phase;
    emit visualPhaseChanged();
}

void MultiEngine::finishIdle()
{
    setVisualPhase(Idle);
    emit stateChanged();
    scheduleComputer();
    processRemoteQueue();
}

void MultiEngine::setPaused(bool value)
{
    if (value == m_paused)
        return;
    m_paused = value;
    if (m_paused && m_mode == Mode::Local)
        m_aiTimer.stop();
    else
        scheduleComputer();
    emit pausedChanged();
}

void MultiEngine::scheduleComputer()
{
    m_aiTimer.stop();
    if (!m_active || m_mode == Mode::Guest || (m_paused && m_mode == Mode::Local)
        || m_visualPhase != Idle || m_core.roundOver())
        return;
    for (int seat = 1; seat < m_core.players(); ++seat) {
        if (seatIsComputer(seat) && m_core.canClaim(seat)) {
            m_aiTimer.start(400);
            return;
        }
    }
    const int actor = m_core.actor();
    if (actor > 0 && seatIsComputer(actor)) {
        // Give a person whose side could call 66 time to do it before a
        // computer plays on, also on a guest that is still animating.
        bool humanMayClaim = false;
        for (int seat = 0; seat < m_core.players(); ++seat) {
            if (!seatIsComputer(seat) && m_core.canClaim(seat))
                humanMayClaim = true;
        }
        m_aiTimer.start(humanMayClaim ? qMax(m_settings->aiPlayDelay(), 3000) : m_settings->aiPlayDelay());
    }
}

void MultiEngine::runComputer()
{
    if (!m_active || m_mode == Mode::Guest || m_visualPhase != Idle || m_core.roundOver())
        return;
    for (int seat = 1; seat < m_core.players(); ++seat) {
        if (seatIsComputer(seat) && m_core.canClaim(seat)) {
            perform(seat, {MultiActionType::Claim, -1, false});
            return;
        }
    }
    const int actor = m_core.actor();
    if (actor <= 0 || !seatIsComputer(actor))
        return;
    const MultiAction action = m_core.chooseAiAction(actor);
    if (!perform(actor, action))
        scheduleComputer();
}

void MultiEngine::nextRound()
{
    if (!m_active || !m_core.roundOver() || m_core.matchOver() || m_visualPhase != Idle)
        return;
    if (m_mode == Mode::Guest) {
        if (!m_awaitingHost) {
            QVariantMap message;
            message.insert(QStringLiteral("t"), QStringLiteral("req"));
            message.insert(QStringLiteral("s"), m_netSeq);
            message.insert(QStringLiteral("op"), QStringLiteral("nextRound"));
            m_session->sendTo(0, message);
            m_awaitingHost = true;
            emit stateChanged();
        }
        return;
    }
    m_core.nextRound();
    if (m_mode == Mode::Host)
        sendToGuests(QStringLiteral("deal"), QVariantMap());
    persist();
    emit resetVisuals();
    finishIdle();
}

void MultiEngine::newMatch()
{
    if (!m_active)
        return;
    if (m_mode == Mode::Local) {
        startMatch(m_core.players());
        return;
    }
    if (m_mode == Mode::Guest) {
        if (m_visualPhase == Idle && !m_awaitingHost && m_core.matchOver()) {
            QVariantMap message;
            message.insert(QStringLiteral("t"), QStringLiteral("req"));
            message.insert(QStringLiteral("s"), m_netSeq);
            message.insert(QStringLiteral("op"), QStringLiteral("newMatch"));
            m_session->sendTo(0, message);
            m_awaitingHost = true;
            emit stateChanged();
        }
        return;
    }
    m_aiTimer.stop();
    m_trickPauseTimer.stop();
    m_watchdog.stop();
    m_remoteQueue.clear();
    m_core.newMatch(m_core.variant(), freshSeed());
    sendToGuests(QStringLiteral("deal"), QVariantMap());
    emit resetVisuals();
    finishIdle();
}

// --- presentation ----------------------------------------------------------------------

QString MultiEngine::cardId(const Snapszer::Card& card)
{
    return QString::number(card.suit) + QLatin1Char('_') + QString::number(card.rank);
}

QString MultiEngine::cardId(int key)
{
    return QString::number(key / 100) + QLatin1Char('_') + QString::number(key % 100);
}

QString MultiEngine::contractLabel(int contract) const
{
    switch (static_cast<Contract>(contract)) {
    case Contract::Normal: return tr("Normal game");
    case Contract::Bettler: return tr("Bettler");
    case Contract::Schnapser: return m_core.hungarian() ? tr("Snapszer") : tr("Schnapser");
    case Contract::Gang: return tr("Gang");
    case Contract::Zehnergang: return tr("Zehnergang");
    case Contract::Kontraschnapser: return tr("Kontraschnapser");
    case Contract::Bauernschnapser: return tr("Bauernschnapser");
    case Contract::Kontrabauernschnapser: return tr("Kontrabauernschnapser");
    }
    return QString();
}

QString MultiEngine::contractName() const
{
    if (!m_active || m_core.declarer() < 0)
        return QString();
    QString name = contractLabel(static_cast<int>(m_core.contract()));
    if (m_core.doubling() > 1)
        name += QStringLiteral(" ×%1").arg(m_core.doubling());
    return name;
}

QString MultiEngine::calledCard() const
{
    return m_active && m_core.calledCard() >= 0 ? cardId(m_core.calledCard()) : QString();
}

QVariantList MultiEngine::seats() const
{
    QVariantList result;
    if (!m_active)
        return result;
    const int actor = m_core.actor();
    for (int seat = 0; seat < m_core.players(); ++seat) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), seatName(seat));
        entry.insert(QStringLiteral("human"), seat == 0 || (seat < m_seatHuman.size() && m_seatHuman[seat]));
        entry.insert(QStringLiteral("cards"), static_cast<int>(m_core.hand(seat).size()));
        entry.insert(QStringLiteral("tricks"), m_core.trickCount(seat));
        entry.insert(QStringLiteral("score"), m_core.score(seat));
        entry.insert(QStringLiteral("dealer"), seat == m_core.dealer());
        entry.insert(QStringLiteral("forehand"), seat == m_core.forehand());
        entry.insert(QStringLiteral("actor"), seat == actor && !m_core.roundOver());
        entry.insert(QStringLiteral("declarer"), seat == m_core.declarer());
        entry.insert(QStringLiteral("sittingOut"), seat == m_core.sittingOut());
        entry.insert(QStringLiteral("passed"), m_core.phase() == MultiPhase::Bidding && m_core.hasPassed(seat));
        entry.insert(QStringLiteral("winner"), m_core.roundOver() && ((m_core.roundWinnerMask() >> seat) & 1));
        QString party;
        if (m_core.variant() == MultiVariant::AustrianFour)
            party = seat % 2 == 0 ? QStringLiteral("partner") : QStringLiteral("opponent");
        if (m_core.declarer() >= 0) {
            if (!m_core.partnersVisibleTo(0, seat))
                party = QStringLiteral("unknown");
            else if (m_core.variant() != MultiVariant::AustrianFour || m_core.sittingOut() >= 0)
                party = m_core.inDeclarerParty(seat) == m_core.inDeclarerParty(0)
                        ? QStringLiteral("partner") : QStringLiteral("opponent");
        }
        entry.insert(QStringLiteral("party"), seat == 0 ? QStringLiteral("me") : party);
        result.append(entry);
    }
    return result;
}

QVariantList MultiEngine::hand() const
{
    QVariantList result;
    if (!m_active)
        return result;
    const bool idle = m_visualPhase == Idle && !m_awaitingHost;
    const std::vector<int> legal = m_core.legalCards(0);
    const bool discard = discarding();
    for (const Snapszer::Card& card : m_core.hand(0)) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), cardId(card));
        entry.insert(QStringLiteral("key"), card.key());
        const bool playable = idle && std::find(legal.begin(), legal.end(), card.key()) != legal.end();
        entry.insert(QStringLiteral("playable"), playable || discard);
        entry.insert(QStringLiteral("marriage"), playable ? m_core.marriageValue(0, card.key()) : 0);
        result.append(entry);
    }
    return result;
}

QVariantList MultiEngine::trick() const
{
    QVariantList result;
    if (!m_active)
        return result;
    for (const Snapszer::Card& card : m_core.trick()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), cardId(card));
        entry.insert(QStringLiteral("seat"), card.playedBy);
        result.append(entry);
    }
    return result;
}

bool MultiEngine::localDecision() const
{
    return m_active && m_visualPhase == Idle && !m_awaitingHost && m_core.actor() == 0;
}

bool MultiEngine::choosingCard() const
{
    return localDecision() && m_core.phase() == MultiPhase::CallCard;
}

bool MultiEngine::discarding() const
{
    return localDecision() && m_core.phase() == MultiPhase::Talon;
}

bool MultiEngine::canClaim() const
{
    return m_active && m_visualPhase == Idle && !m_awaitingHost && m_core.canClaim(0);
}

QVariantList MultiEngine::options() const
{
    QVariantList result;
    if (!localDecision())
        return result;
    static const char* suitNames[] = {
        QT_TR_NOOP("Bells"), QT_TR_NOOP("Leaves"), QT_TR_NOOP("Acorns"), QT_TR_NOOP("Hearts")
    };
    auto add = [&result](const QString& type, int value, const QString& label) {
        QVariantMap entry;
        entry.insert(QStringLiteral("type"), type);
        entry.insert(QStringLiteral("value"), value);
        entry.insert(QStringLiteral("label"), label);
        result.append(entry);
    };
    for (const MultiAction& action : m_core.legalActions(0)) {
        switch (action.type) {
        case MultiActionType::ChooseTrump:
            add(QStringLiteral("trump"), action.value, tr(suitNames[action.value]));
            break;
        case MultiActionType::Snapszer:
            add(QStringLiteral("snapszer"), -1, tr("Snapszer (6)"));
            break;
        case MultiActionType::Bid:
            add(QStringLiteral("bid"), action.value,
                tr("%1 (%2)").arg(contractLabel(action.value))
                    .arg(MultiCore::contractValue(static_cast<Contract>(action.value))));
            break;
        case MultiActionType::Double: {
            const int step = m_core.doublingStep();
            add(QStringLiteral("double"), -1,
                step == 0 ? tr("Kontra") : step == 1 ? tr("Rekontra") : tr("Subkontra"));
            break;
        }
        case MultiActionType::Pass:
            if (m_core.phase() == MultiPhase::Announce)
                add(QStringLiteral("pass"), -1, tr("Normal game"));
            else if (m_core.phase() == MultiPhase::Doubling)
                add(QStringLiteral("pass"), -1, tr("Continue"));
            else if (m_core.forehand() == 0 && m_core.bidHolder() == 0 && m_core.contract() == Contract::Normal)
                add(QStringLiteral("pass"), -1, tr("Normal game"));
            else
                add(QStringLiteral("pass"), -1, tr("Pass"));
            break;
        default:
            break;
        }
    }
    return result;
}

QString MultiEngine::status() const
{
    if (!m_active)
        return QString();
    if (m_core.matchOver())
        return tr("Match over");
    if (m_core.roundOver())
        return tr("Round over");
    if (m_awaitingHost || m_visualPhase != Idle)
        return tr("Cards are moving…");
    const int actor = m_core.actor();
    const bool mine = actor == 0;
    const QString name = seatName(actor);
    switch (m_core.phase()) {
    case MultiPhase::ChooseTrump:
        return mine ? tr("Choose trump") : tr("%1 chooses trump").arg(name);
    case MultiPhase::CallCard:
        return mine ? tr("Call a card; its holder becomes your partner") : tr("%1 calls a card").arg(name);
    case MultiPhase::Announce:
        return mine ? tr("Snapszer or normal game?") : tr("%1 decides on a Snapszer").arg(name);
    case MultiPhase::Bidding:
        return mine ? tr("Your bid") : tr("%1 is bidding").arg(name);
    case MultiPhase::Talon:
        return mine ? tr("Take the talon and discard two cards") : tr("%1 exchanges with the talon").arg(name);
    case MultiPhase::Doubling:
        if (mine)
            return tr("Double the game?");
        // Naming who decides would give away the secret partner.
        return m_core.variant() == MultiVariant::HungarianFour
                ? tr("Waiting for announcements") : tr("%1 considers a Kontra").arg(name);
    case MultiPhase::Play:
        return mine ? tr("Your turn") : tr("%1's turn").arg(name);
    case MultiPhase::RoundOver:
        break;
    }
    return QString();
}

QString MultiEngine::roundResult() const
{
    if (!m_active || !m_core.roundOver())
        return QString();
    QStringList winners;
    for (int seat = 0; seat < m_core.players(); ++seat) {
        if ((m_core.roundWinnerMask() >> seat) & 1)
            winners.append(seatName(seat));
    }
    const QString contract = contractLabel(static_cast<int>(m_core.contract()));
    QString reason;
    switch (m_core.endReason()) {
    case MultiEndReason::Claim66: reason = tr("66 reached"); break;
    case MultiEndReason::LastTrick: reason = tr("last trick"); break;
    case MultiEndReason::ContractMade: reason = tr("%1 made").arg(contract); break;
    case MultiEndReason::ContractFailed: reason = tr("%1 failed").arg(contract); break;
    case MultiEndReason::None: break;
    }
    QString text = tr("%1: +%2 — %3").arg(winners.join(QStringLiteral(", "))).arg(m_core.roundAward()).arg(reason);
    if (m_core.matchOver()) {
        QStringList best;
        for (int seat = 0; seat < m_core.players(); ++seat) {
            if ((m_core.matchWinnerMask() >> seat) & 1)
                best.append(seatName(seat));
        }
        text += QLatin1Char('\n') + tr("%1 won the match").arg(best.join(QStringLiteral(", ")));
    }
    return text;
}

// --- LAN ---------------------------------------------------------------------------------

bool MultiEngine::lanBusy() const
{
    return m_mode == Mode::Local && m_session->role() != LanSession::None;
}

QVariantList MultiEngine::lobby() const
{
    QVariantList result;
    if (!lanBusy())
        return result;
    for (int seat = 0; seat < m_seatNames.size(); ++seat) {
        QVariantMap entry;
        const bool taken = seat == 0 || (seat < m_seatPeers.size() && m_seatPeers[seat] >= 0);
        const bool hosting = m_session->role() == LanSession::Host;
        entry.insert(QStringLiteral("name"), seat == 0 && hosting ? localName() : m_seatNames[seat]);
        entry.insert(QStringLiteral("taken"), taken || !m_seatNames[seat].isEmpty());
        result.append(entry);
    }
    return result;
}

void MultiEngine::hostLanGame(int players)
{
    if (networkGame())
        return;
    players = players >= 4 ? 4 : 3;
    QString error;
    if (!m_session->startHosting(localName(), players, players - 1, &error)) {
        m_networkStatus = tr("Cannot host a game: %1").arg(error);
        emit networkChanged();
        return;
    }
    m_hostPlayers = players;
    m_lobbyOpen = true;
    m_seatPeers.clear();
    m_seatNames.clear();
    for (int seat = 0; seat < players; ++seat) {
        m_seatPeers.append(-1);
        m_seatNames.append(QString());
    }
    m_networkStatus = tr("Waiting for players… Free seats are taken by the computer when you start.");
    emit networkChanged();
}

void MultiEngine::joinLanGame(const QString& address)
{
    const QString trimmed = LanSession::normalizeAddress(address);
    if (networkGame() || trimmed.isEmpty())
        return;
    m_lobbyOpen = false;
    m_seatNames.clear();
    m_joinAddress = trimmed;
    m_settings->setLanAddress(trimmed);
    // Set before connecting: a connection that fails at once overwrites it.
    m_networkStatus = tr("Connecting to %1…").arg(trimmed);
    emit networkChanged();
    m_session->joinHost(trimmed);
}

void MultiEngine::cancelLan()
{
    if (networkGame()) {
        QVariantMap bye;
        bye.insert(QStringLiteral("t"), QStringLiteral("bye"));
        m_session->send(bye);
        leaveNetwork(QString());
        return;
    }
    m_session->stop();
    m_lobbyOpen = false;
    m_seatNames.clear();
    m_seatPeers.clear();
    m_networkStatus.clear();
    emit networkChanged();
}

void MultiEngine::startLanMatch()
{
    if (!m_lobbyOpen)
        return;
    m_lobbyOpen = false;
    m_session->setAcceptingGuests(false);
    persist(); // keep a running match against the computer
    m_mode = Mode::Host;
    const int players = m_hostPlayers;
    m_seatHuman.clear();
    for (int seat = 0; seat < players; ++seat)
        m_seatHuman.append(seat == 0 || m_seatPeers[seat] >= 0);
    m_aiTimer.stop();
    m_trickPauseTimer.stop();
    m_watchdog.stop();
    m_remoteQueue.clear();
    m_core.newMatch(variantFor(players), freshSeed());
    m_active = true;

    QVariantList names, humans;
    for (int seat = 0; seat < players; ++seat) {
        names.append(seat == 0 ? localName() : m_seatNames[seat]);
        humans.append(m_seatHuman[seat]);
    }
    ++m_netSeq;
    for (int seat = 1; seat < players; ++seat) {
        if (m_seatPeers[seat] < 0)
            continue;
        QVariantMap welcome;
        welcome.insert(QStringLiteral("t"), QStringLiteral("welcome"));
        welcome.insert(QStringLiteral("s"), m_netSeq);
        welcome.insert(QStringLiteral("seat"), seat);
        welcome.insert(QStringLiteral("names"), names);
        welcome.insert(QStringLiteral("humans"), humans);
        welcome.insert(QStringLiteral("state"), QString::fromStdString(m_core.serializeState()));
        m_session->sendTo(m_seatPeers[seat], welcome);
    }
    m_networkStatus = tr("Playing over LAN");
    emit resetVisuals();
    emit matchStarted();
    emit networkChanged();
    finishIdle();
}

void MultiEngine::onPeerJoined(int peer)
{
    if (m_session->role() != LanSession::Guest)
        return; // a host waits for the guest's hello
    Q_UNUSED(peer);
    QVariantMap hello;
    hello.insert(QStringLiteral("t"), QStringLiteral("hello"));
    hello.insert(QStringLiteral("v"), kProtocolVersion);
    hello.insert(QStringLiteral("kind"), QStringLiteral("multi"));
    hello.insert(QStringLiteral("name"), localName());
    m_session->sendTo(0, hello);
    m_networkStatus = tr("Connected, waiting for the host…");
    emit networkChanged();
}

void MultiEngine::onPeerLost(int peer)
{
    if (m_session->role() == LanSession::Guest || m_mode == Mode::Guest) {
        leaveNetwork(m_mode == Mode::Guest ? tr("Connection to the host lost") : QString());
        if (m_mode == Mode::Local) {
            m_networkStatus = tr("Connection lost");
            emit networkChanged();
        }
        return;
    }
    const int seat = m_seatPeers.indexOf(peer);
    if (seat <= 0)
        return;
    const QString name = m_seatNames[seat];
    m_seatPeers[seat] = -1;
    if (m_lobbyOpen) {
        m_seatNames[seat].clear();
        sendLobby();
        emit networkChanged();
        return;
    }
    // In a running match the computer takes over the empty seat.
    m_seatHuman[seat] = false;
    m_seatNames[seat] = tr("%1 (computer)").arg(name);
    QVariantMap message;
    QVariantList names, humans;
    for (int s = 0; s < m_core.players(); ++s) {
        names.append(s == 0 ? localName() : m_seatNames[s]);
        humans.append(s == 0 || m_seatHuman[s]);
    }
    message.insert(QStringLiteral("names"), names);
    message.insert(QStringLiteral("humans"), humans);
    sendToGuests(QStringLiteral("names"), message);
    emit networkNotice(tr("%1 left, the computer takes over").arg(name));
    emit stateChanged();
    scheduleComputer();
}

void MultiEngine::onConnectionFailed(const QString& reason)
{
    m_networkStatus = tr("Could not connect: %1").arg(reason);
    emit networkChanged();
}

void MultiEngine::sendLobby()
{
    QVariantList names;
    for (int seat = 0; seat < m_seatNames.size(); ++seat)
        names.append(seat == 0 ? localName() : m_seatNames[seat]);
    for (int seat = 1; seat < m_seatPeers.size(); ++seat) {
        if (m_seatPeers[seat] < 0)
            continue;
        QVariantMap message;
        message.insert(QStringLiteral("t"), QStringLiteral("lobby"));
        message.insert(QStringLiteral("names"), names);
        message.insert(QStringLiteral("seat"), seat);
        m_session->sendTo(m_seatPeers[seat], message);
    }
}

void MultiEngine::onMessage(int peer, const QVariantMap& message)
{
    const QString type = message.value(QStringLiteral("t")).toString();

    if (m_session->role() == LanSession::Host) {
        if (type == QLatin1String("hello")) {
            QVariantMap reply;
            if (message.value(QStringLiteral("kind")).toString() != QLatin1String("multi")) {
                // A two-player app: tell it the size of this table.
                reply.insert(QStringLiteral("t"), QStringLiteral("mode"));
                reply.insert(QStringLiteral("players"), m_hostPlayers);
                m_session->sendTo(peer, reply);
                m_session->dropPeer(peer);
                return;
            }
            if (message.value(QStringLiteral("v")).toInt() != kProtocolVersion) {
                reply.insert(QStringLiteral("t"), QStringLiteral("version"));
                m_session->sendTo(peer, reply);
                m_session->dropPeer(peer);
                return;
            }
            const int seat = m_seatPeers.indexOf(-1, 1);
            if (!m_lobbyOpen || seat < 1) {
                reply.insert(QStringLiteral("t"), QStringLiteral("busy"));
                m_session->sendTo(peer, reply);
                m_session->dropPeer(peer);
                return;
            }
            const QString name = message.value(QStringLiteral("name")).toString().trimmed().left(32);
            m_seatPeers[seat] = peer;
            m_seatNames[seat] = name.isEmpty() ? tr("Guest") : name;
            sendLobby();
            emit networkChanged();
            return;
        }
        if (type == QLatin1String("bye")) {
            onPeerLost(peer);
            m_session->dropPeer(peer);
            return;
        }
        if (m_mode == Mode::Host) {
            m_remoteQueue.append(qMakePair(peer, message));
            processRemoteQueue();
        }
        return;
    }

    // Guest side
    if (type == QLatin1String("lobby")) {
        m_seatNames.clear();
        const QVariantList names = message.value(QStringLiteral("names")).toList();
        for (const QVariant& name : names)
            m_seatNames.append(name.toString());
        m_networkStatus = tr("Joined %1's table, waiting for the host to start").arg(m_seatNames.value(0));
        emit networkChanged();
        return;
    }
    if (type == QLatin1String("welcome")) {
        if (m_mode == Mode::Guest)
            return;
        persist(); // keep a running match against the computer
        m_mode = Mode::Guest;
        m_hostSeat = message.value(QStringLiteral("seat")).toInt();
        if (!adoptState(message.value(QStringLiteral("state")))) {
            leaveNetwork(tr("The LAN game could not be started"));
            return;
        }
        const int players = m_core.players();
        const QVariantList names = message.value(QStringLiteral("names")).toList();
        const QVariantList humans = message.value(QStringLiteral("humans")).toList();
        m_seatNames.clear();
        m_seatHuman.clear();
        for (int seat = 0; seat < players; ++seat) {
            const int hostSeat = (seat + m_hostSeat) % players;
            m_seatNames.append(names.value(hostSeat).toString());
            m_seatHuman.append(humans.value(hostSeat).toBool());
        }
        m_netSeq = message.value(QStringLiteral("s")).toInt();
        m_awaitingHost = false;
        m_remoteQueue.clear();
        m_aiTimer.stop();
        m_active = true;
        m_networkStatus = tr("Playing over LAN");
        emit resetVisuals();
        emit matchStarted();
        emit networkChanged();
        finishIdle();
        return;
    }
    if (type == QLatin1String("mode")) {
        const int players = message.value(QStringLiteral("players")).toInt();
        m_session->stop();
        m_networkStatus.clear();
        emit networkChanged();
        emit lanRedirect(m_joinAddress, players);
        return;
    }
    if (type == QLatin1String("busy") || type == QLatin1String("version")) {
        m_session->stop();
        m_networkStatus = type == QLatin1String("busy")
                ? tr("That table is full or already playing")
                : tr("The other device runs an incompatible Snapszer version or a different game");
        emit networkChanged();
        return;
    }
    if (m_mode != Mode::Guest)
        return;
    if (type == QLatin1String("bye")) {
        leaveNetwork(tr("The host ended the game"));
        return;
    }
    m_remoteQueue.append(qMakePair(peer, message));
    processRemoteQueue();
}

void MultiEngine::processRemoteQueue()
{
    if (m_processingRemote)
        return;
    m_processingRemote = true;
    while (!m_remoteQueue.isEmpty() && m_visualPhase == Idle && networkGame()) {
        const QPair<int, QVariantMap> item = m_remoteQueue.takeFirst();
        if (m_mode == Mode::Host)
            hostHandle(item.first, item.second);
        else
            guestHandle(item.second);
    }
    m_processingRemote = false;
}

void MultiEngine::hostHandle(int peer, const QVariantMap& message)
{
    if (message.value(QStringLiteral("t")).toString() != QLatin1String("req"))
        return;
    const int seat = m_seatPeers.indexOf(peer);
    if (seat <= 0)
        return;
    if (message.value(QStringLiteral("s")).toInt() != m_netSeq) {
        QVariantMap nack;
        nack.insert(QStringLiteral("t"), QStringLiteral("nack"));
        nack.insert(QStringLiteral("s"), m_netSeq);
        m_session->sendTo(peer, nack);
        return;
    }
    const QString op = message.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("nextRound")) {
        if (m_core.roundOver() && !m_core.matchOver())
            nextRound();
        return;
    }
    if (op == QLatin1String("newMatch")) {
        if (m_core.matchOver())
            newMatch();
        return;
    }
    const MultiAction action{static_cast<MultiActionType>(message.value(QStringLiteral("type")).toInt()),
                             message.value(QStringLiteral("value")).toInt(),
                             message.value(QStringLiteral("m")).toBool()};
    if (!perform(seat, action))
        sendToGuests(QStringLiteral("sync"), QVariantMap());
}

void MultiEngine::guestHandle(const QVariantMap& message)
{
    const QString type = message.value(QStringLiteral("t")).toString();
    const QString outOfSync = tr("LAN game ended: the devices got out of sync");
    if (message.contains(QStringLiteral("s")))
        m_netSeq = message.value(QStringLiteral("s")).toInt();

    if (type == QLatin1String("act")) {
        if (!adoptState(message.value(QStringLiteral("pre")))) {
            leaveNetwork(outOfSync);
            return;
        }
        const int players = m_core.players();
        const int seat = (message.value(QStringLiteral("seat")).toInt() - m_hostSeat + players) % players;
        if (seat == 0)
            m_awaitingHost = false;
        const MultiAction action{static_cast<MultiActionType>(message.value(QStringLiteral("type")).toInt()),
                                 message.value(QStringLiteral("value")).toInt(),
                                 message.value(QStringLiteral("m")).toBool()};
        if (!perform(seat, action))
            leaveNetwork(outOfSync);
    } else if (type == QLatin1String("deal") || type == QLatin1String("sync")) {
        if (!adoptState(message.value(QStringLiteral("state")))) {
            leaveNetwork(outOfSync);
            return;
        }
        m_awaitingHost = false;
        emit resetVisuals();
        finishIdle();
    } else if (type == QLatin1String("nack")) {
        m_awaitingHost = false;
        emit stateChanged();
    } else if (type == QLatin1String("names")) {
        const int players = m_core.players();
        const QVariantList names = message.value(QStringLiteral("names")).toList();
        const QVariantList humans = message.value(QStringLiteral("humans")).toList();
        for (int seat = 0; seat < players && seat < m_seatNames.size(); ++seat) {
            const int hostSeat = (seat + m_hostSeat) % players;
            m_seatNames[seat] = names.value(hostSeat).toString();
            m_seatHuman[seat] = humans.value(hostSeat).toBool();
        }
        emit stateChanged();
    }
}

void MultiEngine::sendToGuests(const QString& type, QVariantMap message)
{
    if (m_mode != Mode::Host)
        return;
    ++m_netSeq;
    message.insert(QStringLiteral("t"), type);
    message.insert(QStringLiteral("s"), m_netSeq);
    if (type == QLatin1String("deal") || type == QLatin1String("sync"))
        message.insert(QStringLiteral("state"), QString::fromStdString(m_core.serializeState()));
    m_session->send(message);
}

void MultiEngine::sendRequest(const MultiAction& action)
{
    if (m_mode != Mode::Guest || m_awaitingHost)
        return;
    QVariantMap message;
    message.insert(QStringLiteral("t"), QStringLiteral("req"));
    message.insert(QStringLiteral("s"), m_netSeq);
    message.insert(QStringLiteral("type"), static_cast<int>(action.type));
    message.insert(QStringLiteral("value"), action.value);
    message.insert(QStringLiteral("m"), action.marriage);
    m_session->sendTo(0, message);
    m_awaitingHost = true;
    emit stateChanged();
}

bool MultiEngine::adoptState(const QVariant& encoded)
{
    MultiCore core;
    if (!core.restoreState(encoded.toString().toStdString()))
        return false;
    if (m_mode == Mode::Guest)
        core.rotateSeats(m_hostSeat);
    m_core = core;
    return true;
}

void MultiEngine::leaveNetwork(const QString& notice)
{
    m_session->stop();
    const bool wasNetwork = networkGame();
    m_mode = Mode::Local;
    m_lobbyOpen = false;
    m_remoteQueue.clear();
    m_awaitingHost = false;
    m_seatPeers.clear();
    m_seatNames.clear();
    m_seatHuman.clear();
    m_networkStatus = notice;
    if (wasNetwork) {
        m_aiTimer.stop();
        m_trickPauseTimer.stop();
        m_watchdog.stop();
        setVisualPhase(Idle);
        m_active = false;
        emit resetVisuals();
        emit stateChanged();
    }
    emit networkChanged();
    if (!notice.isEmpty())
        emit networkNotice(notice);
}
