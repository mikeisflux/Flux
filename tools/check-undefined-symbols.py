#!/usr/bin/env python3
"""Methods declared in a header and defined nowhere.

An undefined symbol is the most expensive failure this project has: every
translation unit compiles, and it dies at LINK after the whole two-hour build
has already happened. CLAUDE.md names the file-not-in-BUILD.gn version of it;
this is the other one - a declaration whose definition was never written, or
was renamed on one side only.

The worst failure this project has: it compiles every translation unit fine
and dies at LINK, after everything else has already been built. CLAUDE.md
already names the file-not-in-BUILD.gn version of this; the same error comes
from a declaration whose definition was never written, or was renamed.

Skips what legitimately has no out-of-line definition: pure virtuals, methods
defined inline in the header, `= default`, `= delete`, and overrides of a base
class that supplies a body.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path('src/browser')

decls = []   # (class, name, header, line)
for h in sorted(ROOT.rglob('*.h')):
    cls = None
    lines = h.read_text(encoding='utf-8').splitlines()
    for i, raw in enumerate(lines, 1):
        m = re.match(r'^(?:class|struct)\s+(?:\w+\s+)?(\w+)', raw)
        if m:
            cls = m.group(1)
            continue
        s = raw.strip()
        if s.startswith('//') or not cls:
            continue
        # Join a wrapped declaration.
        joined, k = raw, i - 1
        while (joined.count('(') > joined.count(')') and k + 1 < len(lines)
               and k - (i - 1) < 8):
            k += 1
            joined = joined.rstrip() + ' ' + lines[k].strip()
        j = joined.strip()
        # Anchored on a trailing `;`, and non-greedy inside the parens.
        # Greedy matching made an inline one-liner look like a declaration:
        # for `Scheduler* scheduler() { return scheduler_.get(); }` the
        # parameter group swallowed `) { return scheduler_.get(`, the closing
        # paren matched inside `get()`, and the leftover started with `;`. Ten
        # accessors were reported as undefined symbols that way.
        m = re.match(
            r'^(?:virtual\s+|static\s+|explicit\s+|inline\s+)*'
            r'(?:[\w:]+(?:<[^;]*>)?[\s&*]+)?'
            r'(~?\w+)\((.*?)\)\s*(?:const\s*)?(?:noexcept\s*)?'
            r'(?:override\s*)?(?:final\s*)?;\s*(?://.*)?$', j)
        if not m:
            continue
        name = m.group(1)
        if '= 0' in j or '= default' in j or '= delete' in j:
            continue
        if j.startswith('using ') or name in ('if', 'for', 'while', 'switch',
                                              'return'):
            continue
        # ALL_CAPS is a macro, not a method. WEB_UI_CONTROLLER_TYPE_DECL();
        # matched the declaration shape exactly.
        if name.isupper():
            continue
        decls.append((cls, name, str(h), i))

defined = set()
defined_names = set()
# Headers too: an out-of-class inline definition below the class body -
# `inline Message Message::Clone() const {` - is a definition, and
# scanning only .cc reported it as an undefined symbol.
for c in sorted(list(ROOT.rglob('*.cc')) + list(ROOT.rglob('*.h'))):
    text = c.read_text(encoding='utf-8')
    for m in re.finditer(r'(\w+)::(~?\w+)\s*\(', text):
        defined.add((m.group(1), m.group(2)))
        defined_names.add(m.group(2))
    # Free functions. RegisterBrowserTools, ResolveTemplate,
    # FirstUnresolvedPlaceholder, GetApiKey and RegisterProfilePrefs are all
    # namespace-level, so a scan for `Class::name(` alone reported every one of
    # them as an undefined symbol.
    for m in re.finditer(r'^[\w:][\w:<>,\s&*]*?[\s&*](~?\w+)\([^;{]*\)\s*'
                         r'(?:const\s*)?\{', text, re.M):
        defined_names.add(m.group(1))

missing = [d for d in decls
           if (d[0], d[1]) not in defined and d[1] not in defined_names]

for cls, name, h, line in missing:
    print(f'{h}:{line}: {cls}::{name}() declared, defined nowhere '
          f'- undefined symbol at LINK, after the whole build')

if missing:
    print(f'\n{len(missing)} undefined symbol(s)')
    sys.exit(1)
print(f'Symbols OK ({len(decls)} out-of-line declarations, all defined)')
