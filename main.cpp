#include <sailfishapp.h>

#include <QGuiApplication>
#include <QLocale>
#include <QQuickView>
#include <QQmlContext>
#include <QTranslator>
#include <QtQml>

#include "BtLink.h"
#include "GameEngine.h"
#include "LanSession.h"
#include "MultiEngine.h"

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    QQuickView *view = SailfishApp::createView();

    QTranslator translator;
    const QString translationDirectory = SailfishApp::pathTo(
        QStringLiteral("translations")).toLocalFile();
    if (translator.load(QLocale(), QStringLiteral("harbour-snapszer"),
                        QStringLiteral("-"), translationDirectory)) {
        app->installTranslator(&translator);
    }

    GameEngine *engine = new GameEngine(app);
    view->rootContext()->setContextProperty(QStringLiteral("snapszerEngine"), engine);
    view->rootContext()->setContextProperty(QStringLiteral("multiEngine"), new MultiEngine(engine, app));
    view->rootContext()->setContextProperty(QStringLiteral("lanBrowser"), new LanBrowser(app));
    view->rootContext()->setContextProperty(QStringLiteral("btDevices"), new BtDevices(app));

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-snapszer.qml")));
    view->show();
    return app->exec();
}
