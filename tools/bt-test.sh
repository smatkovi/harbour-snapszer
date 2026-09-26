#!/bin/sh
# Play a two-player match over Bluetooth between this device and a second one,
# with the test binaries built by meego/build.sh (ARM) and by cmake with
# -DSNAPSZER_BUILD_TESTS=ON (Sailfish OS).
#
#   tools/bt-test.sh <remote ssh target> <remote address> [seconds]
#
# The remote side hosts, this device joins. Example:
#
#   BT_REMOTE_BIN=/home/user/bt_twoplayer \
#   tools/bt-test.sh user@192.168.1.8 B4:EE:D4:92:C7:91 35
set -e
REMOTE=${1:?usage: tools/bt-test.sh <ssh target> <bluetooth address> [seconds]}
ADDRESS=${2:?missing the remote Bluetooth address}
SECONDS_=${3:-35}
LOCAL_BIN=${BT_LOCAL_BIN:-./bt_twoplayer}
REMOTE_BIN=${BT_REMOTE_BIN:-./bt_twoplayer}
SSH=${BT_SSH:-ssh}

$SSH "$REMOTE" "killall $(basename "$REMOTE_BIN") 2>/dev/null; sleep 1; \
    nohup $REMOTE_BIN host $((SECONDS_ + 30)) > /tmp/bt-host.log 2>&1 &"
sleep 4
"$LOCAL_BIN" guest "$ADDRESS" "$SECONDS_" || true
echo "--- the other side:"
sleep 5
$SSH "$REMOTE" "cat /tmp/bt-host.log"
