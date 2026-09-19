# Snapszer for MeeGo Harmattan (Nokia N9)

The same game, computer opponents and LAN play as the Sailfish OS and
Android editions, built for the Nokia N9: Qt 4.7.4, QtQuick 1.1 and the
`com.nokia.meego` components. Portrait and landscape.

## Layout of this directory

| Path | Purpose |
| --- | --- |
| `qml/` | The N9 user interface. Files keep the names of `sailfish/` so the Hungarian catalogue applies; `MultiTable.qml` and `MultiRules.qml` are QtQuick 1.1 ports of `qml-common/`. |
| `main.cpp` | Entry point: `QDeclarativeView`, the engines as context properties, translations. `SNAPSZER_SHOT_DIR` and `SNAPSZER_OPEN` are debugging aids (screenshots every 2.5 s, open a page or start a table). |
| `compat/` | Qt 4 stand-ins for the Qt 5 classes the shared code uses: `QJsonDocument`/`QJsonObject` over QtScript's JSON, `QStandardPaths`, `QGuiApplication`, and the `QStringLiteral` macros (`qt4compat.h`, force-included). |
| `tests/` | Qt 4 editions of the LAN tests (Qt 4 cannot connect lambdas). |
| `toolchain.sh` | Builds GCC 14 for `arm-none-linux-gnueabi` against the MADDE sysroot. |
| `build.sh` | Builds the N9 binary (`arm`), a desktop build for the Qt Simulator (`x86`), or the tests (`tests-qt4`, `tests-qt5`). |
| `build-deb.sh` | Packages `build/meego/arm` as `harbour-snapszer_<version>_armel.deb` (`mkdeb.py`, no dpkg needed). |
| `remote-build.sh` | Runs all of the above from the phone on the build machine and fetches the package. |
| `run-x86.sh` | Runs the desktop build inside the Qt Simulator and collects screenshots. |

## Why a modern GCC

Harmattan's own toolchain (MADDE, GCC 4.4) cannot compile the C++17 game
core. `toolchain.sh` builds GCC 14.2 with binutils 2.43 against the MADDE
sysroot (glibc 2.10, Qt 4.7.4) and the binary links libstdc++ and libgcc
statically, so the device only needs its own Qt. Harmattan is armv7-a,
NEON, hard-float, but still uses `/lib/ld-linux.so.3` as the dynamic
linker, hence `-Wl,--dynamic-linker=/lib/ld-linux.so.3` in `build.sh`.
The toolchain takes about half an hour and lives in `/tmp` on the build
machine; `remote-build.sh` keeps a tarball on the phone and restores it
after a reboot.

## What differs in the shared code

* Lambdas in `connect()` became named private slots, connected by
  signature under Qt 4 and by pointer under Qt 5.
* `QTimer::singleShot(0, this, &Class::method)` became
  `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
* `QHostAddress::AnyIPv4`, `isLoopback()`, `toIPv4Address(bool*)`,
  `QByteArray::fromStdString/toStdString` have Qt 4 alternatives.
* `Q_ENUM` is Qt 5 only: moc 4.7 swallows the declaration after a
  `Q_ENUMS()`, and QML never needs the enum by name.

Qt 4 sockets are IPv4-only here, so the N9 does not host over IPv6.

## Translation caveat

`qsTr()` of this Qt's QML passes the source text through Latin-1. Keys of
the MeeGo QML are therefore ASCII; accented words, bullets and dashes are
added outside the key (`.arg("Betyár")`, `"• " + qsTr(...)`), and the C++
strings get `QTextCodec::setCodecForTr(UTF-8)` in `main.cpp`.

## Installing on the N9

Developer mode, then in a terminal or over SSH:

    devel-su dpkg -i harbour-snapszer_1.1.0_armel.deb

Settings and the autosave live in
`~/.config/org.edp17/harbour-snapszer/harbour-snapszer.conf`.
