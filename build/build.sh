#!/usr/bin/env bash
# Configure and build Flux.
#   build/build.sh [release|debug] [ninja target...]
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_checkout

CONFIG="${1:-dev}"; shift || true
case "$CONFIG" in
  dev)     OUT="out/Dev" ;;
  debug)   OUT="out/Debug" ;;
  release) OUT="out/Release" ;;
  *) die "Unknown config '$CONFIG' (want: dev | debug | release)" ;;
esac
ARGS_FILE="$FLUX_ROOT/build/args/${CONFIG}.gn"
[ -f "$ARGS_FILE" ] || die "No such config: $CONFIG"

preflight "$SRC" "$CONFIG"

cd "$SRC"
mkdir -p "$OUT"
cp "$ARGS_FILE" "$OUT/args.gn"

log "gn gen $OUT"
gn gen "$OUT"

TARGETS=("$@")
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(chrome)

# Under-provisioned machines OOM during linking, which wastes the whole build.
# ~1.5GB per link job is the rule of thumb.
JOBS_FLAG=()
RAM_GB=$(free -g 2>/dev/null | awk '/^Mem:/{print $2}')
if [ "${RAM_GB:-64}" -lt 16 ]; then
  warn "Low RAM (${RAM_GB}GB): capping to -j2 to avoid OOM during link."
  JOBS_FLAG=(-j2)
elif [ "${RAM_GB:-64}" -lt 32 ]; then
  JOBS_FLAG=(-j4)
fi

log "Building: ${TARGETS[*]} -> $OUT"
autoninja -C "$OUT" "${JOBS_FLAG[@]}" "${TARGETS[@]}"

log "Built: $SRC/$OUT/chrome"
