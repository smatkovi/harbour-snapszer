#!/bin/sh
# Runs the x86 build on the build machine's X display in an N9-sized window
# and, if asked, saves a screenshot after a few seconds:
#   meego/run-x86.sh                 # run in the foreground
#   meego/run-x86.sh DIR [sec] [page] # start, save full-size screenshots of
#                                     # the view to DIR for sec seconds, quit;
#                                     # page: Settings.qml, LanPage.qml, ...,
#                                     # multi3, multi4, menu (see openPage)
HERE=$(cd "$(dirname "$0")/.." && pwd)
SIMQT=${SIMQT:-$HOME/QtSDK/Simulator/Qt/gcc}
export DISPLAY=${DISPLAY:-:0}
export XAUTHORITY=${XAUTHORITY:-$HOME/.Xauthority}
export SNAPSZER_ROOT="$HERE/build/meego/x86/root"
export SNAPSZER_WINDOWED=1
export QML_IMPORT_PATH="$SIMQT/imports"
# The x86 binary renders inside the Qt Simulator, which must be running.
if ! pgrep -x simulator > /dev/null; then
    "$HOME/QtSDK/Simulator/Application/simulator" > /dev/null 2>&1 &
    sleep 5
fi
# Stage the file layout of the package next to the binary.
mkdir -p "$SNAPSZER_ROOT/bin" "$SNAPSZER_ROOT/icons"
rm -rf "$SNAPSZER_ROOT/qml" "$SNAPSZER_ROOT/images" "$SNAPSZER_ROOT/translations"
cp -a "$HERE/meego/qml" "$HERE/images" "$HERE/build/meego/x86/translations" "$SNAPSZER_ROOT/"
cp -a "$HERE/sailfish/icons/icon-256.png" "$SNAPSZER_ROOT/icons/"
cp "$HERE/build/meego/x86/harbour-snapszer" "$SNAPSZER_ROOT/bin/"
# Settings of the desktop runs stay in a scratch home; the Simulator Qt
# still needs the simulator registry from the real home.
REALHOME=$HOME
export HOME=${SNAPSZER_HOME:-$HERE/build/meego/x86/home}
mkdir -p "$HOME/.config/Nokia"
cp "$REALHOME/.config/Nokia/QtSimulator.conf" "$HOME/.config/Nokia/"
if [ -z "$1" ]; then
    exec "$SNAPSZER_ROOT/bin/harbour-snapszer"
fi
mkdir -p "$1"; rm -f "$1"/shot-*.png
SNAPSZER_SHOT_DIR="$1" SNAPSZER_OPEN="$3" "$SNAPSZER_ROOT/bin/harbour-snapszer" > "$HERE/build/meego/x86/run.log" 2>&1 &
PID=$!
sleep "${2:-6}"
kill $PID 2>/dev/null
ls "$1"
echo "== log:"; cat "$HERE/build/meego/x86/run.log"
