<#
  Configure and build Flux.
    .\build\build.ps1 -CheckoutDrive D: [-Config dev|debug|release] [-Targets chrome]
#>
param(
  [Parameter(Mandatory=$true)][string]$CheckoutDrive,
  [ValidateSet('dev','debug','release')][string]$Config = 'dev',
  [string[]]$Targets = @('chrome')
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

$FluxRoot   = Split-Path -Parent $PSScriptRoot
$Root       = "$CheckoutDrive\flux-build"
$DepotTools = "$Root\depot_tools"
$Src        = "$Root\chromium\src"
if (-not (Test-Path $Src)) { throw "No checkout at $Src. Run build\fetch.ps1 first." }

$env:PATH = "$DepotTools;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = '0'

$Out = switch ($Config) { 'dev' {'out\Dev'} 'debug' {'out\Debug'} 'release' {'out\Release'} }
function Log($m) { Write-Host "==> $m" -ForegroundColor Cyan }

# Preflight: free space on the build drive.
$vol = Get-Volume -DriveLetter $CheckoutDrive.TrimEnd(':')
$freeGB = [math]::Round($vol.SizeRemaining / 1GB, 1)
$need = if ($Config -eq 'release') { 80 } else { 45 }
Log "Config: $Config | $freeGB GB free (build output needs ~$need GB)"
if ($freeGB -lt $need) {
  Write-Host "  [warn] Low space - the build may fail partway." -ForegroundColor Yellow
}

# Push, don't Set: these scripts are chained on one line, and a bare
# Set-Location leaves the caller's shell parked in the Chromium tree, where the
# next command in the chain no longer resolves. The finally runs on throw and
# on exit alike.
Push-Location $Src
try {
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# GN reads args.gn out of the output directory, so the config is copied in
# rather than referenced. Each args file is self-contained - GN's // resolves
# against the Chromium root, not this repo, so there is nothing to import.
Copy-Item "$FluxRoot\build\args\$Config.gn" "$Out\args.gn" -Force

# Point Chromium at the local Visual Studio.
#
# With DEPOT_TOOLS_WIN_TOOLCHAIN=0, build/vs_toolchain.py locates VS by testing
# a hardcoded list of paths (_GenerateCandidatePaths). That list expects VS 2022
# under %ProgramFiles% - but the Build Tools installer's default is
# %ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools, which is not on
# the list. A perfectly good toolchain then reports as
# "No supported Visual Studio can be found."
#
# The list is consulted after $env:vs<year>_install, so setting that resolves
# it. Note GYP_MSVS_OVERRIDE_PATH does NOT: GetToolchainDir calls
# GetVisualStudioVersion separately, and that only reads the candidate paths.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
  # -products * includes Build Tools SKUs, which the default query omits.
  $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try {
    $vs = & $vswhere -products * -all -latest `
                     -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                     -format json | ConvertFrom-Json | Select-Object -First 1
  } finally { $ErrorActionPreference = $prev }
  if ($vs) {
    $year = switch ([int]$vs.installationVersion.Split('.')[0]) {
      18 { '2026' } 17 { '2022' } 16 { '2019' } 15 { '2017' } default { $null }
    }
    if ($year) {
      Set-Item -Path "env:vs${year}_install" -Value $vs.installationPath
      Log "Visual Studio $year at $($vs.installationPath)"
    } else {
      Write-Host "  [warn] VS $($vs.installationVersion) is not a version Chromium supports." -ForegroundColor Yellow
    }
  } else {
    Write-Host "  [warn] No VS install with the C++ x64 toolset. Run build\setup-windows.ps1." -ForegroundColor Yellow
  }
} else {
  Write-Host "  [warn] vswhere.exe not found - cannot locate Visual Studio." -ForegroundColor Yellow
}

Log "gn gen $Out"
Invoke-Native "$DepotTools\gn.bat" gen $Out

$ramGB = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB)
$jobs = @()
if ($ramGB -lt 16)      { Write-Host "  [warn] ${ramGB}GB RAM - capping to -j2" -ForegroundColor Yellow; $jobs = @('-j2') }
elseif ($ramGB -lt 32)  { $jobs = @('-j4') }

Log "Building $($Targets -join ' ') - first build takes hours; incremental takes minutes"
$sw = [Diagnostics.Stopwatch]::StartNew()
Invoke-Native "$DepotTools\autoninja.bat" -C $Out @jobs @Targets
$sw.Stop()

Log "Built in $($sw.Elapsed.ToString('hh\:mm\:ss'))"
Write-Host "  $Src\$Out\chrome.exe" -ForegroundColor Green
} finally {
  Pop-Location
}
