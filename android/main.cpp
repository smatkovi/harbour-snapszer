#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTranslator>

#include "BtLink.h"
#include "GameEngine.h"
#include "LanSession.h"
#include "MultiEngine.h"
#include "ScreenHelper.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Same settings location as the Sailfish build (AppConfigLocation).
    QCoreApplication::setOrganizationName(QStringLiteral("org.edp17"));
    QCoreApplication::setApplicationName(QStringLiteral("harbour-snapszer"));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("harbour-snapszer"), QStringLiteral("-"),
                        QStringLiteral(":/i18n")))
        app.installTranslator(&translator);

    // Declared before the QML engine so it outlives every binding to it.
    GameEngine engine;
    MultiEngine multi(&engine);
    LanBrowser browser;
    BtDevices bluetooth;
    ScreenHelper screen;
    const auto updateScreen = [&]() {
        screen.setLanActive(engine.networkGame() || multi.networkGame() || engine.lanBusy() || multi.lanBusy());
    };
    QObject::connect(&engine, &GameEngine::networkChanged, &screen, updateScreen);
    QObject::connect(&multi, &MultiEngine::networkChanged, &screen, updateScreen);

    // A browser has no raw sockets, so the LAN pages stay out of reach there.
#ifdef Q_OS_WASM
    const bool lanAvailable = false;
#else
    const bool lanAvailable = true;
#endif

    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("lanAvailable"), lanAvailable);
    // The About page names a different licence and a different set of features
    // in the browser, where Qt is used under the GPL rather than the LGPL.
#ifdef Q_OS_WASM
    qml.rootContext()->setContextProperty(QStringLiteral("webEdition"), true);
#else
    qml.rootContext()->setContextProperty(QStringLiteral("webEdition"), false);
#endif
    qml.rootContext()->setContextProperty(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    qml.rootContext()->setContextProperty(QStringLiteral("snapszerEngine"), &engine);
    qml.rootContext()->setContextProperty(QStringLiteral("multiEngine"), &multi);
    qml.rootContext()->setContextProperty(QStringLiteral("lanBrowser"), &browser);
    // Android has no RFCOMM for apps; this one answers "no Bluetooth" and the
    // pages that use it stay hidden.
    qml.rootContext()->setContextProperty(QStringLiteral("btDevices"), &bluetooth);
    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    qml.loadFromModule("Snapszer", "Main");
    return app.exec();
}
