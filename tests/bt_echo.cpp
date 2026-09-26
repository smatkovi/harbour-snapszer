// Bare RFCOMM check for two devices: no game, no session, only BtLink.
//
//   device A:  bt_echo listen
//   device B:  bt_echo connect 40:98:4E:AD:BD:42
//
// The listener echoes every line back; the caller sends three and expects
// them back. Used to tell a broken link from a broken game protocol.
#include "BtLink.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString mode = argc > 1 ? QString::fromLatin1(argv[1]) : QString();
    const QString peer = argc > 2 ? QString::fromLatin1(argv[2]) : QString();
    std::printf("adapter %s available %d\n", qPrintable(Bt::localAddress()), int(Bt::available()));

    if (mode == QLatin1String("listen")) {
        RfcommServer server;
        QString error;
        if (!server.listen(Bt::Channel, &error)) {
            std::printf("FAIL listen: %s\n", qPrintable(error));
            return 1;
        }
        std::printf("listening on channel %d\n", Bt::Channel);
        RfcommSocket* client = 0;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 60000) {
            app.processEvents(QEventLoop::AllEvents, 20);
            if (!client) {
                client = server.findChild<RfcommSocket*>();
                if (client)
                    std::printf("accepted from %s\n", qPrintable(client->peerAddress()));
                continue;
            }
            while (client->bytesAvailable() > 0) {
                const QByteArray line = client->readAll();
                std::printf("echo %d bytes: %s", int(line.size()), line.constData());
                std::fflush(stdout);
                client->write(line);
                client->flush();
            }
            if (client->isUnconnected()) {
                std::printf("peer gone after %d ms\n", int(t.elapsed()));
                break;
            }
        }
        std::printf(client ? "LISTEN OK\n" : "FAIL: nobody connected\n");
        return client ? 0 : 1;
    }

    if (mode != QLatin1String("connect") || Bt::normalizeAddress(peer).isEmpty()) {
        std::printf("usage: %s listen | connect <address>\n", argv[0]);
        return 2;
    }

    RfcommSocket socket;
    socket.connectToDevice(peer, Bt::Channel);
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 30000 && !socket.isConnected())
        app.processEvents(QEventLoop::AllEvents, 20);
    if (!socket.isConnected()) {
        std::printf("FAIL: no connection after %d ms\n", int(t.elapsed()));
        return 1;
    }
    std::printf("connected after %d ms\n", int(t.elapsed()));
    int got = 0;
    for (int i = 0; i < 3; ++i) {
        socket.write(QByteArray("hello ") + QByteArray::number(i) + "\n");
        socket.flush();
        QElapsedTimer round;
        round.start();
        while (round.elapsed() < 5000 && socket.bytesAvailable() == 0)
            app.processEvents(QEventLoop::AllEvents, 20);
        const QByteArray back = socket.readAll();
        std::printf("round %d: %d bytes back in %d ms: %s", i, int(back.size()),
                    int(round.elapsed()), back.constData());
        if (!back.isEmpty())
            ++got;
    }
    std::printf("%s\n", got == 3 ? "ECHO OK" : "FAIL: lines lost");
    return got == 3 ? 0 : 1;
}
