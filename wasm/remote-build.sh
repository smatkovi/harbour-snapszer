#!/bin/sh
# Builds the browser edition from the phone: syncs this tree to the build
# machine, sets the toolchain up there if a reboot wiped /tmp, builds, and
# fetches the publishable directory back into wasm/dist.
#
#   wasm/remote-build.sh          # build and fetch
#   wasm/remote-build.sh clean    # discard the build directory first
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
HOST=$(sh "$HERE/../nfsshift-sfos/tools/buildhost.sh")
REMOTE=/tmp/snapszer-wasm/src

echo "== build host: $HOST"
ssh "$HOST" "mkdir -p $REMOTE"
rsync -a --partial --delete --exclude .git --exclude build --exclude wasm/dist "$HERE/" "$HOST:$REMOTE/"
ssh "$HOST" "sh $REMOTE/wasm/setup.sh"
ssh "$HOST" "cd $REMOTE && sh wasm/build.sh ${1:-}"
rsync -a --partial --delete "$HOST:$REMOTE/wasm/dist/" "$HERE/wasm/dist/"
echo "== fetched into $HERE/wasm/dist"
du -sh "$HERE/wasm/dist"
