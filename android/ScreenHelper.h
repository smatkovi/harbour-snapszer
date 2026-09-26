#pragma once

#include <QObject>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

#ifdef Q_OS_IOS
// UIApplication is Objective-C, so the one call lives in ios/ScreenHelperIos.mm.
void snapszer_keepScreenOn(bool on);
#endif

// While a LAN game is hosted, joined or played: keeps the display on and runs
// a foreground service (LanService.java). Without it Android drops incoming
// packets for the app as soon as it is in the background or the screen is off.
class ScreenHelper : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    void setLanActive(bool active)
    {
        if (active == m_lanActive)
            return;
        m_lanActive = active;
#ifdef Q_OS_ANDROID
        QJniObject context(QNativeInterface::QAndroidApplication::context().object());
        QJniObject::callStaticMethod<void>("org/edp17/snapszer/LanService", active ? "start" : "stop",
                                           "(Landroid/content/Context;)V", context.object());
        QNativeInterface::QAndroidApplication::runOnAndroidMainThread([active]() {
            QJniObject activity(QNativeInterface::QAndroidApplication::context().object());
            QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
            if (!window.isValid())
                return;
            const jint flagKeepScreenOn = 128; // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
            if (active)
                window.callMethod<void>("addFlags", "(I)V", flagKeepScreenOn);
            else
                window.callMethod<void>("clearFlags", "(I)V", flagKeepScreenOn);
        });
#endif
#ifdef Q_OS_IOS
        // No service and no wake lock on iOS: an app in the background gets no
        // sockets either way. Keeping the display awake while a LAN game runs
        // is the part that carries over.
        snapszer_keepScreenOn(active);
#endif
    }

private:
    bool m_lanActive = false;
};
