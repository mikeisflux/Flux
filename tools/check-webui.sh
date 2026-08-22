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

# The packed skill library is generated from data/skills/*.md. Regenerate and
# diff rather than trusting it: the markdown is the authoring format, and a
# packed copy that has drifted from it ships stale instructions to the agent.
python3 "$ROOT/tools/build-skills-json.py" || status=1

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

# grit reserves a fixed number of resource IDs for the console, and it is a
# hard bound: exceed it and the build dies ~500 targets in with IdRangeOverflow,
# naming a file in gen/ that does not explain itself. That has cost one build
# already. The reservation lives in the patch series, the usage lives in
# BUILD.gn, and nothing but this connects the two.
python3 - "$RES" patches/0006-grit-resource-ids.patch <<'GRITPY' || status=1
import os, re, sys
res, patch = sys.argv[1], sys.argv[2]
build = open(os.path.join(res, 'BUILD.gn'), encoding='utf-8').read()


def count(name):
    m = re.search(name + r'\s*(?:\+)?=\s*\[(.*?)\]', build, re.S)
    return len(re.findall(r'"[^"]+"', m.group(1))) if m else 0


# One include each: a static file as-is, a .ts as its compiled .js, and the
# mojom bindings as theirs.
used = count('static_files') + count('ts_files') + count('mojo_files')

spec = open(patch, encoding='utf-8').read()
m = re.search(r'^\+.*"sizes":\s*\{"includes":\s*\[(\d+)\]', spec, re.M)
if not m:
    print(f'{patch}: cannot find the includes reservation - has the patch '
          'been rewritten?')
    sys.exit(1)
reserved = int(m.group(1))

if used > reserved:
    print(f'{patch}: the console needs {used} grit include IDs but only '
          f'{reserved} are reserved. Raise sizes.includes and regenerate the '
          'patch, or the build fails at the resources_grit step.')
    sys.exit(1)
print(f'  grit IDs: {used}/{reserved} used')
GRITPY

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
