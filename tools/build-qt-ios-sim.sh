#!/bin/bash
# Build a static Qt 6 for the iOS *simulator* (arm64).
#
# The Qt binary packages ship plugin initialiser objects built for the device
# only, so linking a simulator app against them fails on the platform plugin.
# Building Qt ourselves for iphonesimulator is the only way around that.
#
# Taken from the NSR Reader port, where this was worked out; qtbase,
# qtshadertools and qtdeclarative are all Snapszer needs (Quick Controls live
# in qtdeclarative since 6.2).
#
# Usage: build-qt-ios-sim.sh <install-prefix> <host-qt-dir>
set -e
PFX="$1"; HOSTQT="$2"
[ -n "$PFX" ] && [ -n "$HOSTQT" ] || { echo "usage: $0 <prefix> <host-qt>"; exit 1; }
VER=6.9.1
SRC="$PWD/ext/qtsrc"
BLD="$PWD/ext/qtbuild"
mkdir -p "$SRC" "$BLD" "$PFX"
BASE=https://download.qt.io/archive/qt/6.9/$VER/submodules

COMMON=(-GNinja
  -DCMAKE_SYSTEM_NAME=iOS
  -DCMAKE_OSX_SYSROOT=iphonesimulator
  -DCMAKE_OSX_ARCHITECTURES=arm64
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
  -DCMAKE_INSTALL_PREFIX=$PFX
  -DCMAKE_PREFIX_PATH=$PFX
  -DCMAKE_FIND_ROOT_PATH="$PFX;$HOSTQT"
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH
  -DCMAKE_BUILD_TYPE=Release
  -DQT_HOST_PATH=$HOSTQT
  -DQT_HOST_PATH_CMAKE_DIR="$HOSTQT/lib/cmake"
  -DQT_BUILD_EXAMPLES=OFF
  -DQT_BUILD_TESTS=OFF
  -DQT_BUILD_BENCHMARKS=OFF)

build_module() {
  local mod=$1; shift
  local dir="$SRC/$mod-everywhere-src-$VER"
  local tar="$SRC/$mod-$VER.tar.xz"
  echo "== $mod"
  [ -f "$tar" ] || curl -sSL --retry 3 -o "$tar" "$BASE/$mod-everywhere-src-$VER.tar.xz"
  [ -d "$dir" ] || tar xf "$tar" -C "$SRC"
  cmake -S "$dir" -B "$BLD/$mod" "${COMMON[@]}" "$@"
  cmake --build "$BLD/$mod" -j3
  cmake --install "$BLD/$mod"
  echo "   $mod installed"
}

# qtbase: trim what a card game never needs, but keep the network -- the LAN
# game is the whole reason this has to run on a real device eventually.
build_module qtbase \
  -DFEATURE_sql=OFF -DFEATURE_testlib=OFF -DFEATURE_printsupport=OFF \
  -DFEATURE_dbus=OFF -DFEATURE_concurrent=OFF
# qtshadertools must exist for the HOST too: cross-building it (and later
# qtdeclarative) needs the host qsb shader compiler, which the binary Qt package
# does not ship.
echo "== qtshadertools (host tools)"
HOSTSRC="$SRC/qtshadertools-everywhere-src-$VER"
HOSTTAR="$SRC/qtshadertools-$VER.tar.xz"
[ -f "$HOSTTAR" ] || curl -sSL --retry 3 -o "$HOSTTAR" "$BASE/qtshadertools-everywhere-src-$VER.tar.xz"
[ -d "$HOSTSRC" ] || tar xf "$HOSTTAR" -C "$SRC"
# always configure this one from scratch: a cached build tree leaves ninja with
# "no work to do" and then the qsb tool - and its Qt6ShaderToolsTools package -
# never gets built, which is exactly what the cross build needs.
rm -rf "$BLD/qtshadertools-host"
cmake -S "$HOSTSRC" -B "$BLD/qtshadertools-host" -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$HOSTQT" \
      -DCMAKE_INSTALL_PREFIX="$HOSTQT" \
      -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_TOOLS_BY_DEFAULT=ON
cmake --build "$BLD/qtshadertools-host" -j3
cmake --install "$BLD/qtshadertools-host"
echo "   host qsb: $(ls "$HOSTQT/bin/qsb" 2>/dev/null || echo MISSING)"
echo "   host cmake packages:"; ls "$HOSTQT/lib/cmake" | grep -i shadertools || echo "   (keine ShaderTools-Pakete!)"

build_module qtshadertools          # required to build qtdeclarative
build_module qtdeclarative          # Quick, QuickControls2, QuickDialogs2

echo "== done"
ls "$PFX/lib" | head
echo "platform plugin:"; ls "$PFX/plugins/platforms" 2>/dev/null
