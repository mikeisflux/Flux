<#
.SYNOPSIS
  Preflight and setup for building Flux natively on Windows.
.DESCRIPTION
  Checks every prerequisite that otherwise fails hours into a build with an
  unhelpful error. Run from an ELEVATED PowerShell.
.EXAMPLE
  .\build\setup-windows.ps1 -CheckoutDrive D:
#>
param(
  [Parameter(Mandatory=$true)][string]$CheckoutDrive,
  [switch]$Fix   # apply the fixes rather than only reporting
)

$ErrorActionPreference = 'Stop'
$issues = @()
function Ok   ($m) { Write-Host "  [ok]   $m" -ForegroundColor Green }
function Warn ($m) { Write-Host "  [warn] $m" -ForegroundColor Yellow; $script:issues += $m }
function Bad  ($m) { Write-Host "  [FAIL] $m" -ForegroundColor Red;    $script:issues += $m }

Write-Host "`nFlux :: Windows build preflight`n" -ForegroundColor Cyan

# --- elevation -------------------------------------------------------------
$admin = ([Security.Principal.WindowsPrincipal] `
  [Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($admin) { Ok "Running elevated" }
else { Warn "Not elevated - long-path and Defender fixes will be skipped" }

# --- drive: filesystem, type, space ----------------------------------------
Write-Host "`nStorage" -ForegroundColor Cyan
$letter = $CheckoutDrive.TrimEnd(':')
$vol = Get-Volume -DriveLetter $letter -ErrorAction SilentlyContinue
if (-not $vol) { Bad "Drive $CheckoutDrive not found"; }
else {
  # NTFS is not optional: Chromium needs case sensitivity and hardlinks.
  if ($vol.FileSystemType -eq 'NTFS') { Ok "Filesystem is NTFS" }
  else { Bad "Filesystem is $($vol.FileSystemType). Chromium REQUIRES NTFS - exFAT/FAT32 will fail the build. Reformat as NTFS." }

  $freeGB = [math]::Round($vol.SizeRemaining / 1GB, 1)
  if ($freeGB -ge 250) { Ok "$freeGB GB free" }
  elseif ($freeGB -ge 150) { Warn "$freeGB GB free - enough for checkout + one dev build, but tight" }
  else { Bad "$freeGB GB free - need 150 GB minimum" }

  # Media type: SD cards and spinning disks are the difference between a 3h
  # build and a 20h one. Chromium's ~400k files make this IOPS-bound.
  $disk = Get-PhysicalDisk | Where-Object {
    $_.DeviceId -eq (Get-Partition -DriveLetter $letter).DiskNumber
  }
  if ($disk) {
    switch ($disk.MediaType) {
      'SSD'          { Ok "Media type: SSD" }
      'HDD'          { Bad "Media type: HDD - a Chromium build on spinning rust takes 3-5x longer. Use an SSD." }
      'Unspecified'  { Warn "Media type unknown (common for USB/SD). If this is an SD CARD, do not build here - IOPS are ~100x below NVMe and the build is random-I/O bound. External NVMe SSD over USB 3.2/Thunderbolt is fine." }
      default        { Warn "Media type: $($disk.MediaType) - verify this is solid state" }
    }
    if ($disk.BusType -in @('USB','SD')) {
      Warn "Bus type: $($disk.BusType). External is workable over USB 3.2+/Thunderbolt; avoid USB 2.0 and SD readers."
    }
  }
}

# --- toolchain -------------------------------------------------------------
Write-Host "`nToolchain" -ForegroundColor Cyan
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
  $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
  if ($vs) {
    Ok "Visual Studio $($vs.catalog.productDisplayVersion) with C++ tools"
    # ATL is required by some Chromium targets and is not in the default C++ workload.
    $atl = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.ATL -property installationPath
    if ($atl) { Ok "C++ ATL present" } else { Bad "C++ ATL missing - add via VS Installer > Modify > Individual components > 'C++ ATL for latest v143 build tools'" }
  } else { Bad "Visual Studio 2022 with 'Desktop development with C++' not found" }
} else { Bad "Visual Studio not installed. Need VS 2022 (Community is fine) + 'Desktop development with C++'" }

# Windows SDK + Debugging Tools. The latter is the classic silent failure.
$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
if (Test-Path "$sdkRoot\Include") {
  $sdks = Get-ChildItem "$sdkRoot\Include" -Directory | Sort-Object Name
  Ok "Windows SDK: $($sdks[-1].Name)"
  if (Test-Path "$sdkRoot\Debuggers\x64\dbghelp.dll") { Ok "Debugging Tools for Windows present" }
  else { Bad "Debugging Tools for Windows MISSING - the build fails on this with a confusing error. Settings > Apps > Windows SDK > Modify > check 'Debugging Tools for Windows'" }
} else { Bad "Windows 10/11 SDK not found" }

# --- environment -----------------------------------------------------------
Write-Host "`nEnvironment" -ForegroundColor Cyan
if ($env:DEPOT_TOOLS_WIN_TOOLCHAIN -eq '0') { Ok "DEPOT_TOOLS_WIN_TOOLCHAIN=0" }
else {
  if ($Fix) { [Environment]::SetEnvironmentVariable('DEPOT_TOOLS_WIN_TOOLCHAIN','0','User'); Ok "Set DEPOT_TOOLS_WIN_TOOLCHAIN=0 (restart your shell)" }
  else { Warn "DEPOT_TOOLS_WIN_TOOLCHAIN is not 0 - depot_tools will try Google's internal toolchain. Re-run with -Fix, or: setx DEPOT_TOOLS_WIN_TOOLCHAIN 0" }
}

$lp = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -ErrorAction SilentlyContinue).LongPathsEnabled
if ($lp -eq 1) { Ok "Win32 long paths enabled" }
else {
  if ($Fix -and $admin) { Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1; Ok "Enabled long paths (reboot required)" }
  else { Bad "Long paths disabled - Chromium exceeds MAX_PATH constantly. Re-run with -Fix as admin." }
}

if ((git config --system core.longpaths) -eq 'true') { Ok "git core.longpaths=true" }
else {
  if ($Fix -and $admin) { git config --system core.longpaths true; Ok "Set git core.longpaths=true" }
  else { Warn "git core.longpaths not set - checkout will fail on deep paths. git config --system core.longpaths true" }
}

# --- Defender --------------------------------------------------------------
# Real-time scanning of a 400k-file build can double total build time.
Write-Host "`nDefender" -ForegroundColor Cyan
$target = "$CheckoutDrive\chromium"
$excl = (Get-MpPreference -ErrorAction SilentlyContinue).ExclusionPath
if ($excl -and ($excl -contains $target)) { Ok "Checkout path excluded from real-time scanning" }
else {
  if ($Fix -and $admin) { Add-MpPreference -ExclusionPath $target; Ok "Excluded $target from Defender" }
  else { Warn "Add a Defender exclusion for $target - worth up to 2x build time. Add-MpPreference -ExclusionPath '$target'" }
}

# --- summary ---------------------------------------------------------------
Write-Host ""
if ($issues.Count -eq 0) {
  Write-Host "All checks passed. Next: build\fetch.ps1 -CheckoutDrive $CheckoutDrive" -ForegroundColor Green
} else {
  Write-Host "$($issues.Count) issue(s) to resolve before building." -ForegroundColor Yellow
  Write-Host "Re-run with -Fix (elevated) to apply what can be automated.`n" -ForegroundColor Yellow
}
