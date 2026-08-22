#!/usr/bin/env bash
# Parse every .ps1 in the repo with the real PowerShell parser.
#
# This exists because PowerShell syntax errors kept reaching the user's machine:
# they are invisible to review (`"$runner: gn clean"` reads fine, and parses as
# a drive-qualified variable), and this container has no PowerShell, so there
# was nothing checking them. Now there is.
#
# Downloads pwsh on first run and caches it. Exits non-zero on any parse error.
set -uo pipefail

CACHE="${PWSH_CACHE:-${TMPDIR:-/tmp}/flux-pwsh}"
PWSH="$CACHE/pwsh"
VERSION="7.4.6"
URL="https://github.com/PowerShell/PowerShell/releases/download/v${VERSION}/powershell-${VERSION}-linux-x64.tar.gz"

if [ ! -x "$PWSH" ]; then
  echo "Fetching PowerShell $VERSION (one time, ~70MB)..." >&2
  mkdir -p "$CACHE" || exit 1
  if ! curl -sSL --retry 3 -o "$CACHE/ps.tar.gz" "$URL"; then
    echo "Could not download PowerShell - skipping the syntax check." >&2
    echo "That means .ps1 changes are UNVERIFIED. Say so rather than claiming they work." >&2
    exit 0
  fi
  tar -xzf "$CACHE/ps.tar.gz" -C "$CACHE" && chmod +x "$PWSH" || exit 1
  rm -f "$CACHE/ps.tar.gz"
fi

cd "$(dirname "$0")/.." || exit 1

# This runs as a hook after every edit, so skip the ~1s parser startup when no
# .ps1 has actually changed since the last clean run.
STAMP="$CACHE/last-checked"
FINGERPRINT=$(find . -name '*.ps1' -type f -printf '%p %s %T@\n' 2>/dev/null | sort | md5sum)
if [ -f "$STAMP" ] && [ "$FINGERPRINT" = "$(cat "$STAMP")" ]; then
  exit 0
fi

"$PWSH" -NoProfile -Command '
using namespace System.Management.Automation.Language
$bad = 0
Get-ChildItem -Recurse -Filter *.ps1 -File | ForEach-Object {
  $errors = $null
  [System.Management.Automation.Language.Parser]::ParseFile(
      $_.FullName, [ref]$null, [ref]$errors) | Out-Null
  if ($errors) {
    $bad++
    Write-Host "FAIL $($_.FullName)"
    foreach ($e in $errors) {
      Write-Host "     line $($e.Extent.StartLineNumber): $($e.Message)"
    }
    return
  }

  # Variable names are case-insensitive, so a local $jobs and a parameter
  # [int]$Jobs are ONE variable - and the parameter type sticks. Assigning an
  # array to the local then fails at runtime with a conversion error that names
  # neither variable. Parsing cannot see this; comparing the two can.
  $ast = [Parser]::ParseFile($_.FullName, [ref]$null, [ref]$null)
  $typed = @{}
  foreach ($p in $ast.FindAll({ $args[0] -is [ParameterAst] }, $true)) {
    if ($p.Attributes | Where-Object { $_ -is [TypeConstraintAst] }) {
      $typed[$p.Name.VariablePath.UserPath] = $p.StaticType
    }
  }
  foreach ($a in $ast.FindAll({ $args[0] -is [AssignmentStatementAst] }, $true)) {
    if ($a.Left -isnot [VariableExpressionAst]) { continue }
    $name = $a.Left.VariablePath.UserPath
    foreach ($declared in $typed.Keys) {
      if ($name -ne $declared) { continue }          # -ne is case-insensitive
      $isArray = $a.Right.Expression -is [ArrayExpressionAst] -or
                 $a.Right.Expression -is [ArrayLiteralAst]
      $caseDiffers = -not [string]::Equals($name, $declared, "Ordinal")
      if ($caseDiffers -or ($isArray -and -not $typed[$declared].IsArray)) {
        $bad++
        Write-Host "FAIL $($_.FullName)"
        Write-Host ("     line {0}: `$$name collides with the [{1}] parameter `$$declared" -f `
                    $a.Extent.StartLineNumber, $typed[$declared].Name)
      }
    }
  }
}
if ($bad -eq 0) { Write-Host "PowerShell syntax OK" }
exit $bad'

status=$?
[ $status -eq 0 ] && printf '%s' "$FINGERPRINT" > "$STAMP"
exit $status
