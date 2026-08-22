#!/usr/bin/env bash
# Catch the Chromium-specific C++ mistakes that have actually broken builds.
#
# This container cannot compile Chromium, so every C++ error is found on the
# user's machine, minutes into a build, and costs a round trip. This is not a
# compiler and does not pretend to be one: it is a list of the specific things
# that have gone wrong, each one added after it cost a build.
#
# Rules are greps over src/browser/**.{h,cc}. A rule earns its place by having
# failed a real build once.
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1
status=0

python3 - <<'PY' || status=1
import pathlib
import re
import sys

ROOT = pathlib.Path('src/browser')
bad = 0


def report(path, line_no, line, message):
    global bad
    print(f'{path}:{line_no}: {message}')
    print(f'    {line.strip()}')
    bad += 1


for path in sorted(ROOT.rglob('*')):
    if path.suffix not in ('.h', '.cc'):
        continue
    lines = path.read_text(encoding='utf-8').splitlines()
    in_block_comment = False

    for i, line in enumerate(lines, start=1):
        stripped = line.strip()

        # Skip comments so prose about a rule does not trip the rule.
        if in_block_comment:
            if '*/' in stripped:
                in_block_comment = False
            continue
        if stripped.startswith('/*'):
            if '*/' not in stripped:
                in_block_comment = True
            continue
        if stripped.startswith('//'):
            continue

        # 1. base::JSONReader::Read / ReadDict / ReadList lost their
        #    single-argument overloads; `options` is required. Omitting it is
        #    "too few arguments to function call, expected at least 2".
        m = re.search(r'JSONReader::(Read|ReadDict|ReadList)\s*\((.*)$', line)
        if m:
            # The call may wrap, so look at the rest of the statement.
            rest = m.group(2)
            j = i
            while ';' not in rest and j < len(lines):
                rest += lines[j]
                j += 1
            call = rest.split(';')[0]
            if ',' not in call:
                report(path, i, line,
                       f'JSONReader::{m.group(1)} needs an options argument - '
                       'pass base::JSON_PARSE_RFC, as the providers do')

        # 2. The chromium-rawref plugin rejects reference-typed fields:
        #    "[chromium-rawref] Use raw_ref<T> instead of a native reference."
        #    Only fields, so this looks for a declaration ending in `_;`.
        if path.suffix == '.h':
            m = re.match(r'^\s+(?:const\s+)?[\w:]+(?:<[^>]*>)?\s*&\s*\w+_\s*;',
                         line)
            if m:
                report(path, i, line,
                       'a reference-typed field is rejected by the '
                       'chromium-rawref plugin - use raw_ref<T>')

        # 3. A raw pointer field trips chromium-rawptr the same way. raw_ptr<T>
        #    is the form used everywhere else in this tree.
        if path.suffix == '.h':
            m = re.match(
                r'^\s+(?:const\s+)?(?!raw_ptr|raw_ref)[\w:]+(?:<[^>]*>)?\s*\*\s*\w+_\s*;',
                line)
            if m:
                report(path, i, line,
                       'a raw pointer field is rejected by the raw-ptr '
                       'plugin - use raw_ptr<T>')

sys.exit(1 if bad else 0)
PY

[ $status -eq 0 ] && echo "C++ rules OK"
exit $status
