#!/bin/sh
# Builds the browser edition and assembles the directory that gets published.
# Run on the build machine after wasm/setup.sh, from the top of the tree:
#
#   sh wasm/build.sh          # build and assemble wasm/dist
#   sh wasm/build.sh clean    # throw the build directory away first
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
W=${SNAPSZER_WASM_ROOT:-/tmp/snapszer-wasm}
BUILD=$W/build
DIST=$HERE/wasm/dist

[ "$1" = clean ] && rm -rf "$BUILD"

# The host Qt supplies moc, rcc, qmlimportscanner and lrelease; it must be the
# same version as the WebAssembly kit.
HOST_QT=${QT_HOST_PATH:-/usr}

. "$W/emsdk/emsdk_env.sh" >/dev/null 2>&1

"$W/qt/bin/qt-cmake" -S "$HERE/android" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DQT_HOST_PATH="$HOST_QT" \
    -DQT_HOST_PATH_CMAKE_DIR="$HOST_QT/lib/cmake"
nice -n 10 cmake --build "$BUILD" -j"${JOBS:-8}"

rm -rf "$DIST"; mkdir -p "$DIST"
cp "$BUILD/snapszer.js" "$BUILD/snapszer.wasm" "$BUILD/qtloader.js" "$DIST/"
cp "$HERE/wasm/index.html" "$HERE/wasm/manifest.webmanifest" \
   "$HERE/wasm/icon-180.png" "$HERE/wasm/icon-192.png" "$HERE/wasm/icon-512.png" "$DIST/"
# Qt for WebAssembly is used under the GPL, so the licence travels with the page.
cp "$HERE/wasm/LICENSE-GPL-3.0.txt" "$DIST/"
# Keep Qt's own loader page next to ours as the reference for the qtLoad call.
cp "$BUILD/snapszer.html" "$DIST/qt-reference.html"
# GitHub Pages must serve the files as they are, not run them through Jekyll.
: > "$DIST/.nojekyll"

echo "== $DIST"
ls -la "$DIST"
echo "wasm gzipped: $(gzip -9 -c "$DIST/snapszer.wasm" | wc -c) bytes"
