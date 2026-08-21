<#
  Bootstrap depot_tools and fetch the pinned Chromium source on Windows.
  Downloads ~90GB. Run once. Run setup-windows.ps1 first.

  .\build\fetch.ps1 -CheckoutDrive D:
#>
param(
  [Parameter(Mandatory=$true)][string]$CheckoutDrive
)
$ErrorActionPreference = 'Stop'

$FluxRoot = Split-Path -Parent $PSScriptRoot
Get-Content "$FluxRoot\chromium.version" | ForEach-Object {
  if ($_ -match '^\s*([A-Z_]+)=(.+)$') { Set-Variable -Name $Matches[1] -Value $Matches[2].Trim() }
}

$Root        = "$CheckoutDrive\flux-build"
$DepotTools  = "$Root\depot_tools"
$ChromiumDir = "$Root\chromium"

function Log($m) { Write-Host "==> $m" -ForegroundColor Cyan }

New-Item -ItemType Directory -Force -Path $Root | Out-Null

# depot_tools must precede any other Python on PATH or gclient uses the wrong one.
if (-not (Test-Path $DepotTools)) {
  Log "Cloning depot_tools"
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $DepotTools
}
$env:PATH = "$DepotTools;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = '0'

Log "Bootstrapping depot_tools (downloads its own Python/git; a few minutes)"
& "$DepotTools\gclient.bat" --version | Out-Null

New-Item -ItemType Directory -Force -Path $ChromiumDir | Out-Null
Set-Location $ChromiumDir

if (-not (Test-Path "$ChromiumDir\.gclient")) {
  Log "Writing .gclient (pin: $CHROMIUM_VERSION)"
@"
solutions = [
  {
    "name": "src",
    "url": "$CHROMIUM_UPSTREAM",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      "checkout_pgo_profiles": True,
      "checkout_nacl": False,
      "checkout_android": False,
      "checkout_ios": False,
    },
  },
]
"@ | Set-Content -Encoding ASCII "$ChromiumDir\.gclient"
}

if (-not (Test-Path "$ChromiumDir\src")) {
  Log "Cloning Chromium $CHROMIUM_VERSION (~30GB, 30-60 min)"
  # Shallow clone of the pinned tag only. Full history is ~40GB more.
  git -c core.longpaths=true -c core.fscache=true `
      clone --depth 1 --branch $CHROMIUM_VERSION $CHROMIUM_MIRROR "$ChromiumDir\src"
}

Set-Location "$ChromiumDir\src"

Log "gclient sync (pulls third_party; ~60GB, 1-2 hours; looks stalled at times)"
& "$DepotTools\gclient.bat" sync --no-history -D --nohooks

Log "gclient runhooks (fetches the Windows toolchain + PGO profiles)"
& "$DepotTools\gclient.bat" runhooks

Log "Fetch complete."
Write-Host "  Checkout: $ChromiumDir\src" -ForegroundColor Green
Write-Host "  Next:     .\build\sync.ps1 -CheckoutDrive $CheckoutDrive" -ForegroundColor Green
