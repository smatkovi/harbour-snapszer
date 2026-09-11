#pragma once

#include <QObject>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

// Keeps the display on while a LAN game runs, so the connection is not lost
// while the opponent thinks.
class ScreenHelper : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    void setKeepScreenOn(bool keep)
    {
        if (keep == m_keepScreenOn)
            return;
        m_keepScreenOn = keep;
#ifdef Q_OS_ANDROID
        QNativeInterface::QAndroidApplication::runOnAndroidMainThread([keep]() {
            QJniObject activity(QNativeInterface::QAndroidApplication::context().object());
            QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
            if (!window.isValid())
                return;
            const jint flagKeepScreenOn = 128; // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
            if (keep)
                window.callMethod<void>("addFlags", "(I)V", flagKeepScreenOn);
            else
                window.callMethod<void>("clearFlags", "(I)V", flagKeepScreenOn);
        });
#endif
    }

private:
    bool m_keepScreenOn = false;
};
