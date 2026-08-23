#!/usr/bin/env bash
# Run tools/rebrand-strings.py against the real string files at the pinned tag
# and prove it does the right thing to them.
#
# The rebrand is not a patch, so nothing else notices when Chromium reshapes
# these files on an uprev. What could go wrong is specific:
#
#   - the strings move, and the rebrand silently renames nothing
#   - a support URL with a capital C in it appears, and gets renamed into a
#     broken link
#   - the copyright attribution gets renamed, which would be false
#
# Each of those is checked here against the actual files rather than assumed.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1

VERSION=$(sed -n 's/^CHROMIUM_VERSION=//p' chromium.version | tr -d ' \t\r')
[ -n "$VERSION" ] || { echo "No CHROMIUM_VERSION in chromium.version" >&2; exit 1; }

CACHE="${TMPDIR:-/tmp}/flux-rebrand-$VERSION"
BASE="https://raw.githubusercontent.com/chromium/chromium/$VERSION"
FILES="chrome/app/chromium_strings.grd chrome/app/settings_chromium_strings.grdp"

mkdir -p "$CACHE/tree/chrome/app" || exit 1
for f in $FILES; do
  dest="$CACHE/tree/$f"
  if [ ! -s "$dest" ]; then
    if ! curl -sfSL --retry 2 -o "$dest" "$BASE/$f"; then
      echo "Could not fetch $f - the rebrand is UNVERIFIED. Say so rather" >&2
      echo "than claiming the strings are renamed." >&2
      exit 0
    fi
  fi
  cp "$dest" "$CACHE/tree/$f.before"
done

python3 tools/rebrand-strings.py "$CACHE/tree" >/dev/null || {
  echo "FAIL: rebrand-strings.py errored on the pinned files." >&2
  exit 1
}

status=0

# 1. It actually did something, and specifically to the string that sent us
#    looking: the session restore notice the browser shows on startup.
if ! grep -q "Flux restores your tabs every time you restart" \
        "$CACHE/tree/chrome/app/chromium_strings.grd"; then
  echo "FAIL: the session-restore notice still names Chromium." >&2
  echo "      The strings likely moved on the last uprev." >&2
  status=1
fi

# 2. The copyright attribution is a legal statement about who wrote Chromium,
#    not a product name. Renaming it would be a false claim, and the about
#    page shows it.
if ! grep -q "The Chromium Authors" \
        "$CACHE/tree/chrome/app/chromium_strings.grd"; then
  echo "FAIL: the Chromium Authors copyright was renamed." >&2
  status=1
fi

# 3. No URL changed. A renamed host is a dead support link, and it would be
#    found by a user clicking it rather than by anything here.
for f in $FILES; do
  before=$(grep -o 'https\?://[^ "<>]*' "$CACHE/tree/$f.before" | sort | md5sum)
  after=$(grep -o 'https\?://[^ "<>]*' "$CACHE/tree/$f" | sort | md5sum)
  if [ "$before" != "$after" ]; then
    echo "FAIL: the rebrand altered a URL in $f." >&2
    diff <(grep -o 'https\?://[^ "<>]*' "$CACHE/tree/$f.before" | sort) \
         <(grep -o 'https\?://[^ "<>]*' "$CACHE/tree/$f" | sort) >&2
    status=1
  fi
done

# 4. The URL guard, against a case that does not exist in the tree yet.
#    No URL in these files contains "Chromium" today, so assertion 3 has
#    nothing to catch and would pass with the guard deleted - which is exactly
#    how a check in this repo has silently rotted before. This supplies the
#    case rather than waiting for an uprev to.
FIXTURE="$CACHE/fixture"
rm -rf "$FIXTURE" && mkdir -p "$FIXTURE/chrome/app" || exit 1
cat > "$FIXTURE/chrome/app/chromium_strings.grd" <<'FIX'
<message name="IDS_TEST">Chromium sent you to https://support.Chromium.example/Chromium/help for Chromium help.</message>
<message name="IDS_TEST2">Copyright The Chromium Authors. All rights reserved.</message>
FIX
cp "$FIXTURE/chrome/app/chromium_strings.grd" \
   "$FIXTURE/chrome/app/settings_chromium_strings.grdp"
python3 tools/rebrand-strings.py "$FIXTURE" >/dev/null 2>&1
FIXED=$(cat "$FIXTURE/chrome/app/chromium_strings.grd")
EXPECT='Flux sent you to https://support.Chromium.example/Chromium/help for Flux help.'
if ! printf '%s' "$FIXED" | grep -qF "$EXPECT"; then
  echo "FAIL: the URL guard does not hold. Expected the URL untouched and the" >&2
  echo "      prose renamed. Got:" >&2
  printf '        %s\n' "$FIXED" >&2
  status=1
fi
if ! printf '%s' "$FIXED" | grep -qF 'The Chromium Authors'; then
  echo "FAIL: the copyright guard does not hold on the fixture." >&2
  status=1
fi

# Leave the tree dirty-free for a re-run: restore the originals so the next
# invocation starts from pristine rather than from already-rebranded files.
for f in $FILES; do cp "$CACHE/tree/$f.before" "$CACHE/tree/$f"; done

[ $status -eq 0 ] && echo "Rebrand OK (renames, keeps the copyright, touches no URL)"
exit $status
