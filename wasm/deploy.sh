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
REV_FULL=${SOURCE_REV_FULL:-$(git -C "$HERE" rev-parse HEAD 2>/dev/null || echo "")}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

[ -f "$DIST/snapszer.wasm" ] || { echo "no build in $DIST, run wasm/build.sh first" >&2; exit 1; }

cp -a "$DIST/." "$WORK/"

# GPLv3 section 6(d) wants directions to the source that match what was built,
# so the page names the exact revision rather than a moving branch.
if [ -n "$REV_FULL" ]; then
    TREE="https://github.com/smatkovi/harbour-snapszer/tree/$REV_FULL"
else
    TREE="https://github.com/smatkovi/harbour-snapszer"
fi
sed -i -e "s#__SOURCE_TREE__#$TREE#" -e "s#__SOURCE_REV__#$REV#" "$WORK/index.html"

cd "$WORK"
git init -q
git checkout -q -b gh-pages

# The branch is replaced wholesale on every deploy. Refuse if its tip is not one
# of our own deploys, and carry over a custom domain if one was configured.
if git fetch -q --depth 1 "$REMOTE" gh-pages 2>/dev/null; then
    prev=$(git log -1 --format=%s FETCH_HEAD)
    case "$prev" in
        "Snapszer for the browser, built from "*) ;;
        *) echo "gh-pages holds something else: $prev" >&2; exit 1 ;;
    esac
    if git show FETCH_HEAD:CNAME > CNAME 2>/dev/null; then
        echo "== carried over CNAME: $(cat CNAME)"
    else
        rm -f CNAME
    fi
fi
git add -A
git -c user.name="${GIT_AUTHOR_NAME:-smatkovi}" -c user.email="${GIT_AUTHOR_EMAIL:-smatkovi@users.noreply.github.com}" \
    commit -q -m "Snapszer for the browser, built from $REV"
git push -f -q "$REMOTE" gh-pages
cd /
echo "== pushed gh-pages to $REMOTE"
echo "== https://smatkovi.github.io/harbour-snapszer/"
