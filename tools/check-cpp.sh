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
        # Complete inline /* ... */ pairs are Chromium's argument-annotation
        # style - /*max_body_size=*/N - and the line around one is code.
        # Removing them BEFORE the comment test is the whole point: the test
        # used to see a line starting with `/*`, find a `*/` on it, and skip
        # the line entirely. Every annotated call site in the tree was
        # therefore invisible to every rule below, and rule 4 shipped green
        # while the exact 10 MiB call it was written for walked past it.
        #
        # The rules still match against `line`, not this - rule 4 needs the
        # annotation text itself.
        code = re.sub(r'/\*.*?\*/', '', line)
        stripped = code.strip()

        # Skip comments so prose about a rule does not trip the rule.
        if in_block_comment:
            if '*/' in line:
                in_block_comment = False
            continue
        if stripped.startswith('/*'):
            in_block_comment = True
            continue
        if stripped.startswith('//') or not stripped:
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

        # 4. SimpleURLLoader::DownloadToString DCHECKs
        #    `max_body_size <= kMaxBoundedStringDownloadSize` (5 MiB). It is a
        #    hard ceiling, not a clamp, and dcheck_always_on makes it fatal:
        #    10 MiB in both providers killed the browser process on the first
        #    request it ever made, which was the API-key probe. Entering a key
        #    took the whole browser down, from either entry point.
        #
        #    Only a literal is checked, which is the form every call here used.
        #    Pass network::SimpleURLLoader::kMaxBoundedStringDownloadSize and
        #    the number cannot drift from Chromium's.
        m = re.search(r'max_body_size=\*/\s*([0-9*\s]+?)\s*\)', line)
        if m:
            try:
                size = eval(m.group(1), {'__builtins__': {}})
            except Exception:
                size = None
            if isinstance(size, int) and size > 5 * 1024 * 1024:
                report(path, i, line,
                       f'max_body_size {size} exceeds '
                       'kMaxBoundedStringDownloadSize (5242880); '
                       'DownloadToString DCHECKs on this and it is fatal - '
                       'pass network::SimpleURLLoader::'
                       'kMaxBoundedStringDownloadSize')

sys.exit(1 if bad else 0)
PY

# A pref registered SYNCABLE_PREF must also appear in Chromium's central
# SyncablePrefsDatabase allowlist. PrefModelAssociator::RegisterPref DCHECKs
# it, dcheck_always_on is true in dev.gn, and the failure is fatal at profile
# creation - the browser dies before drawing a window and exits 0 with nothing
# on screen, which is the least debuggable failure this project has hit.
#
# Allowed only if the patch series adds the pref to that allowlist. Nothing
# does today, so this is currently a flat prohibition, and the message says
# what it would take.
python3 - <<'SYNCPY' || status=1
import pathlib
import re
import sys

prefs = pathlib.Path('src/browser/flux_prefs.cc')
if not prefs.exists():
    sys.exit(0)

# The allowlist lives in Chromium; the series would have to patch it.
patched = set()
for patch in sorted(pathlib.Path('patches').glob('*.patch')):
    text = patch.read_text(encoding='utf-8')
    if 'syncable_prefs_database' in text:
        patched.update(re.findall(r'^\+.*?"(flux\.[\w.]+)"', text, re.M))

bad = 0
for i, line in enumerate(prefs.read_text(encoding='utf-8').splitlines(), 1):
    stripped = line.strip()
    if stripped.startswith('//') or 'SYNCABLE_PREF' not in stripped:
        continue
    print(f'src/browser/flux_prefs.cc:{i}: SYNCABLE_PREF without an entry in '
          "Chromium's SyncablePrefsDatabase is fatal at profile creation. "
          'Register it non-syncable, or add a patch to that allowlist.')
    print(f'    {stripped}')
    bad += 1

sys.exit(1 if bad else 0)
SYNCPY

# A source file that exists but is not in BUILD.gn compiles nowhere, so the
# error is not a compile error at all - it is an undefined symbol at LINK, at
# the very end of the build, after everything else has been paid for. Adding
# oauth_redirect_watcher.cc and forgetting the build entry cost exactly that.
#
# check-webui.sh has had this rule for src/resources since the same thing
# happened there; src/browser had nothing.
python3 - <<'GNPY' || status=1
import pathlib
import re
import sys

build_path = pathlib.Path('src/browser/BUILD.gn')
build = build_path.read_text(encoding='utf-8')

# Every sources list in the file, unioned. There is more than one target here
# - the mojom() one comes first - and matching only the first finds
# "mojom/flux.mojom" and reports every real source as missing.
#
# Only sources lists: deps and public_deps are full of quoted strings too, and
# a target label is not a file.
listed = set()
for block in re.findall(r'sources\s*=\s*\[(.*?)\]', build, re.S):
    listed.update(re.findall(r'"([^"]+)"', block))

# Some sources are compiled by Chromium's own target instead, added there by
# the patch series - the views subclasses under ui/ are, because depending on
# //chrome/browser/ui from the flux source_set would be circular. Those count
# as built, so the patches are scanned too: a line the series ADDS naming
# //chrome/browser/flux/<path>.
for patch in sorted(pathlib.Path('patches').glob('*.patch')):
    text = patch.read_text(encoding='utf-8')
    for added in re.findall(r'^\+.*?"//chrome/browser/flux/([^"]+)"', text,
                            re.M):
        listed.add(added)

root = pathlib.Path('src/browser')
on_disk = {
    str(p.relative_to(root)).replace('\\', '/')
    for p in root.rglob('*')
    if p.suffix in ('.h', '.cc')
}

bad = 0
for name in sorted(on_disk - listed):
    print(f'src/browser/{name}: not in src/browser/BUILD.gn - it will not be '
          'compiled, and anything referencing it fails at LINK')
    bad += 1
for name in sorted(listed - on_disk):
    if name.endswith(('.h', '.cc')):
        print(f'src/browser/BUILD.gn lists {name}, which does not exist')
        bad += 1

sys.exit(1 if bad else 0)
GNPY

# A switch over an enum that misses a case and has no default. Chromium builds
# with -Wswitch as an error, so this is a build failure rather than a runtime
# bug, and it fails in the middle of the build rather than at the end. It
# happens whenever an enum gains a value: every switch over it has to grow a
# case, and they are spread across the tree.
python3 - <<'SWITCHPY' || status=1
import pathlib
import re

enums = {}
mojom = pathlib.Path('src/browser/mojom/flux.mojom').read_text(encoding='utf-8')
for name, body in re.findall(r'^enum (\w+) \{(.*?)\n\};', mojom, re.S | re.M):
    enums[name] = {v for v in re.findall(r'^\s+(k\w+)', body, re.M)}
for h in pathlib.Path('src/browser').rglob('*.h'):
    for name, body in re.findall(r'enum class (\w+)[^{]*\{(.*?)\};',
                                 h.read_text(encoding='utf-8'), re.S):
        enums[name] = {v for v in re.findall(r'^\s+(k\w+)', body, re.M)}

bad = 0
for p in sorted(pathlib.Path('src/browser').rglob('*.cc')):
    text = p.read_text(encoding='utf-8')
    for m in re.finditer(r'switch\s*\(([^)]*)\)\s*\{', text):
        i, depth = m.end() - 1, 0
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = text[m.end():i]
        cases = re.findall(r'case\s+(?:[\w:]*::)?(\w+)::(k\w+)', body)
        bare = re.findall(r'case\s+(k\w+)', body)
        if not cases and not bare:
            continue
        if re.search(r'\bdefault\s*:', body):
            continue
        if cases:
            enum_name = cases[0][0]
            covered = {v for _, v in cases}
        else:
            covered = set(bare)
            enum_name = next((n for n, vals in enums.items()
                              if covered and covered <= vals), None)
        if enum_name not in enums:
            continue
        missing = sorted(enums[enum_name] - covered)
        if missing:
            line = text.count('\n', 0, m.start()) + 1
            print(f'{p}:{line}: switch over {enum_name} has no default and '
                  f'misses {", ".join(missing)} - -Wswitch is an error in '
                  f'Chromium, so this fails the build')
            bad += 1

raise SystemExit(1 if bad else 0)
SWITCHPY

# A method that shadows a base virtual without saying `override`. Chromium
# builds -Winconsistent-missing-override as an error, so the tidy case is a
# build break; the dangerous case is a signature that has drifted from the
# base, where without `override` it becomes a brand new method nothing calls,
# the base version runs instead, and the behaviour disappears with no
# diagnostic anywhere. Covers this project's own base classes only.
python3 - <<'OVERRIDEPY' || status=1
import collections
import pathlib
import re

ROOT = pathlib.Path('src/browser')
virtuals = collections.defaultdict(set)
bases = {}
decls = collections.defaultdict(list)

for h in sorted(set(ROOT.rglob('*.h'))):
    cls = None
    for i, raw in enumerate(h.read_text(encoding='utf-8').splitlines(), 1):
        m = re.match(r'^(?:class|struct)\s+(?:\w+\s+)?(\w+)\s*'
                     r'(?::\s*(.*?))?\s*\{?\s*$', raw)
        if m and (m.group(2) or raw.rstrip().endswith('{')):
            cls = m.group(1)
            if m.group(2):
                bases[cls] = re.findall(
                    r'(?:public|protected|private)\s+([\w:]+)', m.group(2))
            continue
        if not cls:
            continue
        s = raw.strip()
        if s.startswith('//'):
            continue
        mm = re.match(r'^(virtual\s+)?[\w:<>,\s&*]+?\s+(\w+)\(', s)
        if not mm:
            continue
        name = mm.group(2)
        if mm.group(1):
            virtuals[cls].add(name)
        # Searched, not captured by position: the parameter list is matched by
        # a greedy class that swallows a trailing `override`, which reported
        # every correctly-marked override in the tree as missing one.
        decls[cls].append((name, i, str(h), bool(re.search(r'\boverride\b', s))))

bad = 0
for cls, entries in decls.items():
    for base in bases.get(cls, []):
        base = base.split('::')[-1]
        if base not in virtuals:
            continue
        for name, line, path, has_override in entries:
            if name in virtuals[base] and not has_override:
                print(f'{path}:{line}: {cls}::{name}() matches a virtual in '
                      f'{base} but is not marked override')
                bad += 1

raise SystemExit(1 if bad else 0)
OVERRIDEPY

# A constructor initializer list in a different order from the declarations.
# Members are initialised in declaration order whatever the list says, so this
# is at best misleading and at worst reads a member that has not been
# constructed yet. Chromium builds -Wreorder as an error, so it is a build
# failure too.
python3 - <<'REORDERPY' || status=1
import collections
import pathlib
import re

ROOT = pathlib.Path('src/browser')
members = collections.defaultdict(list)
for h in sorted(ROOT.rglob('*.h')):
    cls = None
    for raw in h.read_text(encoding='utf-8').splitlines():
        m = re.match(r'^(?:class|struct)\s+(?:\w+\s+)?(\w+)', raw)
        if m:
            cls = m.group(1)
            continue
        if not cls or raw.strip().startswith('//') or 'static' in raw:
            continue
        mm = re.match(r'^\s{2,}(?:const\s+|mutable\s+)*'
                      r'[\w:]+(?:<.*>)?[\s&*]+(\w+_)\s*(?:=[^;]*)?;\s*$', raw)
        if mm:
            members[cls].append(mm.group(1))

bad = 0
for c in sorted(ROOT.rglob('*.cc')):
    text = c.read_text(encoding='utf-8')
    for m in re.finditer(r'(\w+)::\1\([^)]*\)\s*\n?\s*:\s*(.*?)\{', text, re.S):
        cls, init = m.group(1), m.group(2)
        if cls not in members:
            continue
        order = [n for n in re.findall(r'(\w+)\(', init) if n in members[cls]]
        expected = [n for n in members[cls] if n in order]
        if order != expected:
            line = text.count('\n', 0, m.start()) + 1
            print(f'{c}:{line}: {cls} initializes {", ".join(order)}')
            print(f'    declared in the order {", ".join(expected)}')
            print('    members are initialised in declaration order, and '
                  '-Wreorder is an error in Chromium')
            bad += 1

raise SystemExit(1 if bad else 0)
REORDERPY

# A virtual method with a non-empty body defined inline in a header.
#
# chromium-style's find-bad-constructs plugin rejects these, and /WX makes it
# an error rather than a warning - so `virtual bool NeedsPage() const { return
# false; }` failed seven translation units at once, nine minutes into a build.
# It is idiomatic C++ and reads like nothing at all, which is exactly why the
# compiler is the only thing that had ever objected to it.
#
# Only headers: the same shape inside a .cc is accepted, which is why all seven
# `override { return true; }` in browser_tools.cc compiled while the one
# declaration in the header did not.
python3 - <<'VIRTPY' || status=1
import pathlib, re, sys
root = pathlib.Path('src/browser')
bad = 0
for h in sorted(root.rglob('*.h')):
    for i, line in enumerate(h.read_text(encoding='utf-8').splitlines()):
        if 'virtual' not in line:
            continue
        # A body on the same line with something between the braces. `= 0;`,
        # `= default;`, `{}` and a bare declaration are all fine.
        m = re.search(r'\bvirtual\b[^;{]*\{\s*(\S[^}]*)\}', line)
        if m and m.group(1).strip():
            print(f'{h}:{i + 1}: virtual method with a non-empty inline body')
            print(f'    {line.strip()}')
            print('    chromium-style rejects this in a header and /WX makes '
                  'it an error - declare it here, define it in the .cc')
            bad += 1
sys.exit(1 if bad else 0)
VIRTPY

# ui::AXTree::Unserialize on a tree that is not freshly constructed.
#
# RequestAXTreeSnapshot hands back a complete standalone tree every time, not a
# delta. Unserializing a second snapshot into the tree holding the first is
# read as an incremental update, and on a live page whose node ids have moved
# that is an illegal reparent: a FATAL inside AXTree that takes the browser
# process down. It killed the browser the first time the agent read Gmail
# twice, and the code that did it carried a comment confidently asserting that
# Unserialize "replaces the tree's contents in place". It does not.
#
# The fix is a fresh tree per snapshot, so the rule is: the make_unique has to
# be right there above the Unserialize.
python3 - <<'AXPY' || status=1
import pathlib, re, sys
root = pathlib.Path('src/browser')
bad = 0
for c in sorted(root.rglob('*.cc')):
    lines = c.read_text(encoding='utf-8').splitlines()
    for i, line in enumerate(lines):
        if not re.search(r'\bUnserialize\s*\(', line):
            continue
        window = '\n'.join(lines[max(0, i - 3):i])
        if 'make_unique<ui::AXTree>' not in window:
            print(f'{c}:{i + 1}: Unserialize on a tree that was not just built')
            print(f'    {line.strip()}')
            print('    a snapshot is a whole tree, not a delta - build a fresh '
                  'ui::AXTree immediately above this or it is a FATAL reparent')
            bad += 1
for h in sorted(root.rglob('*.h')):
    for i, line in enumerate(h.read_text(encoding='utf-8').splitlines()):
        if re.match(r'\s*ui::AXTree\s+\w+_\s*;', line):
            print(f'{h}:{i + 1}: AXTree held by value')
            print(f'    {line.strip()}')
            print('    it has to be replaced wholesale per snapshot, and '
                  'AXTree deletes copy and move assignment - hold a unique_ptr')
            bad += 1
sys.exit(1 if bad else 0)
AXPY

[ $status -eq 0 ] && echo "C++ rules OK"
exit $status
