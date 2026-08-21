#!/usr/bin/env bash
# Shared environment for all Flux build scripts.
set -euo pipefail

FLUX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$FLUX_ROOT/chromium.version"

: "${CHROMIUM_DIR:=$FLUX_ROOT/../chromium}"          # where the checkout lives
: "${DEPOT_TOOLS_DIR:=$FLUX_ROOT/../depot_tools}"
: "${FLUX_OUT:=out/Release}"
SRC="$CHROMIUM_DIR/src"

export PATH="$DEPOT_TOOLS_DIR:$PATH"
export DEPOT_TOOLS_UPDATE=${DEPOT_TOOLS_UPDATE:-1}

log()  { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m/!\\\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31mERR\033[0m %s\n' "$*" >&2; exit 1; }

require_checkout() {
  [ -d "$SRC" ] || die "No Chromium checkout at $SRC. Run build/fetch.sh first."
}

# Fail early rather than 6 hours into a build.
# Thresholds differ sharply by config: an official build with LTO+PGO needs
# far more of everything than a dev build.
preflight() {
  local free_gb cores ram_gb config="${2:-dev}"
  free_gb=$(df -BG --output=avail "$1" 2>/dev/null | tail -1 | tr -dc '0-9')
  cores=$(nproc)
  ram_gb=$(free -g 2>/dev/null | awk '/^Mem:/{print $2}')

  log "Preflight: ${free_gb}GB free, ${cores} cores, ${ram_gb}GB RAM (config: $config)"

  # Checkout is ~100GB regardless; build output is what varies.
  local need_disk=150 need_ram=16
  if [ "$config" = "release" ]; then need_disk=250; need_ram=32; fi

  [ "${free_gb:-0}" -ge "$need_disk" ] || \
    warn "Need ~${need_disk}GB for a '$config' build (checkout ~100GB + output). Have ${free_gb}GB."
  [ "${ram_gb:-0}" -ge "$need_ram" ] || \
    warn "Only ${ram_gb}GB RAM. A '$config' build wants ${need_ram}GB+. Cap link jobs: autoninja -j2 (see docs/10-building.md)."
  [ "${cores:-0}" -ge 8 ] || \
    warn "Only ${cores} cores. Expect a long first build; see docs/10-building.md for realistic timings."

  if [ "$config" = "release" ] && [ "${cores:-0}" -lt 16 ]; then
    warn "Official builds on <16 cores take 15-25h. Use 'dev' while iterating."
  fi
}
