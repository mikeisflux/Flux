#!/usr/bin/env bash
# Lint the chrome://flux console the way the Chromium build will.
#
# build_webui() runs stylelint and eslint as build steps, so a formatting nit in
# app.css is a hard build failure ~44k targets deep - about two hours in. This
# catches the same things up front.
#
# Covers the two that have actually broken the build:
#   - stylelint, using a mirror of Chromium's config
#   - mixed type/value imports, which Chromium's @webui-eslint plugin rejects
#     (that plugin is not on npm, so this is a targeted check, not full eslint)
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1
ROOT="$PWD"
RES="src/resources"
CACHE="${WEBUI_LINT_CACHE:-${TMPDIR:-/tmp}/flux-webui-lint}"
status=0

if ! command -v node >/dev/null 2>&1; then
  echo "node not found - CSS lint SKIPPED, so app.css is UNVERIFIED." >&2
else
  if [ ! -x "$CACHE/node_modules/.bin/stylelint" ]; then
    echo "Installing stylelint (one time)..." >&2
    mkdir -p "$CACHE"
    if ! (cd "$CACHE" && npm install --silent --no-fund --no-audit \
            stylelint@17 @stylistic/stylelint-plugin >/dev/null 2>&1); then
      echo "stylelint install failed - CSS lint SKIPPED, app.css is UNVERIFIED." >&2
    fi
  fi
  if [ -x "$CACHE/node_modules/.bin/stylelint" ]; then
    # Run from the cache dir so the plugin resolves; pass absolute paths, since
    # a relative one would resolve against the cache, not the repo.
    (cd "$CACHE" && ./node_modules/.bin/stylelint \
        --config "$ROOT/tools/stylelint.config.mjs" "$ROOT/$RES"/*.css) || status=1
  fi
fi

# Chromium's @webui-eslint/no-mixed-type-and-value-imports: an import statement
# may not carry both a value and a `type` specifier.
python3 - "$RES" <<'PY' || status=1
import glob, io, os, re, sys
bad = 0
for path in sorted(glob.glob(os.path.join(sys.argv[1], '*.ts'))):
    source = io.open(path, encoding='utf-8').read()
    for match in re.finditer(r'^import\s*\{(.*?)\}\s*from', source, re.S | re.M):
        if 'type ' in match.group(1):
            line = source[:match.start()].count('\n') + 1
            print(f'{path}:{line}: mixed type and value import - split them')
            bad += 1
sys.exit(1 if bad else 0)
PY

# Every resource has to be listed in BUILD.gn or it is simply not packed, and
# the failure shows up as a missing module at runtime - after a two-hour build.
# Adding a file and forgetting the build entry has already happened once.
python3 - "$RES" <<'GNPY' || status=1
import os, re, sys
res = sys.argv[1]
build = open(os.path.join(res, 'BUILD.gn'), encoding='utf-8').read()
listed = set(re.findall(r'"([^"]+\.(?:ts|html|css|json|svg|png))"', build))
bad = 0
for name in sorted(os.listdir(res)):
    if not name.endswith(('.ts', '.html', '.css', '.json')):
        continue
    # The mojom stub is dropped in by this script and removed again; during a
    # real build it is generated, so it must never be listed here.
    if name.endswith('.d.ts'):
        continue
    if name not in listed:
        print(f'{res}/{name}: not listed in BUILD.gn - it will not be packed')
        bad += 1
sys.exit(1 if bad else 0)
GNPY

# Type-check the console under the same strict settings build_webui compiles
# it with. This is the difference between a typo costing ten seconds and
# costing a ninety-minute build, so it is worth the one-time npm install.
TSC="$ROOT/tools/webui-typecheck/node_modules/.bin/tsc"
if [ ! -x "$TSC" ]; then
  echo "Installing typescript (one time)..." >&2
  if ! (cd "$ROOT/tools/webui-typecheck" && npm install --silent --no-fund \
          --no-audit typescript@5.6.3 >/dev/null 2>&1); then
    echo "typescript install failed - TS is UNVERIFIED. Say so rather than" >&2
    echo "claiming the console compiles." >&2
  fi
fi
if [ -x "$TSC" ]; then
  # The mojom bindings only exist inside a Chromium build, so a hand-kept stub
  # stands in. It has to sit next to the sources under its real name: a
  # relative import is not redirectable through tsconfig "paths".
  STUB="$RES/flux.mojom-webui.d.ts"
  cp "$ROOT/tools/webui-typecheck/stubs/flux.mojom-webui.d.ts" "$ROOT/$STUB"
  trap 'rm -f "$ROOT/$STUB"' EXIT
  "$TSC" -p "$ROOT/tools/webui-typecheck/tsconfig.json" || status=1
  rm -f "$ROOT/$STUB"
  trap - EXIT
fi

[ $status -eq 0 ] && echo "WebUI lint OK"
exit $status
