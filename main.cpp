#include <sailfishapp.h>

#include <QGuiApplication>
#include <QLocale>
#include <QQuickView>
#include <QQmlContext>
#include <QTranslator>
#include <QtQml>

#include "GameEngine.h"

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

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-snapszer.qml")));
    view->show();
    return app->exec();
}
