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
preflight() {
  local free_gb cores ram_gb
  free_gb=$(df -BG --output=avail "$1" 2>/dev/null | tail -1 | tr -dc '0-9')
  cores=$(nproc)
  ram_gb=$(free -g 2>/dev/null | awk '/^Mem:/{print $2}')

  log "Preflight: ${free_gb}GB free, ${cores} cores, ${ram_gb}GB RAM"
  [ "${free_gb:-0}" -ge 200 ] || warn "Chromium needs ~200GB free (checkout ~100GB + build ~80GB). Have ${free_gb}GB."
  [ "${cores:-0}" -ge 16 ]    || warn "Only ${cores} cores. A full build takes 20h+ below 16 cores; 32+ recommended."
  [ "${ram_gb:-0}" -ge 32 ]   || warn "Only ${ram_gb}GB RAM. Linking needs ~1.5GB/core; expect OOM under 32GB."
}
