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

log "Sync complete. Next: build/build.sh"
