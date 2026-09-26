#!/bin/sh
# Build the Sailfish OS RPMs in the SDK container on the Arch machine.
#
#   tools/build.sh [aarch64|armv7hl] ...     (default: aarch64)
#
# The packages land in ~/ps/rpms/snapszer/ on this device. BUILD_HOST,
# SDK_CONTAINER and SDK_TARGET override the defaults. The MeeGo edition has
# its own script, meego/remote-build.sh.
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ARCHES=${*:-"aarch64"}

if [ -n "$BUILD_HOST" ]; then
    HOST=$BUILD_HOST
elif ssh -o BatchMode=yes -o ConnectTimeout=4 sebastian@192.168.1.21 true 2>/dev/null; then
    HOST=sebastian@192.168.1.21
else
    HOST=arch
fi
CONTAINER=${SDK_CONTAINER:-sfossdk52}
TARGET=${SDK_TARGET:-SailfishOS-5.2.0.15}
VERSION=$(sed -n 's/^Version: *//p' "$ROOT/harbour-snapszer.spec")
RELEASE=$(sed -n 's/^Release: *//p' "$ROOT/harbour-snapszer.spec")

echo "harbour-snapszer $VERSION-$RELEASE for: $ARCHES"
ssh "$HOST" "mkdir -p ~/snapszer-build/src ~/snapszer-build/out"
rsync -a --delete --exclude .git --exclude build "$ROOT/" "$HOST:snapszer-build/src/"
# The build happens inside the container's own file system: the shared mount
# is too slow and mb2 writes into it constantly.
ssh "$HOST" "cd ~/snapszer-build/src && tar czf /tmp/snapszer-src.tgz . && \
    docker cp /tmp/snapszer-src.tgz $CONTAINER:/tmp/snapszer-src.tgz"

mkdir -p "$HOME/ps/rpms/snapszer"
for ARCH in $ARCHES; do
    RPM=harbour-snapszer-$VERSION-$RELEASE.$ARCH.rpm
    ssh "$HOST" "docker exec $CONTAINER bash -lc '\
        rm -rf ~/sbuild-snapszer-$ARCH && mkdir -p ~/sbuild-snapszer-$ARCH && \
        cd ~/sbuild-snapszer-$ARCH && tar xzf /tmp/snapszer-src.tgz && \
         mb2 -t $TARGET-$ARCH -s harbour-snapszer.spec build' | grep -E '^Wrote:|error:|Error|packages and'"
    ssh "$HOST" "docker cp $CONTAINER:/home/mersdk/sbuild-snapszer-$ARCH/RPMS/$RPM ~/snapszer-build/out/"
    rsync -a "$HOST:snapszer-build/out/$RPM" "$HOME/ps/rpms/snapszer/"
    echo "  -> ~/ps/rpms/snapszer/$RPM"
done
