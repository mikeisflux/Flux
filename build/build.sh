#!/usr/bin/env bash
# Configure and build Flux.
#   build/build.sh [release|debug] [ninja target...]
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_checkout

CONFIG="${1:-release}"; shift || true
OUT="out/$( [ "$CONFIG" = debug ] && echo Debug || echo Release )"
ARGS_FILE="$FLUX_ROOT/build/args/${CONFIG}.gn"
[ -f "$ARGS_FILE" ] || die "No such config: $CONFIG"

cd "$SRC"
mkdir -p "$OUT"
cp "$ARGS_FILE" "$OUT/args.gn"

log "gn gen $OUT"
gn gen "$OUT"

TARGETS=("$@")
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(chrome)

log "Building: ${TARGETS[*]} (first build: 3-8h on 32 cores)"
autoninja -C "$OUT" "${TARGETS[@]}"

log "Built: $SRC/$OUT/chrome"
