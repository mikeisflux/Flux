#!/usr/bin/env bash
# Bootstrap depot_tools and fetch the pinned Chromium source.
# This downloads ~100GB and takes hours. Run once.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

preflight "$(dirname "$CHROMIUM_DIR")"

if [ ! -d "$DEPOT_TOOLS_DIR" ]; then
  log "Cloning depot_tools -> $DEPOT_TOOLS_DIR"
  git clone --depth 1 \
    https://chromium.googlesource.com/chromium/tools/depot_tools.git \
    "$DEPOT_TOOLS_DIR"
fi

mkdir -p "$CHROMIUM_DIR"
cd "$CHROMIUM_DIR"

if [ ! -f .gclient ]; then
  log "Writing .gclient (pin: $CHROMIUM_VERSION)"
  cat > .gclient <<GCLIENT
solutions = [
  {
    "name": "src",
    "url": "$CHROMIUM_UPSTREAM",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      # Flux does not ship the closed-source PGO profiles or the internal
      # test corpora; skipping them saves ~15GB and hours of sync.
      "checkout_pgo_profiles": True,
      "checkout_nacl": False,
      "checkout_android": False,
      "checkout_ios": False,
    },
  },
]
GCLIENT
fi

if [ ! -d src ]; then
  log "Fetching Chromium (this takes 1-3 hours)"
  # --no-history keeps the checkout to ~50GB instead of ~90GB. A fork that
  # needs to bisect upstream should drop this flag.
  git clone --no-checkout "$CHROMIUM_MIRROR" src
fi

cd src
log "Checking out $CHROMIUM_VERSION"
git fetch --depth 1 origin "refs/tags/$CHROMIUM_VERSION:refs/tags/$CHROMIUM_VERSION" || \
  git fetch origin "refs/tags/$CHROMIUM_VERSION:refs/tags/$CHROMIUM_VERSION"
git checkout "tags/$CHROMIUM_VERSION"

log "Running gclient sync (pulls third_party DEPS; 1-2 hours)"
gclient sync --with_branch_heads --with_tags -D --nohooks

log "Installing Linux build dependencies (requires sudo)"
if [ -f build/install-build-deps.sh ]; then
  ./build/install-build-deps.sh --no-prompt || \
    warn "install-build-deps failed; install them manually before building."
fi

log "Running hooks"
gclient runhooks

log "Fetch complete. Next: build/sync.sh"
