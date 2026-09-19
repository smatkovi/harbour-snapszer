#pragma once

#include <QSettings>

#include "GameEngine.h"

// The single place that decides where preferences and the autosaved match are
// kept. Every platform but the browser uses an explicit file path, because the
// Sailjail sandbox on Sailfish OS requires the settings identity to match the
// desktop file. WebAssembly has no writable file system that survives a page
// reload, so there the organisation/application constructor is used, which Qt
// maps onto the browser's local storage for the page.
class AppSettings : public QSettings
{
public:
    AppSettings()
#ifdef Q_OS_WASM
        : QSettings(QStringLiteral("org.edp17"), QStringLiteral("harbour-snapszer"))
#else
        : QSettings(GameEngine::settingsFilePath(), QSettings::NativeFormat)
#endif
    {
    }
};
