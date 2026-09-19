// Qt 4 edition of tests/lan_multiplayer.cpp for the MeeGo build: one host,
// two guests and a computer seat over real TCP on localhost, with simulated
// card animations of different speed. Signal handlers are slots of a helper
// object because Qt 4 cannot connect lambdas.
#include "GameEngine.h"
#include "LanSession.h"
#include "MultiEngine.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace Snapszer;
static std::mt19937 rng(99);
static int rnd(int n) { return int(rng() % unsigned(n)); }

// Answers the engine's animation requests after a random delay, like QML.
class QmlSimulator : public QObject
{
    Q_OBJECT
public:
    QmlSimulator(MultiEngine* engine, int maxDelay) : m_engine(engine), m_maxDelay(maxDelay)
    {
        m_card.setSingleShot(true);
        m_trick.setSingleShot(true);
        connect(engine, SIGNAL(cardAnimationRequested(QString,int)), this, SLOT(onCard()));
        connect(engine, SIGNAL(trickAnimationRequested(int)), this, SLOT(onTrick()));
        connect(&m_card, SIGNAL(timeout()), this, SLOT(fireCard()));
        connect(&m_trick, SIGNAL(timeout()), this, SLOT(fireTrick()));
        connect(engine, SIGNAL(networkNotice(QString)), this, SLOT(onNotice(QString)));
    }
    int notices() const { return m_notices; }
private slots:
    void onCard() { m_card.start(rnd(m_maxDelay)); }
    void onTrick() { m_trick.start(rnd(m_maxDelay)); }
    void fireCard() { m_engine->completeCardAnimation(); }
    void fireTrick() { m_engine->completeTrickAnimation(); }
    void onNotice(const QString& text) { ++m_notices; std::printf("notice: %s\n", qPrintable(text)); }
private:
    MultiEngine* m_engine;
    int m_maxDelay;
    QTimer m_card;
    QTimer m_trick;
    int m_notices = 0;
};

class RedirectCatcher : public QObject
{
    Q_OBJECT
public:
    int players = 0;
public slots:
    void onRedirect(const QString& address, int size)
    {
        std::printf("redirect from two-player join to %s with %d players\n", qPrintable(address), size);
        players = size;
    }
};

static void drive(MultiEngine* e)
{
    if (!e->active() || e->visualPhase() != MultiEngine::Idle)
        return;
    if (e->roundOver()) {
        if (rnd(4) == 0)
            e->matchOver() ? e->newMatch() : e->nextRound();
        return;
    }
    if (e->canClaim() && rnd(2)) {
        e->claim();
        return;
    }
    const QVariantList options = e->options();
    if (!options.isEmpty()) {
        const QVariantMap o = options[rnd(options.size())].toMap();
        e->act(o.value("type").toString(), o.value("value").toInt());
        return;
    }
    if (e->choosingCard()) {
        e->act("call", rnd(4) * 100 + 9 + rnd(6));
        return;
    }
    QList<QVariantMap> playable;
    const QVariantList hand = e->hand();
    for (int i = 0; i < hand.size(); ++i)
        if (hand[i].toMap().value("playable").toBool())
            playable.append(hand[i].toMap());
    if (playable.isEmpty())
        return;
    const QVariantMap card = playable[rnd(playable.size())];
    e->act(e->discarding() ? "discard" : "play", card.value("key").toInt(),
           card.value("marriage").toInt() > 0 && rnd(2));
}

static QString names(const QVariantList& seats)
{
    QStringList n;
    for (int i = 0; i < seats.size(); ++i)
        n << seats[i].toMap().value("name").toString();
    return n.join(",");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const int players = argc > 1 ? std::atoi(argv[1]) : 4;
    const int rules = argc > 2 ? std::atoi(argv[2]) : 1;
    rng.seed(argc > 3 ? std::atoi(argv[3]) : 5);

    GameEngine settings;
    settings.setAnimationsEnabled(true);
    settings.setAiPlayDelay(150);
    MultiEngine host(&settings), guestA(&settings), guestB(&settings);
    host.setRules3(rules); host.setRules4(rules);
    QmlSimulator simHost(&host, 20), simA(&guestA, 90), simB(&guestB, 40);

    QElapsedTimer t;
    host.hostLanGame(players);

    // A two-player app typing the host's address is told the table size.
    GameEngine twoPlayer;
    RedirectCatcher redirect;
    QObject::connect(&twoPlayer, SIGNAL(lanRedirect(QString,int)), &redirect, SLOT(onRedirect(QString,int)));
    twoPlayer.joinLanGame("127.0.0.1");
    t.start();
    while (t.elapsed() < 3000 && redirect.players == 0)
        app.processEvents(QEventLoop::AllEvents, 20);
    if (redirect.players != players) {
        std::printf("FAIL: no redirect to the %d-player table\n", players);
        return 1;
    }

    guestA.joinLanGame("127.0.0.1");
    t.restart();
    while (t.elapsed() < 3000 && host.lobby().size() && !host.lobby()[1].toMap()["taken"].toBool())
        app.processEvents(QEventLoop::AllEvents, 20);
    guestB.joinLanGame("127.0.0.1");
    t.restart();
    while (t.elapsed() < 3000 && !host.lobby()[2].toMap()["taken"].toBool())
        app.processEvents(QEventLoop::AllEvents, 20);
    app.processEvents(QEventLoop::AllEvents, 200);
    std::printf("guest A status: %s\n", qPrintable(guestA.networkStatus()));
    host.startLanMatch();
    t.restart();
    while (t.elapsed() < 3000 && !(guestA.active() && guestB.active()))
        app.processEvents(QEventLoop::AllEvents, 20);
    if (!guestA.active() || !guestB.active()) {
        std::printf("FAIL: guests did not start\n");
        return 1;
    }
    std::printf("started %s: host sees %s | guestA seat %d sees %s\n", qPrintable(host.rulesName()),
                qPrintable(names(host.seats())), guestA.localSeatOnHost(), qPrintable(names(guestA.seats())));

    int mismatch = 0, maxMismatch = 0, checks = 0, rounds = 0;
    bool wasOver = false;
    QElapsedTimer driveClock, checkClock;
    driveClock.start(); checkClock.start();
    bool driving = true;
    const int duration = argc > 4 ? std::atoi(argv[4]) : 60000;

    // One polling loop replaces the two QTimers of the Qt 5 test.
    #define STEP() do { \
        app.processEvents(QEventLoop::AllEvents, 5); \
        if (driving && driveClock.elapsed() >= 15) { driveClock.restart(); drive(&host); drive(&guestA); drive(&guestB); } \
        if (checkClock.elapsed() >= 10) { \
            checkClock.restart(); \
            if (host.roundOver() && !wasOver) ++rounds; \
            wasOver = host.roundOver(); \
            const bool idle = host.visualPhase() == 0 && guestA.visualPhase() == 0 && guestB.visualPhase() == 0; \
            if (!idle) { mismatch = 0; } else { \
                ++checks; \
                MultiCore a = host.core(), b = host.core(); \
                a.rotateSeats(guestA.localSeatOnHost()); \
                b.rotateSeats(guestB.localSeatOnHost()); \
                const bool same = a.serializeState() == guestA.core().serializeState() \
                    && b.serializeState() == guestB.core().serializeState(); \
                if (!same) { maxMismatch = std::max(maxMismatch, ++mismatch); \
                    if (mismatch == 40) { std::printf("FAIL: persistent mismatch\n"); return 1; } \
                } else { mismatch = 0; } \
            } \
        } \
    } while (0)

    t.restart();
    while (t.elapsed() < duration && host.networkGame() && guestA.networkGame() && guestB.networkGame())
        STEP();
    driving = false;
    std::printf("rounds %d, idle checks %d, max transient mismatch %d, connected %d/%d/%d\n", rounds, checks,
                maxMismatch, host.networkGame(), guestA.networkGame(), guestB.networkGame());
    if (!host.networkGame() || !guestA.networkGame() || !guestB.networkGame() || rounds < 2) {
        std::printf("FAIL\n");
        return 1;
    }

    // A guest leaves: the computer takes the seat and the match goes on.
    guestB.cancelLan();
    t.restart();
    while (t.elapsed() < 2000 && simHost.notices() + simA.notices() == 0)
        STEP();
    const int roundsBefore = rounds;
    driving = true;
    t.restart();
    while (t.elapsed() < 15000 && rounds < roundsBefore + 1)
        STEP();
    std::printf("after guest B left: rounds %d (before %d), host active %d\n", rounds, roundsBefore, host.active());
    if (rounds < roundsBefore + 1) {
        std::printf("FAIL: match stalled after a guest left\n");
        return 1;
    }
    host.cancelLan();
    t.restart();
    while (t.elapsed() < 3000 && guestA.networkGame())
        app.processEvents(QEventLoop::AllEvents, 20);
    std::printf("host left: guest A network %d active %d status '%s'\n", guestA.networkGame(), guestA.active(),
                qPrintable(guestA.networkStatus()));
    if (guestA.networkGame()) {
        std::printf("FAIL\n");
        return 1;
    }
    std::printf("ALL OK\n");
    return 0;
}

#include "lan_multiplayer_qt4.moc"
