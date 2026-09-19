#!/bin/sh
# Sets up the Qt-for-WebAssembly toolchain on the build machine. Everything
# lives under /tmp/snapszer-wasm so it can be deleted in one go; a reboot of
# the build machine wipes it and this script simply runs again (two minutes).
#
# Two things here are not obvious:
#  * Qt publishes the single-threaded WebAssembly kit only as a Windows-host
#    archive. The WebAssembly libraries inside are host independent and the
#    wrapper scripts are plain /bin/sh, so it works on Linux; the multithreaded
#    kit, which does ship a Linux archive, is useless to us because threads
#    need COOP/COEP response headers and GitHub Pages cannot set them.
#  * The Emscripten version must be exactly the one Qt was built with. A
#    distribution package is almost always too new and Qt refuses it.
set -e
W=${SNAPSZER_WASM_ROOT:-/tmp/snapszer-wasm}
QT_VER=6.11.1
QT_PKG=6.11.1-0-202605090529
EMSDK_VER=4.0.7
BASE=https://download.qt.io/online/qtsdkrepository/all_os/wasm/qt6_6111/qt6_6111_wasm_singlethread/qt.qt6.6111.wasm_singlethread

mkdir -p "$W/dl" "$W/qt"

if [ ! -f "$W/qt/bin/qt-cmake" ]; then
    echo "== Qt $QT_VER for WebAssembly (single-threaded)"
    for a in qtbase qtdeclarative qtsvg qttranslations; do
        f="$a-Windows-Windows_11_24H2-Clang-Windows-WebAssembly-X86_64.7z"
        [ -f "$W/dl/$f" ] || curl -fsSL -o "$W/dl/$f" "$BASE/$QT_PKG$f"
        bsdtar -xf "$W/dl/$f" -C "$W/qt"
    done
    chmod +x "$W"/qt/bin/qt-cmake "$W"/qt/bin/qmake* "$W"/qt/bin/qt-* 2>/dev/null || true
fi

if [ ! -x "$W/emsdk/upstream/emscripten/emcc" ]; then
    echo "== Emscripten $EMSDK_VER (the version Qt $QT_VER was built with)"
    [ -d "$W/emsdk" ] || git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$W/emsdk"
    (cd "$W/emsdk" && ./emsdk install "$EMSDK_VER" && ./emsdk activate "$EMSDK_VER")
fi

echo "== done"; du -sh "$W/qt" "$W/emsdk"
