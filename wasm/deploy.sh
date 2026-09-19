#!/bin/sh
# Publishes wasm/dist to the gh-pages branch of the repository. Each deploy is
# a single fresh commit, force-pushed, so the generated ~29 MB of WebAssembly
# never accumulates in the history. Run it where git can push, with wasm/dist
# already built (wasm/build.sh or wasm/remote-build.sh).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
DIST=$HERE/wasm/dist
REMOTE=${REMOTE:-$(git -C "$HERE" remote get-url origin 2>/dev/null || echo https://github.com/smatkovi/harbour-snapszer.git)}
# The build machine has no checkout, so the source commit can be passed in.
REV=${SOURCE_REV:-$(git -C "$HERE" rev-parse --short HEAD 2>/dev/null || echo unknown)}
WORK=$(mktemp -d)

[ -f "$DIST/snapszer.wasm" ] || { echo "no build in $DIST, run wasm/build.sh first" >&2; exit 1; }

cp -a "$DIST/." "$WORK/"
cd "$WORK"
git init -q
git checkout -q -b gh-pages
git add -A
git -c user.name="${GIT_AUTHOR_NAME:-smatkovi}" -c user.email="${GIT_AUTHOR_EMAIL:-smatkovi@users.noreply.github.com}" \
    commit -q -m "Snapszer for the browser, built from $REV"
git push -f -q "$REMOTE" gh-pages
cd /; rm -rf "$WORK"
echo "== pushed gh-pages to $REMOTE"
echo "== https://smatkovi.github.io/harbour-snapszer/"
