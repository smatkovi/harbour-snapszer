#!/bin/sh
# Builds and signs the Android APK on the build machine, the way tools/build.sh
# makes the Sailfish RPMs there.
#
#   tools/build-apk.sh        # -> ~/ps/rpms/snapszer/harbour-snapszer-<version>-arm64-v8a.apk
#
# On the build machine: ~/snapszer-android holds Qt for Android, the SDK and
# the NDK (its setup.sh made them) and gets the source tree synced to src/;
# the release keystore is ~/.android/snapszer-release.jks, the same key the
# Tarock APK carries. Its password is not in the repository: it is taken from
# the line apksigner was first run with, kept in ~/tarock-build/build.log there.
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
if [ -n "$BUILD_HOST" ]; then
    HOST=$BUILD_HOST
elif ssh -o BatchMode=yes -o ConnectTimeout=4 sebastian@192.168.1.21 true 2>/dev/null; then
    HOST=sebastian@192.168.1.21
else
    HOST=arch
fi
VERSION=$(sed -n 's/^Version: *//p' "$ROOT/harbour-snapszer.spec")
APK=harbour-snapszer-$VERSION-arm64-v8a.apk
echo "== Snapszer $VERSION, APK on $HOST"
rsync -a --delete --exclude .git --exclude build "$ROOT/" "$HOST:snapszer-android/src/"
ssh "$HOST" "VERSION=$VERSION APK=$APK sh -s" <<'REMOTE'
set -e
W=$HOME/snapszer-android
BT=$W/sdk/build-tools/36.1.0
export ANDROID_SDK_ROOT=$W/sdk ANDROID_NDK_ROOT=$W/sdk/ndk/27.2.12479018
export GRADLE_USER_HOME=$W/gradle JAVA_HOME=/usr/lib/jvm/java-21-openjdk
if [ ! -f $W/build/build.ninja ]; then
    cmake -S $W/src/android -B $W/build -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=$W/qt/lib/cmake/Qt6/qt.toolchain.cmake -DQT_HOST_PATH=/usr \
        -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT -DANDROID_NDK_ROOT=$ANDROID_NDK_ROOT \
        -DQT_ANDROID_ABIS=arm64-v8a -DCMAKE_BUILD_TYPE=Release > $W/configure-$VERSION.log 2>&1
fi
nice -n 10 cmake --build $W/build --target apk > $W/build-$VERSION.log 2>&1 || { tail -30 $W/build-$VERSION.log; exit 1; }
UNSIGNED=$(find $W/build -name "*release-unsigned.apk" | head -1)
[ -n "$UNSIGNED" ] || { echo "no unsigned APK built"; exit 1; }
KSPASS=$(grep -o "ks-pass pass:[^ ]*" $HOME/tarock-build/build.log | head -1 | cut -d: -f2)
[ -n "$KSPASS" ] || { echo "keystore password not on record"; exit 1; }
mkdir -p $HOME/snapszer-build/out
$BT/zipalign -f -p 4 "$UNSIGNED" $HOME/snapszer-build/out/aligned.apk
$BT/apksigner sign --ks $HOME/.android/snapszer-release.jks --ks-pass "pass:$KSPASS" \
    --out $HOME/snapszer-build/out/$APK $HOME/snapszer-build/out/aligned.apk
rm -f $HOME/snapszer-build/out/aligned.apk
$BT/apksigner verify --print-certs $HOME/snapszer-build/out/$APK | grep "SHA-256"
$BT/aapt2 dump badging $HOME/snapszer-build/out/$APK 2>/dev/null | head -1
REMOTE
mkdir -p "$HOME/ps/rpms/snapszer"
rsync -a "$HOST:snapszer-build/out/$APK" "$HOME/ps/rpms/snapszer/"
ls -la "$HOME/ps/rpms/snapszer/$APK"
