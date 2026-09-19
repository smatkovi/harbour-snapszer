// Keeps the display on while a LAN game runs; loaded only then. On a device
// without QtMobility the Loader simply fails and the game goes on.
import QtQuick 1.1
import QtMobility.systeminfo 1.1

ScreenSaver {
    screenSaverInhibited: true
}
