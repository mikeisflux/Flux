<#
  Pull, sync, build - one command, from any directory.

      C:\Users\Mike\Flux\go.ps1

  Nothing here depends on where the shell happens to be sitting, and there is
  nothing to chain with ';', so a paste that arrives out of order still works.

  Options:
    -Config debug|release   default dev
    -NoPull                 build what is already checked out
    -NoSync                 skip patches/junctions (nothing changed under patches\)
    -Jobs N                 force the parallel job count (default: from free RAM)
#>
param(
  [ValidateSet('dev','debug','release')][string]$Config = 'dev',
  [string]$CheckoutDrive = '',
  [switch]$NoPull,
  [switch]$NoSync,
  # Override the job count build.ps1 computes from free memory, e.g. -Jobs 8.
  [int]$Jobs = 0
)
$ErrorActionPreference = 'Stop'

# $PSScriptRoot, never the current directory - this script is meant to be
# invoked by absolute path from wherever you already are.
$FluxRoot = $PSScriptRoot
$Log = Join-Path $FluxRoot 'build.log'

function Step($m) { Write-Host "`n=== $m" -ForegroundColor Magenta }

# Find the checkout rather than making the caller remember which drive it went
# on. First drive with a Chromium tree under \flux-build wins.
if (-not $CheckoutDrive) {
  foreach ($d in (Get-PSDrive -PSProvider FileSystem)) {
    if (Test-Path "$($d.Name):\flux-build\chromium\src") { $CheckoutDrive = "$($d.Name):"; break }
  }
  if (-not $CheckoutDrive) {
    throw "No Chromium checkout found (looked for <drive>\flux-build\chromium\src on every drive). Run build\fetch.ps1 -CheckoutDrive C: first."
  }
}

if (-not $NoPull) {
  Step "Pulling latest Flux"
  # git writes progress to stderr; judge it by the exit code, not by whether
  # anything showed up on stderr.
  $prev = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    git -C $FluxRoot pull 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
  } finally { $ErrorActionPreference = $prev }
  if ($code -ne 0) { throw "git pull failed with code $code" }
}

if (-not $NoSync) {
  Step "Applying patches and linking Flux into the tree"
  & "$FluxRoot\build\sync.ps1" -CheckoutDrive $CheckoutDrive 2>&1 | Tee-Object -FilePath $Log
  if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) { throw "sync failed - see $Log" }
}

Step "Building"
& "$FluxRoot\build\build.ps1" -CheckoutDrive $CheckoutDrive -Config $Config -Jobs $Jobs 2>&1 |
  Tee-Object -FilePath $Log -Append

Write-Host "`nFull log: $Log" -ForegroundColor DarkGray
