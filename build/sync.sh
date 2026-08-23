#!/usr/bin/env bash
# Apply the Flux patch series and link our modules into the Chromium tree.
# Idempotent: safe to re-run after editing patches or src/.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_checkout

cd "$SRC"

log "Resetting tree to pristine $CHROMIUM_VERSION"
git checkout -- . 2>/dev/null || true
git clean -fd chrome/browser/flux 2>/dev/null || true

# Our source is symlinked, not copied, so editing flux/src/ is picked up by
# ninja without re-running sync.
log "Linking Flux modules into //chrome/browser/flux"
rm -rf "$SRC/chrome/browser/flux"
ln -sfn "$FLUX_ROOT/src/browser" "$SRC/chrome/browser/flux"

rm -rf "$SRC/chrome/browser/resources/flux"
ln -sfn "$FLUX_ROOT/src/resources" "$SRC/chrome/browser/resources/flux"

log "Applying patch series"
while read -r patch; do
  case "$patch" in ''|\#*) continue ;; esac
  if git apply --check "$FLUX_ROOT/patches/$patch" 2>/dev/null; then
    git apply "$FLUX_ROOT/patches/$patch"
    printf '    applied  %s\n' "$patch"
  elif git apply --reverse --check "$FLUX_ROOT/patches/$patch" 2>/dev/null; then
    printf '    already  %s\n' "$patch"
  else
    die "Patch failed to apply: $patch
This usually means Chromium moved out from under the patch. Rebase it against
$CHROMIUM_VERSION, or re-pin with build/repin.sh."
  fi
done < "$FLUX_ROOT/patches/series"

log "Applying Flux branding"
# Copied over Chromium's own files rather than patched in: these are binaries,
# and a binary in the patch series is a merge conflict waiting to happen on
# every uprev. The reset above keeps this idempotent.
if [ -d "$FLUX_ROOT/branding/icons" ]; then
  (cd "$FLUX_ROOT/branding/icons" && find . -type f -print0) |
    while IFS= read -r -d '' rel; do
      dest="$SRC/chrome/app/theme/chromium/${rel#./}"
      mkdir -p "$(dirname "$dest")"
      cp -f "$FLUX_ROOT/branding/icons/${rel#./}" "$dest"
    done
  printf '    branded  chrome/app/theme/chromium\n'
fi

# Chromium keeps its own name as a literal in the unbranded string files - 742
# of them - so the browser introduces itself as Chromium in every notice and
# bubble that patch 0009's IDS_PRODUCT_NAME does not reach. A script rather
# than a patch: 742 hunks against a file Chromium edits constantly would be
# the most expensive thing in the series to rebase, and this derives its
# answer from whatever the tree currently says.
if [ -x "$FLUX_ROOT/tools/rebrand-strings.py" ]; then
  python3 "$FLUX_ROOT/tools/rebrand-strings.py" "$SRC"
fi


log "Sync complete. Next: build/build.sh"
