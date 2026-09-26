#!/bin/sh
# Builds the iOS edition.
#
#   sh ios/build.sh [iphonesimulator|iphoneos]
#
# Same source as the Android and browser editions: the project in android/ is
# the portable Qt 6 one, and iOS is the third shell around it (see the if(IOS)
# block in android/CMakeLists.txt).
#
# Needs a Qt for iOS in QT_IOS and the matching host Qt in QT_HOST_PATH. For
# the simulator that Qt has to be built from source -- the binary packages ship
# device-only plugin initialisers -- which is what tools/build-qt-ios-sim.sh is
# for.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
SDK=${1:-iphonesimulator}
BUILD=${BUILD:-$HERE/build-ios-$SDK}
: "${QT_IOS:?QT_IOS muss auf ein Qt fuer iOS zeigen}"
: "${QT_HOST_PATH:?QT_HOST_PATH muss auf das passende Host-Qt zeigen}"

"$QT_IOS/bin/qt-cmake" -S "$HERE/android" -B "$BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DQT_HOST_PATH_CMAKE_DIR="$QT_HOST_PATH/lib/cmake" \
    -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${MIN_IOS:-16.0}"

cmake --build "$BUILD" -j"${JOBS:-3}"

APP=$(find "$BUILD" -name "snapszer.app" -maxdepth 4 | head -1)
[ -n "$APP" ] || { echo "kein Bündel gefunden"; exit 1; }
# The simulator refuses an unsigned bundle but does not care who signed it.
codesign --force --sign - --timestamp=none "$APP"
echo "$APP"
du -sh "$APP"
