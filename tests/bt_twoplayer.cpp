// Two-player game over Bluetooth, played between two real devices: the LAN
// tests cannot cover RFCOMM because it has no loopback.
//
//   device A:  bt_twoplayer host
//   device B:  bt_twoplayer guest 40:98:4E:AD:BD:42   (A's address)
//
// Both sides play their own seat as fast as the table allows and print their
// view of it at the end; tools/bt-test.sh starts the two and compares the
// two views, which must be mirror images of each other.
//
// Polling loop and no lambdas, so the same file builds under Qt 4.7 for the
// N9/N950 and under Qt 5 for Sailfish OS.
#include "BtLink.h"
#include "GameEngine.h"
#include "LanSession.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QStringList>

#include <cstdio>
#include <cstdlib>
#include <random>

static std::mt19937 rng(7);
static int rnd(int n) { return int(rng() % unsigned(n)); }

static QStringList ids(const QVariantList& cards)
{
    QStringList r;
    for (int i = 0; i < cards.size(); ++i)
        r << cards[i].toMap().value("id").toString();
    return r;
}

// The host's view and the guest's view of the same table are mirror images:
// what one calls its own hand the other calls the opponent's.
static QString snap(GameEngine* e, bool mirror)
{
    QStringList a = ids(e->playerHand()), b = ids(e->cpuHand());
    int pp = e->playerPoints(), cp = e->cpuPoints(), turn = e->turnPlayer();
    if (mirror) { std::swap(a, b); std::swap(pp, cp); turn = 1 - turn; }
    return a.join(",") + "|" + b.join(",") + QString("|%1:%2|t%3|").arg(pp).arg(cp).arg(turn)
        + ids(e->trickCards()).join(",") + e->trumpCard().value("id").toString();
}

static void pump(QCoreApplication& app, int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        app.processEvents(QEventLoop::AllEvents, 10);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // Keep settings and autosaves of the test out of the real ones. Qt 4 has
    // no test mode; there the scratch HOME of the build script does it.
    QStandardPaths::setTestModeEnabled(true);
#endif
    const QString role = argc > 1 ? QString::fromLatin1(argv[1]) : QString();
    const QString peer = argc > 2 ? QString::fromLatin1(argv[2]) : QString();
    const int seconds = argc > 3 ? atoi(argv[3]) : 40;
    if (role != QLatin1String("host") && role != QLatin1String("guest")) {
        std::printf("usage: %s host | guest <address> [seconds]\n", argv[0]);
        return 2;
    }

    std::printf("local adapter: %s (available %d)\n",
                qPrintable(Bt::localAddress()), int(Bt::available()));

    GameEngine engine;
    engine.setAnimationsEnabled(false);
    engine.setPaused(true);
    engine.start();

    if (role == QLatin1String("host")) {
        engine.hostLanGame();
        std::printf("hosting on RFCOMM channel %d: %s%s\n", Bt::Channel,
                    engine.bluetoothHosting() ? "open" : "NOT open",
                    qPrintable(engine.bluetoothError().isEmpty()
                               ? QString() : QString::fromLatin1(" (%1)").arg(engine.bluetoothError())));
    } else {
        if (Bt::normalizeAddress(peer).isEmpty()) {
            std::printf("FAIL: '%s' is not a Bluetooth address\n", qPrintable(peer));
            return 2;
        }
        pump(app, 500);
        engine.joinBluetoothGame(peer);
        std::printf("connecting to %s\n", qPrintable(Bt::normalizeAddress(peer)));
    }

    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 45000 && !engine.networkGame())
        app.processEvents(QEventLoop::AllEvents, 20);
    if (!engine.networkGame()) {
        std::printf("FAIL: no connection (%s)\n", qPrintable(engine.networkStatus()));
        return 1;
    }
    std::printf("connected after %d ms\n", int(t.elapsed()));

    int actions = 0, matches = 0;
    bool over = false;
    t.restart();
    while (t.elapsed() < seconds * 1000 && engine.networkGame()) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (engine.visualPhase() != 0)
            continue;
        if (engine.roundOver()) {
            if (rnd(3) == 0) {
                if (engine.matchOver())
                    engine.newMatch();
                else
                    engine.nextRound();
            }
            continue;
        }
        if (!engine.playerInputEnabled())
            continue;
        if (engine.canClaim66() && rnd(2)) {
            engine.claim66();
            continue;
        }
        for (int i = 0; i < 5; ++i) {
            if (engine.isPlayerCardPlayable(i)) {
                engine.playCard(i, engine.marriagePointsForCard(i) > 0);
                ++actions;
                break;
            }
        }
        if (engine.matchOver() && !over)
            ++matches;
        over = engine.matchOver();
    }

    // Read the table off before anything is torn down: when the other side
    // stops first, the engine falls back to the local game against the
    // computer and the state to compare would be gone.
    const int playedMs = int(t.elapsed());
    const bool stillConnected = engine.networkGame();
    const QString view = snap(&engine, role == QLatin1String("guest"));
    const QString status = engine.networkStatus();
    std::printf("actions %d matches %d played %d ms connected %d (%s)\n", actions, matches,
                playedMs, int(stillConnected), qPrintable(status));
    std::printf("SNAP %s\n", qPrintable(view));
    // The side that stops first pulls the link down, so the host regularly
    // ends unconnected: what counts is that the match ran over Bluetooth.
    const bool ok = actions > 20 && playedMs > 10000;
    std::printf("%s\n", ok ? "BT OK" : "FAIL: too few actions or the link dropped early");
    return ok ? 0 : 1;
}
