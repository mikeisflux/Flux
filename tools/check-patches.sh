#!/usr/bin/env bash
# Apply the whole patch series to pristine Chromium and report anything that
# fails - without a checkout.
#
# It fetches only the files the patches actually touch (currently 18) from the
# pinned tag, which is a few seconds, and applies the series in order. A patch
# that has rotted is otherwise discovered by the user, on their machine, at the
# start of a build they were about to spend two hours on.
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1
ROOT="$PWD"
VERSION=$(sed -n 's/^CHROMIUM_VERSION=//p' chromium.version | tr -d ' \t\r')
[ -n "$VERSION" ] || { echo "No CHROMIUM_VERSION in chromium.version" >&2; exit 1; }
BASE="https://raw.githubusercontent.com/chromium/chromium/$VERSION"

CACHE="${TMPDIR:-/tmp}/flux-patch-check/$VERSION"
WORK=$(mktemp -d) || exit 1
trap 'rm -rf "$WORK"' EXIT

status=0
paths=$(grep -h '^--- a/' "$ROOT"/patches/*.patch | sed 's|^--- a/||' | sort -u)

for path in $paths; do
  if [ ! -f "$CACHE/$path" ]; then
    mkdir -p "$CACHE/$(dirname "$path")"
    code=$(curl -sS -o "$CACHE/$path" -w '%{http_code}' "$BASE/$path")
    if [ "$code" != "200" ]; then
      rm -f "$CACHE/$path"
      echo "Could not fetch $path from $VERSION (HTTP $code)." >&2
      echo "If this is offline, the patch series is UNVERIFIED - say so rather" >&2
      echo "than claiming it applies." >&2
      exit 0
    fi
  fi
  mkdir -p "$WORK/$(dirname "$path")"
  cp "$CACHE/$path" "$WORK/$path"
done

cd "$WORK" || exit 1
git init -q . && git add -A &&
  git -c user.email=check@flux -c user.name=check commit -qm pristine || exit 1

while read -r patch; do
  case "$patch" in ''|\#*) continue ;; esac
  if git apply "$ROOT/patches/$patch" 2>/dev/null; then
    printf '    applies  %s\n' "$patch"
  else
    printf '    FAILED   %s\n' "$patch"
    git apply --verbose "$ROOT/patches/$patch" 2>&1 | sed 's/^/      /' | tail -20
    status=1
  fi
done < "$ROOT/patches/series"

if [ $status -eq 0 ]; then
  echo "Patch series applies cleanly to $VERSION"
else
  echo "Patch series does NOT apply to $VERSION - rebase before handing it over." >&2
fi
exit $status
