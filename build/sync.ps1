<#
  Apply the Flux patch series and link our modules into the Chromium tree.
  Idempotent - safe to re-run after editing patches or src\.

  .\build\sync.ps1 -CheckoutDrive D:
#>
param(
  [Parameter(Mandatory=$true)][string]$CheckoutDrive
)
$ErrorActionPreference = 'Stop'

# Native tools (git, gclient, ninja) write progress to stderr. With
# $ErrorActionPreference = 'Stop', PowerShell treats ANY native stderr output as
# a terminating error, so a normal "Cloning into..." aborts the script. Run them
# through this instead: stderr is passed through as text, and success is judged
# by the exit code, which is the only thing that actually means anything.
function Invoke-Native {
  param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$Arguments
  )
  $previous = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    & $Exe @Arguments 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $previous
  }
  if ($code -ne 0) {
    throw "$Exe exited with code $code"
  }
}

$FluxRoot = Split-Path -Parent $PSScriptRoot
$Src      = "$CheckoutDrive\flux-build\chromium\src"
if (-not (Test-Path $Src)) { throw "No checkout at $Src. Run build\fetch.ps1 first." }

function Log($m) { Write-Host "==> $m" -ForegroundColor Cyan }

# Push, don't Set: these scripts are chained on one line, and a bare
# Set-Location leaves the caller's shell parked in the Chromium tree, where the
# next command in the chain no longer resolves. The finally runs on throw and
# on exit alike.
Push-Location $Src
try {

Log "Resetting tree to pristine"
$ErrorActionPreference = 'Continue'
git checkout -- . 2>&1 | Out-Null
git clean -fd chrome/browser/flux 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'

# Directory JUNCTIONS, not symlinks: junctions need no elevation or Developer
# Mode, and ninja follows them fine. Editing flux\src\ is picked up directly.
Log "Linking Flux modules into the tree"
foreach ($link in @(
    @{ Path = "$Src\chrome\browser\flux";           Target = "$FluxRoot\src\browser" },
    @{ Path = "$Src\chrome\browser\resources\flux"; Target = "$FluxRoot\src\resources" }
)) {
  if (Test-Path $link.Path) {
    # Remove-Item on a junction deletes the junction, not the target - but be
    # explicit about it rather than trusting -Recurse semantics.
    (Get-Item $link.Path).Delete()
  }
  New-Item -ItemType Junction -Path $link.Path -Target $link.Target | Out-Null
  Write-Host "    junction $($link.Path)" -ForegroundColor DarkGray
}

Log "Applying patch series"
$failed = $null
Get-Content "$FluxRoot\patches\series" | ForEach-Object {
  $line = $_.Trim()
  if ($line -eq '' -or $line.StartsWith('#')) { return }
  $patch = "$FluxRoot\patches\$line"

  # Normalize to LF before applying. Chromium's tree is LF throughout; if the
  # patch arrives with CRLF (a git checkout setting away on any Windows box),
  # every context line mismatches and git apply refuses it - reporting a
  # content failure for what is purely an encoding difference.
  $normalized = Join-Path ([System.IO.Path]::GetTempPath()) ("flux-" + [System.IO.Path]::GetFileName($patch))
  [System.IO.File]::WriteAllText(
      $normalized,
      ([System.IO.File]::ReadAllText($patch) -replace "`r`n", "`n"))
  $patch = $normalized

  # A failing --check is the normal probe result, not an error.
  $ErrorActionPreference = 'Continue'
  git apply --check $patch 2>&1 | Out-Null
  if ($LASTEXITCODE -eq 0) {
    git apply $patch 2>&1 | ForEach-Object { "$_" }
    Write-Host "    applied  $line" -ForegroundColor Green
  } else {
    git apply --reverse --check $patch 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
      Write-Host "    already  $line" -ForegroundColor DarkGray
    } else {
      Write-Host "    FAILED   $line" -ForegroundColor Red
      if (-not $failed) { $failed = $line }
    }
  }
}

$ErrorActionPreference = 'Stop'

if ($failed) {
  Write-Host ""
  Write-Host "Patch '$failed' did not apply." -ForegroundColor Yellow
  Write-Host "This is normal on a first build - Chromium moved out from under the patch." -ForegroundColor Yellow
  Write-Host "Diagnose with:" -ForegroundColor Yellow
  Write-Host "  cd $Src" -ForegroundColor White
  Write-Host "  git apply --verbose --reject `"$FluxRoot\patches\$failed`"" -ForegroundColor White
  Write-Host "Send the output back and the patch can be rebased." -ForegroundColor Yellow
  exit 1
}

Log "Sync complete. Next: .\build\build.ps1 -CheckoutDrive $CheckoutDrive"
} finally {
  Pop-Location
}
