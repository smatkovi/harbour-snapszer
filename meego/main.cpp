// Entry point of the MeeGo Harmattan (Nokia N9) edition. Qt 4.7 with
// QtQuick 1.1: the same engines as the other editions, exposed to the QML
// under meego/qml through the root context.
#include <QApplication>
#include <QDeclarativeComponent>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QDeclarativeError>
#include <QDeclarativeView>
#include <QDir>
#include <QGraphicsObject>
#include <QLocale>
#include <QMetaObject>
#include <QImage>
#include <QPainter>
#include <QTextCodec>
#include <QTimer>
#include <QTranslator>
#include <QVariant>
#include <QWidget>
#include <QUrl>
#include <cstdio>

#include "BtLink.h"
#include "GameEngine.h"
#include "LanSession.h"
#include "MultiEngine.h"

// Debug aid: with SNAPSZER_SHOT_DIR set, a full-size PNG of the view is
// written there every 2.5 seconds (shot-1.png, shot-2.png, ...). Works on the
// device and inside the Qt Simulator, where the window is shown scaled down.
class ScreenshotTimer : public QObject
{
    Q_OBJECT
public:
    ScreenshotTimer(QWidget* view, const QString& dir)
        : QObject(view), m_view(view), m_dir(dir), m_count(0)
    {
        m_timer.setInterval(2500);
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(shoot()));
        m_timer.start();
    }
private slots:
    void shoot()
    {
        const QString file = QString::fromLatin1("%1/shot-%2.png").arg(m_dir).arg(++m_count);
        QImage image(m_view->size(), QImage::Format_ARGB32);
        image.fill(0xff000000);
        QPainter painter(&image);
        m_view->render(&painter);
        painter.end();
        if (!image.save(file))
            qWarning("screenshot: cannot write %s", qPrintable(file));
    }
private:
    QWidget* m_view;
    QString m_dir;
    QTimer m_timer;
    int m_count;
};

// The N9 has no gdb and drops core files: print a raw backtrace instead,
// which addr2line on the build machine can resolve against the unstripped
// build/meego/arm/harbour-snapszer.
#if defined(__linux__) && !defined(Q_OS_ANDROID)
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
static void crashHandler(int sig)
{
    void* frames[64];
    const int n = backtrace(frames, 64);
    const char head[] = "harbour-snapszer: fatal signal, backtrace:\n";
    if (write(2, head, sizeof(head) - 1) < 0) {}
    backtrace_symbols_fd(frames, n, 2);
    signal(sig, SIG_DFL);
    raise(sig);
}
static void installCrashHandler()
{
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    signal(SIGBUS, crashHandler);
    signal(SIGILL, crashHandler);
}
#else
static void installCrashHandler() {}
#endif

int main(int argc, char* argv[])
{
    installCrashHandler();
    QApplication app(argc, argv);
    // Qt 4 hands untranslated tr()/qsTr() sources through Latin-1; the
    // sources contain bullets, dashes and accents.
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    // Same settings location as the other editions.
    QCoreApplication::setOrganizationName(QString::fromLatin1("org.edp17"));
    QCoreApplication::setApplicationName(QString::fromLatin1("harbour-snapszer"));

    // Installed as /opt/harbour-snapszer/{bin,qml,images,icons,translations};
    // SNAPSZER_ROOT overrides that for desktop runs.
    QString root = QString::fromLocal8Bit(qgetenv("SNAPSZER_ROOT"));
    if (root.isEmpty())
        root = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QString::fromLatin1(".."));

    QTranslator translator;
    if (translator.load(QString::fromLatin1("harbour-snapszer-") + QLocale::system().name(),
                        root + QString::fromLatin1("/translations")))
        app.installTranslator(&translator);

    // Declared before the view so they outlive every binding to them.
    GameEngine engine;
    MultiEngine multi(&engine);
    LanBrowser browser;
    BtDevices bluetooth;

    QDeclarativeView view;
    view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
    view.rootContext()->setContextProperty(QString::fromLatin1("snapszerEngine"), &engine);
    view.rootContext()->setContextProperty(QString::fromLatin1("multiEngine"), &multi);
    view.rootContext()->setContextProperty(QString::fromLatin1("lanBrowser"), &browser);
    view.rootContext()->setContextProperty(QString::fromLatin1("btDevices"), &bluetooth);
    view.setSource(QUrl::fromLocalFile(root + QString::fromLatin1("/qml/harbour-snapszer.qml")));
    if (view.status() == QDeclarativeView::Error) {
        const QList<QDeclarativeError> errors = view.errors();
        for (int i = 0; i < errors.size(); ++i)
            std::fprintf(stderr, "%s\n", qPrintable(errors[i].toString()));
        return 1;
    }

    const QByteArray shotDir = qgetenv("SNAPSZER_SHOT_DIR");
    if (!shotDir.isEmpty())
        new ScreenshotTimer(&view, QString::fromLocal8Bit(shotDir));
    const QByteArray open = qgetenv("SNAPSZER_OPEN");
    if (!open.isEmpty() && view.rootObject()) {
        QMetaObject::invokeMethod(view.rootObject(), "openPage", Qt::QueuedConnection,
                                  Q_ARG(QVariant, QVariant(QString::fromLocal8Bit(open))));
    }

    // The N9 runs applications full screen; a desktop run gets a window of
    // the N9's portrait size.
    if (qgetenv("SNAPSZER_WINDOWED").isEmpty()) {
        view.showFullScreen();
    } else {
        view.resize(480, 854);
        view.setVisible(true); // show() is not exported by every Qt 4 build
    }
    return app.exec();
}

#include "main.moc"
