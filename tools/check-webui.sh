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

# Same for the connector definitions: data/connectors/*.json is the authoring
# source, and the packed resource is what the browser process actually reads.
python3 "$ROOT/tools/build-connectors-json.py" || status=1

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

# The mojom stub is transcribed by hand, so it can silently fall behind the
# mojom it stands in for - and a stub that is behind is worse than no stub: it
# type-checks clean and the build fails anyway. Adding OnConnectorChanged and
# forgetting one observer implementation cost a build exactly this way.
python3 - src/browser/mojom/flux.mojom \
    tools/webui-typecheck/stubs/flux.mojom-webui.d.ts <<'MOJOPY' || status=1
import re, sys

mojom = open(sys.argv[1], encoding='utf-8').read()
stub = open(sys.argv[2], encoding='utf-8').read()


def mojom_methods(interface):
    m = re.search(r'^interface ' + interface + r'\s*\{(.*?)^\};',
                  mojom, re.S | re.M)
    if not m:
        return None
    # A method is a name at the start of a statement followed by '('. Comment
    # lines are skipped so a name inside prose is not mistaken for one.
    body = re.sub(r'//[^\n]*', '', m.group(1))
    return set(re.findall(r'(?:^|\n)\s*([A-Z]\w*)\s*\(', body))


def stub_methods(block_name, kind):
    m = re.search(kind + r'\s+' + block_name + r'\b[^{]*\{(.*?)^\}',
                  stub, re.S | re.M)
    if not m:
        return None
    body = re.sub(r'//[^\n]*', '', m.group(1))
    # The `$: { ... }` block is mojo plumbing the generator adds, not a method
    # the mojom declares. Left in, it reports bindNewPipeAndPassReceiver as
    # drift on every run.
    body = re.sub(r'\$:\s*\{.*?\};', '', body, flags=re.S)
    return set(re.findall(r'(?:^|\n)\s*(\w+)\s*\(', body))


def lower_camel(name):
    return name[0].lower() + name[1:]


bad = 0
for interface, block, kind in (
        ('FluxPageHandler', 'FluxPageHandlerRemote', 'class'),
        ('FluxPageHandlerObserver', 'FluxPageHandlerObserverInterface',
         'interface')):
    declared = mojom_methods(interface)
    stubbed = stub_methods(block, kind)
    if declared is None:
        print(f'flux.mojom: cannot find interface {interface}')
        bad += 1
        continue
    if stubbed is None:
        print(f'stub: cannot find {kind} {block}')
        bad += 1
        continue
    for name in sorted(declared):
        if lower_camel(name) not in stubbed:
            print(f'stub: {block} is missing {lower_camel(name)}() - '
                  f'flux.mojom declares {interface}.{name}')
            bad += 1
    for name in sorted(stubbed):
        if name[0].upper() + name[1:] not in declared:
            print(f'stub: {block}.{name}() is not in flux.mojom - '
                  'the stub is ahead of, or out of step with, the real thing')
            bad += 1

sys.exit(1 if bad else 0)
MOJOPY

# fetch() cannot read a chrome:// URL, and the failure is invisible until the
# browser is running: the renderer registers only chrome-untrusted:, devtools:
# and isolated-app: as fetch-capable, so a fetch of a packed resource throws
#
#   Fetch API cannot load chrome://flux/skills.json.
#   URL scheme "chrome" is not supported.
#
# Every catalogue in the console was loaded this way, so every screen threw
# during render and sat on its loading skeleton forever. Use loadPackedJson()
# in resource.ts, which is XHR and does reach the WebUIDataSource.
# Comment lines are skipped - resource.ts explains the rule in prose, and the
# check must not fire on its own documentation.
fetch_calls=$(grep -rn '\bfetch(' "$RES"/*.ts 2>/dev/null \
  | grep -v ':[0-9]*:[[:space:]]*\(//\|\*\|/\*\)' || true)
if [ -n "$fetch_calls" ]; then
  echo "FAIL: fetch() in the console - it cannot load chrome:// URLs." >&2
  echo "$fetch_calls" >&2
  echo "      Use loadPackedJson() from resource.ts (XMLHttpRequest)." >&2
  status=1
fi

# A screen fired off with a bare `void ...render(...)` swallows its own
# rejection, and the skeleton drawn before it stays on screen with no error -
# which is how three broken catalogues looked like a browser that simply never
# finished loading. Route renders through settle() so a failure is visible.
if grep -n 'void this\.[A-Za-z]*\.render(' "$RES/app.ts" >/dev/null 2>&1; then
  echo "FAIL: a screen render in app.ts is fired off with a bare void." >&2
  grep -n 'void this\.[A-Za-z]*\.render(' "$RES/app.ts" >&2
  echo "      Wrap it in settle() so a rejection draws an error, not a" >&2
  echo "      skeleton that never resolves." >&2
  status=1
fi

# A WebUI data source enables Trusted Types - webui::SetupWebUIDataSource
# calls EnableTrustedTypesCSP, setting require-trusted-types-for 'script' - so
# assigning a string to .innerHTML throws at runtime:
#
#   Failed to set the 'innerHTML' property on 'Element':
#   This document requires 'TrustedHTML' assignment.
#
# Every screen in the console drew an inline SVG icon that way, so every screen
# threw partway through rendering. It compiles, it type-checks, and it only
# fails once the browser is running. Build icons with icons.ts, which uses
# createElementNS and never goes through the HTML parser.
html_sinks=$(grep -rn '\.\(innerHTML\|outerHTML\)[[:space:]]*=\|insertAdjacentHTML\|document\.write(' \
  "$RES"/*.ts 2>/dev/null \
  | grep -v ':[0-9]*:[[:space:]]*\(//\|\*\|/\*\)' || true)
if [ -n "$html_sinks" ]; then
  echo "FAIL: an HTML string sink in the console - Trusted Types blocks these." >&2
  echo "$html_sinks" >&2
  echo "      Build the nodes instead; see icons.ts for the SVG helpers." >&2
  status=1
fi

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
