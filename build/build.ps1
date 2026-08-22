<#
  Configure and build Flux.
    .\build\build.ps1 -CheckoutDrive D: [-Config dev|debug|release] [-Targets chrome]
#>
param(
  [Parameter(Mandatory=$true)][string]$CheckoutDrive,
  [ValidateSet('dev','debug','release')][string]$Config = 'dev',
  [string[]]$Targets = @('chrome'),
  # Override the computed job count, e.g. -Jobs 8.
  [int]$Jobs = 0
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

# autoninja refuses to switch build runners inside a populated output directory
# ("Run gn clean before switching from siso to ninja"). Rather than sniff for
# whatever state files siso leaves behind - a filename we would be guessing at,
# and a guess that fails silently - record what we generated with and compare.
# Correct by construction, and it catches a switch in either direction.
$runner = if (Select-String -Path "$Out\args.gn" -Pattern '^\s*use_siso\s*=\s*false' -Quiet) { 'ninja' } else { 'siso' }
$stamp = "$Out\.flux_runner"
$previous = if (Test-Path $stamp) { (Get-Content $stamp -Raw).Trim() } else { '' }
# No stamp beside an existing build.ninja means the dir predates this check,
# so its runner is unknown and a clean is the safe read.
if ((Test-Path "$Out\build.ninja") -and $previous -ne $runner) {
  Log "Build runner is now ${runner}: gn clean $Out"
  Invoke-Native "$DepotTools\gn.bat" clean $Out
}

Log "gn gen $Out"
Invoke-Native "$DepotTools\gn.bat" gen $Out
Set-Content -Path $stamp -Value $runner

# Pick -j from FREE memory, not core count.
#
# ninja defaults to cores+2, and clang-cl holds the whole translation unit in
# memory: most are modest, but Chromium's larger ones (chrome_metrics_service_
# client, connectors_service) peak at several GB each. Enough of those landing
# together and clang dies with "LLVM ERROR: out of memory" - which reads like a
# compiler bug and is really just too many jobs.
#
# Free memory, not total: whatever else is running on the machine is memory the
# build cannot have. 4GB per job leaves room for the occasional heavy TU.
if ($Jobs -gt 0) {
  $j = $Jobs
  Log "Using -j$j (requested)"
} else {
  $cores = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
  # FreePhysicalMemory is in kilobytes.
  $freeGB = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB
  $byMemory = [math]::Max(2, [math]::Floor($freeGB / 4))
  $j = [math]::Min($cores, $byMemory)
  Log ("-j{0} ({1} cores, {2:N1} GB free -> room for {3} jobs at 4GB each)" -f `
       $j, $cores, $freeGB, $byMemory)
  if ($j -lt $cores) {
    Write-Host "  Close other apps and re-run to use more cores." -ForegroundColor DarkGray
  }
}
# Not $jobs: PowerShell variable names are case-insensitive, so that
# would assign an array into the [int]$Jobs parameter and throw.
$ninjaFlags = @("-j$j")

Log "Building $($Targets -join ' ') - first build takes hours; incremental takes minutes"
$sw = [Diagnostics.Stopwatch]::StartNew()
Invoke-Native "$DepotTools\autoninja.bat" -C $Out @ninjaFlags @Targets
$sw.Stop()

Log "Built in $($sw.Elapsed.ToString('hh\:mm\:ss'))"
Write-Host "  $Src\$Out\chrome.exe" -ForegroundColor Green
} finally {
  Pop-Location
}
