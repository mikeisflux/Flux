<#
  Apply the Flux patch series and link our modules into the Chromium tree.
  Idempotent - safe to re-run after editing patches or src\.

  .\build\sync.ps1 -CheckoutDrive D:
#>
param(
  [Parameter(Mandatory=$true)][string]$CheckoutDrive
)
$ErrorActionPreference = 'Stop'

$FluxRoot = Split-Path -Parent $PSScriptRoot
$Src      = "$CheckoutDrive\flux-build\chromium\src"
if (-not (Test-Path $Src)) { throw "No checkout at $Src. Run build\fetch.ps1 first." }

function Log($m) { Write-Host "==> $m" -ForegroundColor Cyan }

Set-Location $Src

Log "Resetting tree to pristine"
git checkout -- . 2>$null
git clean -fd chrome/browser/flux 2>$null

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

  git apply --check $patch 2>$null
  if ($LASTEXITCODE -eq 0) {
    git apply $patch
    Write-Host "    applied  $line" -ForegroundColor Green
  } else {
    git apply --reverse --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) {
      Write-Host "    already  $line" -ForegroundColor DarkGray
    } else {
      Write-Host "    FAILED   $line" -ForegroundColor Red
      if (-not $failed) { $failed = $line }
    }
  }
}

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
