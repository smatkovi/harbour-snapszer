// Qt 4 edition of tests/lan_twoplayer.cpp for the MeeGo build: same checks,
// but driven from a polling loop because Qt 4 cannot connect lambdas.
#include "GameEngine.h"
#include "LanSession.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QStringList>
#include <QUdpSocket>
#include <cstdio>
#include <cstdlib>
#include <random>

static std::mt19937 rng(3);
static int rnd(int n) { return int(rng() % unsigned(n)); }
static QStringList ids(const QVariantList& cards)
{
    QStringList r;
    for (int i = 0; i < cards.size(); ++i)
        r << cards[i].toMap().value("id").toString();
    return r;
}
static QString snap(GameEngine* e, bool mirror)
{
    QStringList a = ids(e->playerHand()), b = ids(e->cpuHand());
    int pp = e->playerPoints(), cp = e->cpuPoints(), turn = e->turnPlayer();
    if (mirror) { std::swap(a, b); std::swap(pp, cp); turn = 1 - turn; }
    return a.join(",") + "|" + b.join(",") + QString("|%1:%2|t%3|").arg(pp).arg(cp).arg(turn)
        + ids(e->trickCards()).join(",") + e->trumpCard().value("id").toString();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    GameEngine host, guest;
    host.setAnimationsEnabled(false); guest.setAnimationsEnabled(false);
    host.setPaused(true); guest.setPaused(true);
    host.start(); guest.start();
    host.hostLanGame();

    // An app of the first LAN release sends only the version 1 probe.
    QUdpSocket old;
    old.bind(QHostAddress(QHostAddress::Any), 0);
    old.writeDatagram("SNAPSZER-DISCOVER 1", QHostAddress::LocalHost, LanSession::DiscoveryPort);
    QElapsedTimer t; t.start();
    QByteArray reply;
    while (t.elapsed() < 2000 && reply.isEmpty()) {
        app.processEvents(QEventLoop::AllEvents, 20);
        if (old.hasPendingDatagrams()) {
            reply.resize(int(old.pendingDatagramSize()));
            old.readDatagram(reply.data(), reply.size());
        }
    }
    std::printf("v1 discovery reply: '%s'\n", reply.constData());

    guest.joinLanGame("127.0.0.1");
    t.restart();
    while (t.elapsed() < 3000 && !(host.networkGame() && guest.networkGame()))
        app.processEvents(QEventLoop::AllEvents, 20);
    if (!host.networkGame() || !guest.networkGame()) { std::printf("FAIL connect\n"); return 1; }

    int actions = 0, streak = 0, maxStreak = 0, matches = 0;
    bool over = false;
    GameEngine* engines[2] = { &host, &guest };
    t.restart();
    while (t.elapsed() < 25000 && host.networkGame() && guest.networkGame()) {
        app.processEvents(QEventLoop::AllEvents, 5);
        for (int k = 0; k < 2; ++k) {
            GameEngine* e = engines[k];
            if (e->visualPhase() != 0) continue;
            if (e->roundOver()) { if (rnd(3) == 0) e->matchOver() ? e->newMatch() : e->nextRound(); continue; }
            if (!e->playerInputEnabled()) continue;
            if (e->canClaim66() && rnd(2)) { e->claim66(); continue; }
            for (int i = 0; i < 5; ++i)
                if (e->isPlayerCardPlayable(i)) { e->playCard(i, e->marriagePointsForCard(i) > 0); ++actions; break; }
        }
        if (host.matchOver() && !over) ++matches;
        over = host.matchOver();
        if (host.visualPhase() == 0 && guest.visualPhase() == 0) {
            if (snap(&host, false) != snap(&guest, true)) maxStreak = std::max(maxStreak, ++streak); else streak = 0;
            if (streak > 30) { std::printf("FAIL mismatch\n"); return 1; }
        }
    }
    std::printf("actions %d matches %d max transient mismatch %d connected %d/%d\n", actions, matches, maxStreak,
                host.networkGame(), guest.networkGame());
    guest.cancelLan();
    t.restart();
    while (t.elapsed() < 3000 && host.networkGame()) app.processEvents(QEventLoop::AllEvents, 20);
    std::printf("after leave host network %d\n", host.networkGame());
    const bool ok = reply.startsWith("SNAPSZER-HOST 1 ") && actions > 200 && !host.networkGame();
    std::printf(ok ? "ALL OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
