#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTranslator>

#include "GameEngine.h"
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
    ScreenHelper screen;
    QObject::connect(&engine, &GameEngine::networkChanged, &screen, [&]() {
        screen.setKeepScreenOn(engine.networkGame());
    });

    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("snapszerEngine"), &engine);
    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    qml.loadFromModule("Snapszer", "Main");
    return app.exec();
}
