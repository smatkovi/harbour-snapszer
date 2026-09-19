#!/bin/sh
# Builds the MeeGo Harmattan (Nokia N9) edition of Snapszer. Runs on the
# build machine (see meego/README.md), inside the synced source tree:
#
#   meego/build.sh arm        N9 binary            -> build/meego/arm/harbour-snapszer
#   meego/build.sh x86        same code for x86_64 against the Qt Simulator's
#                             Qt 4.7.4, to run and look at it on the desktop
#   meego/build.sh tests-qt4  game and LAN tests against Qt 4.7.4, then runs them
#   meego/build.sh tests-qt5  the original tests against the system Qt 5, runs them
#   meego/build.sh tests-arm  the Qt 4 tests as ARM binaries, to run under qemu-arm
#                             (qemu-arm -L $SYSROOT build/meego/tests-arm/test_multicore)
#
# The ARM build needs the cross toolchain from meego/toolchain.sh (XGCC) and
# the MADDE sysroot (SYSROOT); moc and lrelease come from the Qt Simulator's
# Qt (SIMQT), which is the same Qt 4.7.4 as on the device.
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd)
MODE=${1:-arm}
XGCC=${XGCC:-/tmp/xgcc-harmattan}
SYSROOT=${SYSROOT:-$HOME/QtSDK/Madde/sysroots/harmattan_sysroot_10.2011.34-1_slim}
SIMQT=${SIMQT:-$HOME/QtSDK/Simulator/Qt/gcc}
DESKTOPQT=${DESKTOPQT:-$HOME/QtSDK/Desktop/Qt/4.8.1/gcc}
JOBS=${JOBS:-8}
OUT=$HERE/build/meego/$MODE
mkdir -p "$OUT"

ENGINE_SRC="src/GameCore.cpp src/GameEngine.cpp src/LanSession.cpp src/MultiCore.cpp src/MultiEngine.cpp"
MOC_HEADERS="src/GameEngine.h src/MultiEngine.h src/LanSession.h"

QT4_FLAGS="-std=gnu++17 -O2 -Wall -Wno-register -Wno-deprecated-declarations -Wno-nonnull \
 -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DQT_NO_DEBUG \
 -I$HERE/meego/compat -include $HERE/meego/compat/qt4compat.h -I$HERE/src"
QT4_MODULES="QtCore QtGui QtNetwork QtScript QtDeclarative"

case "$MODE" in
arm|tests-arm)
    CXX=$XGCC/bin/arm-none-linux-gnueabi-g++
    [ -x "$CXX" ] || { echo "cross compiler missing: $CXX (run meego/toolchain.sh)" >&2; exit 1; }
    MOC=$SIMQT/bin/moc
    QTINC=$SYSROOT/usr/include/qt4
    CXXFLAGS="--sysroot=$SYSROOT $QT4_FLAGS -I$QTINC"
    for m in $QT4_MODULES; do CXXFLAGS="$CXXFLAGS -I$QTINC/$m"; done
    # Hard-float Harmattan still uses ld-linux.so.3; GCC would ask for the
    # armhf loader name, which the N9 does not have.
    # --exclude-libs keeps the static libstdc++/libgcc private to the
    # binary: Qt on the device stays bound to its own (GCC 4.4) runtime
    # instead of our exported copies.
    LDFLAGS="--sysroot=$SYSROOT -static-libstdc++ -static-libgcc -Wl,-O1 -Wl,--as-needed \
 -Wl,--exclude-libs,ALL -Wl,--dynamic-linker=/lib/ld-linux.so.3"
    LIBS="-lQtDeclarative -lQtScript -lQtNetwork -lQtGui -lQtCore -lpthread"
    ;;
x86|tests-qt4)
    # The Qt Simulator's Qt 4.7.4 (the device's version). Its QtGui only
    # renders inside the Qt Simulator application, which is also the only
    # place the com.nokia.meego plugin loads on x86 (meego/run-x86.sh starts
    # it). X86QT=$DESKTOPQT links the plain desktop Qt 4.8.1 instead, enough
    # for the tests but not for the components.
    CXX=${CXX:-g++}
    MOC=$SIMQT/bin/moc
    QTINC=$SIMQT/include
    X86QT=${X86QT:-$SIMQT}
    CXXFLAGS="$QT4_FLAGS -I$QTINC"
    for m in $QT4_MODULES; do CXXFLAGS="$CXXFLAGS -I$QTINC/$m"; done
    LDFLAGS="-L$X86QT/lib -Wl,-rpath,$X86QT/lib"
    LIBS="-lQtDeclarative -lQtScript -lQtNetwork -lQtGui -lQtCore -lpthread"
    ;;
tests-qt5)
    CXX=${CXX:-g++}
    MOC=${MOC:-/usr/bin/moc}
    CXXFLAGS="-std=gnu++17 -O2 -Wall -fPIC -I$HERE/src $(pkg-config --cflags Qt5Core Qt5Gui Qt5Network)"
    LDFLAGS=
    LIBS="$(pkg-config --libs Qt5Core Qt5Gui Qt5Network) -lpthread"
    ;;
*)
    echo "usage: $0 arm|x86|tests-qt4|tests-qt5|tests-arm" >&2; exit 2 ;;
esac

# --- Makefile -------------------------------------------------------------
# One generated Makefile per mode keeps rebuilds incremental.
MK=$OUT/Makefile
{
    echo "CXX=$CXX"
    echo "MOC=$MOC"
    echo "CXXFLAGS=$CXXFLAGS"
    echo "LDFLAGS=$LDFLAGS"
    echo "LIBS=$LIBS"
    echo "SRC=$HERE"
    echo
    objs=
    for s in $ENGINE_SRC; do
        o=$(basename "$s" .cpp).o; objs="$objs $o"
        echo "$o: \$(SRC)/$s \$(SRC)/src/*.h"; printf '\t$(CXX) $(CXXFLAGS) -c $< -o $@\n'
    done
    for h in $MOC_HEADERS; do
        n=$(basename "$h" .h); objs="$objs moc_$n.o"
        echo "moc_$n.cpp: \$(SRC)/$h"; printf '\t$(MOC) $< -o $@\n'
        echo "moc_$n.o: moc_$n.cpp"; printf '\t$(CXX) $(CXXFLAGS) -c $< -o $@\n'
    done
    echo "ENGINE_OBJS=$objs"
    echo
    case "$MODE" in
    arm|x86)
        echo "all: harbour-snapszer"
        echo "main.moc: \$(SRC)/meego/main.cpp"; printf '\t$(MOC) $< -o $@\n'
        echo "main.o: \$(SRC)/meego/main.cpp main.moc \$(SRC)/src/*.h"; printf '\t$(CXX) $(CXXFLAGS) -I. -c $< -o $@\n'
        echo "harbour-snapszer: main.o \$(ENGINE_OBJS)"
        printf '\t$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)\n'
        ;;
    tests-qt4|tests-arm)
        echo "all: test_multicore lan_twoplayer lan_multiplayer"
        echo "test_multicore: \$(SRC)/tests/test_multicore.cpp GameCore.o MultiCore.o"
        printf '\t$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< GameCore.o MultiCore.o\n'
        echo "lan_twoplayer: \$(SRC)/meego/tests/lan_twoplayer_qt4.cpp \$(ENGINE_OBJS)"
        printf '\t$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(ENGINE_OBJS) $(LIBS)\n'
        echo "lan_multiplayer_qt4.moc: \$(SRC)/meego/tests/lan_multiplayer_qt4.cpp"; printf '\t$(MOC) $< -o $@\n'
        echo "lan_multiplayer: \$(SRC)/meego/tests/lan_multiplayer_qt4.cpp lan_multiplayer_qt4.moc \$(ENGINE_OBJS)"
        printf '\t$(CXX) $(CXXFLAGS) -I. $(LDFLAGS) -o $@ $< $(ENGINE_OBJS) $(LIBS)\n'
        ;;
    tests-qt5)
        echo "all: test_multicore lan_twoplayer lan_multiplayer"
        echo "test_multicore: \$(SRC)/tests/test_multicore.cpp GameCore.o MultiCore.o"
        printf '\t$(CXX) $(CXXFLAGS) -o $@ $< GameCore.o MultiCore.o\n'
        for t in lan_twoplayer lan_multiplayer; do
            echo "$t: \$(SRC)/tests/$t.cpp \$(ENGINE_OBJS)"
            printf '\t$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(ENGINE_OBJS) $(LIBS)\n'
        done
        ;;
    esac
} > "$MK"

nice make -C "$OUT" -j"$JOBS" all

# --- translations (app builds only) ----------------------------------------
case "$MODE" in
arm|x86)
    LRELEASE=$SIMQT/bin/lrelease
    mkdir -p "$OUT/translations"
    # Qt 4.7's lrelease only knows TS version 2.0; the catalogue says 2.1.
    sed 's/<TS version="2\.1"/<TS version="2.0"/' "$HERE/translations/harbour-snapszer-hu.ts" > "$OUT/harbour-snapszer-hu.ts"
    "$LRELEASE" -silent "$OUT/harbour-snapszer-hu.ts" -qm "$OUT/translations/harbour-snapszer-hu.qm"
    echo "== built $OUT/harbour-snapszer"
    ;;
tests-arm)
    echo "== built ARM tests in $OUT (run them with qemu-arm -L \$SYSROOT)"
    ;;
tests-qt4|tests-qt5)
    # Settings and autosaves of the tests stay in a scratch home directory.
    [ -z "$BUILD_ONLY" ] || { echo "== built tests in $OUT"; exit 0; }
    export HOME="$OUT/home"; mkdir -p "$HOME"
    cd "$OUT"
    run() {
        echo "== $*"
        if ! "$@" > "$OUT/$1.log" 2>&1; then
            tail -5 "$OUT/$1.log"; echo "FAILED: $*"; exit 1
        fi
        tail -2 "$OUT/$1.log"
    }
    run ./test_multicore
    run ./lan_twoplayer
    run ./lan_multiplayer 4 1 5 40000
    run ./lan_multiplayer 3 0 7 40000
    echo "== all tests passed ($MODE)"
    ;;
esac
