// Headless LAN test for three- and four-player matches: one host, two guests
// (plus a computer seat at a four-player table), real TCP on localhost, and
// simulated card animations of different speed on every device.
#include "GameEngine.h"
#include "LanSession.h"
#include "MultiEngine.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <random>

using namespace Snapszer;
static std::mt19937 rng(99);
static int rnd(int n) { return int(rng() % unsigned(n)); }

static void simulateQml(MultiEngine* e, int maxDelay)
{
    QObject::connect(e, &MultiEngine::cardAnimationRequested, e, [e, maxDelay](const QString&, int) {
        QTimer::singleShot(rnd(maxDelay), e, [e]() { e->completeCardAnimation(); });
    });
    QObject::connect(e, &MultiEngine::trickAnimationRequested, e, [e, maxDelay](int) {
        QTimer::singleShot(rnd(maxDelay), e, [e]() { e->completeTrickAnimation(); });
    });
}

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
    for (const QVariant& c : e->hand())
        if (c.toMap().value("playable").toBool())
            playable.append(c.toMap());
    if (playable.isEmpty())
        return;
    const QVariantMap card = playable[rnd(playable.size())];
    e->act(e->discarding() ? "discard" : "play", card.value("key").toInt(),
           card.value("marriage").toInt() > 0 && rnd(2));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    const int players = argc > 1 ? std::atoi(argv[1]) : 4;
    const int rules = argc > 2 ? std::atoi(argv[2]) : 1;
    rng.seed(argc > 3 ? std::atoi(argv[3]) : 5);

    GameEngine settings;
    settings.setAnimationsEnabled(true);
    settings.setAiPlayDelay(150);
    MultiEngine host(&settings), guestA(&settings), guestB(&settings);
    host.setRules3(rules); host.setRules4(rules);
    simulateQml(&host, 20);
    simulateQml(&guestA, 90);
    simulateQml(&guestB, 40);

    int notices = 0;
    for (MultiEngine* e : {&host, &guestA, &guestB})
        QObject::connect(e, &MultiEngine::networkNotice, [&](const QString& t) { ++notices; std::printf("notice: %s\n", qPrintable(t)); });

    QElapsedTimer t;
    host.hostLanGame(players);

    // A two-player app typing the host's address is told the table size.
    GameEngine twoPlayer;
    int redirectedTo = 0;
    QObject::connect(&twoPlayer, &GameEngine::lanRedirect, [&](const QString& address, int size) {
        std::printf("redirect from two-player join to %s with %d players\n", qPrintable(address), size);
        redirectedTo = size;
    });
    twoPlayer.joinLanGame("[::1]");
    t.start();
    while (t.elapsed() < 3000 && redirectedTo == 0)
        app.processEvents(QEventLoop::AllEvents, 20);
    if (redirectedTo != players) {
        std::printf("FAIL: no redirect to the %d-player table\n", players);
        return 1;
    }

    guestA.joinLanGame("::1"); // IPv6
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
                qPrintable([&] { QStringList n; for (auto s : host.seats()) n << s.toMap()["name"].toString(); return n.join(","); }()),
                guestA.localSeatOnHost(),
                qPrintable([&] { QStringList n; for (auto s : guestA.seats()) n << s.toMap()["name"].toString(); return n.join(","); }()));

    int mismatch = 0, maxMismatch = 0, checks = 0, rounds = 0;
    bool wasOver = false;
    QTimer driver, checker;
    QObject::connect(&driver, &QTimer::timeout, [&] { drive(&host); drive(&guestA); drive(&guestB); });
    QObject::connect(&checker, &QTimer::timeout, [&] {
        if (host.roundOver() && !wasOver)
            ++rounds;
        wasOver = host.roundOver();
        const bool idle = host.visualPhase() == 0 && guestA.visualPhase() == 0 && guestB.visualPhase() == 0;
        if (!idle) {
            mismatch = 0;
            return;
        }
        ++checks;
        MultiCore a = host.core(), b = host.core();
        a.rotateSeats(guestA.localSeatOnHost());
        b.rotateSeats(guestB.localSeatOnHost());
        const bool same = a.serializeState() == guestA.core().serializeState()
                && b.serializeState() == guestB.core().serializeState();
        if (!same) {
            maxMismatch = std::max(maxMismatch, ++mismatch);
            if (mismatch == 40) {
                std::printf("FAIL: persistent mismatch\n");
                std::exit(1);
            }
        } else {
            mismatch = 0;
        }
    });
    driver.start(15);
    checker.start(10);
    t.restart();
    const int duration = argc > 4 ? std::atoi(argv[4]) : 60000;
    while (t.elapsed() < duration && host.networkGame() && guestA.networkGame() && guestB.networkGame())
        app.processEvents(QEventLoop::AllEvents, 20);
    driver.stop();
    std::printf("rounds %d, idle checks %d, max transient mismatch %d, connected %d/%d/%d\n", rounds, checks,
                maxMismatch, host.networkGame(), guestA.networkGame(), guestB.networkGame());
    if (!host.networkGame() || !guestA.networkGame() || !guestB.networkGame() || rounds < 2) {
        std::printf("FAIL\n");
        return 1;
    }

    // A guest leaves: the computer takes the seat and the match goes on.
    guestB.cancelLan();
    t.restart();
    while (t.elapsed() < 2000 && notices == 0)
        app.processEvents(QEventLoop::AllEvents, 20);
    const int roundsBefore = rounds;
    driver.start(15);
    t.restart();
    while (t.elapsed() < 15000 && rounds < roundsBefore + 1)
        app.processEvents(QEventLoop::AllEvents, 20);
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
