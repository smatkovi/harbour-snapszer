#!/bin/sh
# Builds the N9 package from the phone: syncs this tree to the build machine
# (tools/buildhost.sh of nfsshift-sfos picks LAN or tunnel), restores the
# cross toolchain there if a reboot wiped /tmp, builds, packages, and fetches
# the .deb into ~/ps/rpms/snapszer/.
#
#   meego/remote-build.sh            # build + package
#   meego/remote-build.sh tests      # only the Qt 4 and Qt 5 test suites
#   meego/remote-build.sh x86        # only the desktop (Qt Simulator) build
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
HOST=$(sh "$HERE/../nfsshift-sfos/tools/buildhost.sh")
REMOTE=/tmp/snapszer/src
TOOLCHAIN_TAR=${TOOLCHAIN_TAR:-$HOME/ps/toolchains/xgcc-harmattan-gcc14-hardfp.tar.gz}
MODE=${1:-package}

echo "== build host: $HOST"
ssh "$HOST" "mkdir -p $REMOTE"
rsync -a --partial --delete --exclude .git --exclude build "$HERE/" "$HOST:$REMOTE/"

case "$MODE" in
tests)
    ssh "$HOST" "cd $REMOTE && sh meego/build.sh tests-qt5 && sh meego/build.sh tests-qt4"
    exit 0 ;;
x86)
    ssh "$HOST" "cd $REMOTE && sh meego/build.sh x86"
    exit 0 ;;
esac

# The toolchain lives in /tmp on the build machine (its disk is full) and
# is kept as a tarball on the phone; rebuild it only when both are missing.
if ! ssh "$HOST" test -x /tmp/xgcc-harmattan/bin/arm-none-linux-gnueabi-g++; then
    if [ -f "$TOOLCHAIN_TAR" ]; then
        echo "== restoring the cross toolchain from $TOOLCHAIN_TAR"
        rsync -a --partial "$TOOLCHAIN_TAR" "$HOST:/tmp/xgcc-harmattan.tar.gz"
        ssh "$HOST" "tar xzf /tmp/xgcc-harmattan.tar.gz -C /tmp && rm /tmp/xgcc-harmattan.tar.gz"
    else
        echo "== building the cross toolchain (about half an hour)"
        ssh "$HOST" "sh $REMOTE/meego/toolchain.sh"
        mkdir -p "$(dirname "$TOOLCHAIN_TAR")"
        ssh "$HOST" "tar czf /tmp/xgcc-harmattan.tar.gz -C /tmp xgcc-harmattan"
        rsync -a --partial "$HOST:/tmp/xgcc-harmattan.tar.gz" "$TOOLCHAIN_TAR"
    fi
fi

ssh "$HOST" "cd $REMOTE && sh meego/build.sh arm && sh meego/build-deb.sh"
mkdir -p "$HOME/ps/rpms/snapszer"
rsync -a --partial "$HOST:$REMOTE/build/meego/harbour-snapszer_*_armel.deb" "$HOME/ps/rpms/snapszer/"
ls -la "$HOME/ps/rpms/snapszer/"
