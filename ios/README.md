# Snapszer on iOS

The iPhone/iPad edition. There is no separate source tree: the project in
`android/` is the portable Qt 6 one, the browser edition already builds it too
(`wasm/build.sh` points `qt-cmake` at `android/`), and iOS is the third shell
around the same `src/` and the same QML.

    export QT_HOST_PATH=/path/to/Qt/6.9.1/macos
    export QT_IOS=/path/to/Qt/6.9.1/ios      # or ext/qt-sim, see below
    sh ios/build.sh iphoneos                 # or iphonesimulator

What that needed, beyond an `if(IOS)` block with the bundle properties:

* **`ios/Info.plist.in`** with `NSLocalNetworkUsageDescription`. Without it
  iOS 14 and later refuse every connection on the local network *without ever
  asking the user*, and the LAN game simply finds nothing.
* **`ios/ScreenHelperIos.mm`** — one UIKit call. Android keeps the display on
  with a window flag and runs a foreground service so the app still gets packets
  in the background; on iOS only the first half has a counterpart
  (`idleTimerDisabled`), because an app in the background loses its sockets
  regardless.

## Two things do not come along

**Classic Bluetooth.** `src/BtLink.cpp` speaks RFCOMM over raw sockets, and iOS
gives no third-party app classic Bluetooth at all - only BLE, or the MFi
programme. The file compiles to its existing stub, the same one the Android and
browser editions use.

**Automatic LAN discovery.** It broadcasts UDP, and iOS 14 and later need
Apple's multicast entitlement for that, which is granted on request and cannot
be sideloaded. Typing the host's address works, which the LAN page already
supports.

## The simulator

Qt's binary packages for iOS ship plugin initialiser objects built for the
device only, so a simulator build cannot be linked against them - that is what
stopped the NSR Reader port from using the simulator at all.
`tools/build-qt-ios-sim.sh` (borrowed from there) builds qtbase, qtshadertools
and qtdeclarative for `iphonesimulator` instead. The first run takes well over
an hour; CI caches it.

## Onto a device

No Apple developer account is involved: CI produces an **unsigned** `.ipa`.
Install it with AltStore or Sideloadly using your own Apple ID - a free account
has to re-sign every 7 days.
