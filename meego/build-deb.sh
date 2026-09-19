#!/bin/sh
# Packages the ARM build as a Harmattan .deb. Runs on the build machine
# after "meego/build.sh arm":
#
#   meego/build-deb.sh                # -> build/meego/harbour-snapszer_<version>_armel.deb
#   VERSION=1.1.1 meego/build-deb.sh
#
# Installed layout on the N9:
#   /opt/harbour-snapszer/bin/harbour-snapszer      the game
#   /opt/harbour-snapszer/{qml,images,icons,translations}
#   /usr/share/applications/harbour-snapszer.desktop
#   /usr/share/themes/base/meegotouch/icons/harbour-snapszer-80.png
# The .deb is written by mkdeb.py (no dpkg-deb needed), as in nfsshift-sfos.
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd)
PKG=$HERE/meego
OUT=$HERE/build/meego
BIN=$OUT/arm/harbour-snapszer
XGCC=${XGCC:-/tmp/xgcc-harmattan}
VERSION=${VERSION:-$(sed -n 's/^Version: *//p' "$HERE/harbour-snapszer.spec" | head -1)}

[ -x "$BIN" ] || { echo "ARM binary missing: $BIN (run meego/build.sh arm)" >&2; exit 1; }
[ -f "$OUT/arm/translations/harbour-snapszer-hu.qm" ] || { echo "translations missing in $OUT/arm" >&2; exit 1; }

STAGE=$OUT/stage
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" "$STAGE/opt/harbour-snapszer/bin" "$STAGE/opt/harbour-snapszer/icons" \
         "$STAGE/usr/share/applications" "$STAGE/usr/share/themes/base/meegotouch/icons" \
         "$STAGE/usr/share/doc/harbour-snapszer"

# --- program and data -------------------------------------------------------
cp "$BIN" "$STAGE/opt/harbour-snapszer/bin/harbour-snapszer"
"$XGCC/bin/arm-none-linux-gnueabi-strip" "$STAGE/opt/harbour-snapszer/bin/harbour-snapszer"
chmod 755 "$STAGE/opt/harbour-snapszer/bin/harbour-snapszer"
cp -a "$PKG/qml" "$STAGE/opt/harbour-snapszer/qml"
cp -a "$HERE/images" "$STAGE/opt/harbour-snapszer/images"
cp -a "$OUT/arm/translations" "$STAGE/opt/harbour-snapszer/translations"
cp "$HERE/sailfish/icons/icon-256.png" "$STAGE/opt/harbour-snapszer/icons/icon-256.png"

# --- icons: 80x80 for the launcher, 64x64 base64 for the package manager ---
magick "$HERE/sailfish/icons/icon-256.png" -resize 80x80 "$STAGE/usr/share/themes/base/meegotouch/icons/harbour-snapszer-80.png"
magick "$HERE/sailfish/icons/icon-256.png" -resize 64x64 "$OUT/icon-64.png"

cp "$PKG/harbour-snapszer.desktop" "$STAGE/usr/share/applications/harbour-snapszer.desktop"
gzip -9nc "$PKG/changelog" > "$STAGE/usr/share/doc/harbour-snapszer/changelog.gz"
find "$STAGE" -type f ! -path "*/bin/*" -exec chmod 644 {} +
find "$STAGE" -type d -exec chmod 755 {} +

# --- control from control.in --------------------------------------------------
# XB-Maemo-Icon-26 is the 64x64 PNG as base64, continuation lines indented by
# one space; without it the application manager shows no icon.
VERSION="$VERSION" ICON="$OUT/icon-64.png" python3 - "$PKG/control.in" "$STAGE/DEBIAN/control" <<'PY'
import base64, os, sys, textwrap
src, dst = sys.argv[1], sys.argv[2]
with open(os.environ["ICON"], "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
icon = "\n".join(" " + line for line in textwrap.wrap(b64, 76))
with open(src, "r", encoding="utf-8") as f:
    ctl = f.read()
ctl = ctl.replace("@VERSION@", os.environ["VERSION"]).replace("@ICON@", icon)
with open(dst, "w", encoding="utf-8") as f:
    f.write(ctl)
PY

DEB="$OUT/harbour-snapszer_${VERSION}_armel.deb"
python3 "$PKG/mkdeb.py" "$STAGE" "$DEB"
python3 "$PKG/mkdeb.py" --info "$DEB" | head -40
echo "== $DEB"
