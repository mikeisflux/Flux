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

Set-Location $Src
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# GN reads args.gn from the output dir. Our configs import a shared .gni via
# //flux/, which requires the junction created by sync.ps1.
Copy-Item "$FluxRoot\build\args\$Config.gn" "$Out\args.gn" -Force

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
