#!/usr/bin/env python3
"""Three ways a name can be unreachable from where it is used.

Chromium does not build in this container, so every one of these reaches the
user's machine and costs a build. All three arrived together in one round:

  error: use of undeclared identifier 'RunTool'
  error: out-of-line definition of 'RunTool' does not match any declaration
  error: 'PendingApprovals' is a private member of 'flux::FluxAgentService'
  error: use of undeclared identifier 'NormalizeCommand'

`check-undefined-symbols.py` looks for a declaration with no definition. Every
rule here is the reverse or the sideways version: a definition no declaration
reaches, a member the caller may not touch, a free function defined below the
line that calls it. All three read as ordinary, correct C++.

The header parser keeps a STACK of open classes, because the first version
kept one name and a nested `class Delegate {` destroyed it: when the nested
body closed, the enclosing class was forgotten, every member declared below it
went unrecorded, and the check reported 103 findings of which every one was
wrong. `agent_runner.h` opens `class Delegate` on line 39 and `private:` on
line 98 - so AgentRunner's entire private section was invisible.

Rule 3 is type-directed for the same reason. Judging by method name alone
called `observer_->OnLearnedFact()` a private member of FluxPageHandler: the
receiver is a `mojo::Remote`, whose interface is generated and lives in no
header here. It now resolves the receiver's declared type first and only
judges a call whose receiver it can name.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path('src/browser')

DECL_RE = re.compile(
    r'^(?:virtual\s+|static\s+|explicit\s+|inline\s+|constexpr\s+|const\s+)*'
    r'(?:[\w:]+(?:<[^;]*>)?[\s&*]+)?'
    r'(~?\w+)\((.*?)\)\s*(?:const\s*)?(?:noexcept\s*)?'
    r'(?:override\s*)?(?:final\s*)?(?:=\s*(?:0|default|delete)\s*)?;'
    r'\s*(?://.*)?$')

# An inline definition inside a class body.
INLINE_RE = re.compile(
    r'^(?:virtual\s+|static\s+|inline\s+|constexpr\s+|const\s+)*'
    r'(?:[\w:]+(?:<[^;]*>)?[\s&*]+)?(~?\w+)\([^;]*\)\s*'
    r'(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?[:{]')

CLASS_RE = re.compile(r'^(class|struct)\s+(?:\w+\s+)?(\w+)'
                      r'(?:\s*(?:final\b|:)|\s*\{|\s*$)')

# `const raw_ptr<FluxAgentService> service_;`, `std::unique_ptr<X> x_;`,
# `FluxAgentService* service_;`, `Foo& foo_;`
FIELD_RE = re.compile(
    r'^(?:const\s+|mutable\s+|static\s+)*'
    r'(?:raw_ptr|raw_ref|std::unique_ptr|scoped_refptr|base::WeakPtr)?'
    r'<?\s*([A-Z]\w+)\s*>?\s*[*&]?\s*(\w+_)\s*(?:=[^;]*)?;')


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def join_wrapped(lines, i):
    joined, k = lines[i], i
    while (joined.count('(') > joined.count(')') and k + 1 < len(lines)
           and k - i < 8):
        k += 1
        joined = joined.rstrip() + ' ' + lines[k].strip()
    return joined.strip()


def parse_headers():
    """members[class][method] = access; fields[class][member_] = type."""
    members, fields, friends, free = {}, {}, {}, set()
    for h in sorted(ROOT.rglob('*.h')):
        lines = strip_comments(h.read_text(encoding='utf-8')).splitlines()
        stack = []      # [(class, access, depth_at_open)]
        depth = 0
        for i, raw in enumerate(lines):
            s = raw.strip()
            opened = None
            m = CLASS_RE.match(s)
            if m and not s.rstrip().endswith(';'):
                opened = m.group(2)
                members.setdefault(opened, {})
                fields.setdefault(opened, {})
            elif stack:
                cls, access, _ = stack[-1]
                am = re.match(r'^(public|protected|private)\s*:', s)
                if am:
                    stack[-1] = (cls, am.group(1), stack[-1][2])
                elif s.startswith('friend '):
                    fm = re.search(r'\b(\w+)\s*;', s)
                    if fm:
                        friends.setdefault(cls, set()).add(fm.group(1))
                else:
                    j = join_wrapped(lines, i)
                    dm = DECL_RE.match(j) or INLINE_RE.match(j)
                    if dm and not dm.group(1).isupper() \
                            and not j.startswith('using '):
                        members[cls].setdefault(dm.group(1), access)
                    else:
                        fm = FIELD_RE.match(s)
                        if fm:
                            fields[cls][fm.group(2)] = fm.group(1)
            else:
                j = join_wrapped(lines, i)
                dm = DECL_RE.match(j)
                if dm and not dm.group(1).isupper():
                    free.add(dm.group(1))

            depth += raw.count('{') - raw.count('}')
            if opened is not None:
                # A class's members start public in a struct, private in a
                # class. depth is now inside the body.
                stack.append((opened,
                              'public' if m.group(1) == 'struct' else 'private',
                              depth - 1))
            while stack and depth <= stack[-1][2]:
                stack.pop()
    return members, fields, friends, free


def main():
    members, fields, friends, header_free = parse_headers()
    findings = []

    # ---- Rule 1: Class::Name defined in a .cc, declared in no class body.
    checked_defs = 0
    for c in sorted(ROOT.rglob('*.cc')):
        text = strip_comments(c.read_text(encoding='utf-8'))
        for m in re.finditer(r'^[\w:<>,\s&*~]*?\b(\w+)::(~?\w+)\s*\([^;]*?\)'
                             r'[\w\s:,()&*]*\{', text, re.M):
            cls, name = m.group(1), m.group(2)
            if cls not in members or name.isupper():
                continue
            checked_defs += 1
            if name in members[cls]:
                continue
            line = text[:m.start()].count('\n') + 1
            findings.append(
                f'{c}:{line}: {cls}::{name}() is defined here and declared in '
                f'no class body - clang rejects the definition outright')

    # ---- Rule 2: a free function called above the line that defines it.
    checked_free = 0
    for c in sorted(ROOT.rglob('*.cc')):
        text = strip_comments(c.read_text(encoding='utf-8'))
        defs, decls = {}, {}
        for m in re.finditer(r'^(?:static\s+|inline\s+|constexpr\s+)*'
                             r'(?:[\w:]+(?:<[^>;{]*>)?[\s&*]+)'
                             r'(\w+)\([^;{]*\)\s*(?:const\s*)?\{', text, re.M):
            defs.setdefault(m.group(1), text[:m.start()].count('\n') + 1)
        for m in re.finditer(r'^(?:static\s+|inline\s+)*'
                             r'(?:[\w:]+(?:<[^>;]*>)?[\s&*]+)(\w+)\([^;{]*\);',
                             text, re.M):
            decls.setdefault(m.group(1), text[:m.start()].count('\n') + 1)
        for name, def_line in sorted(defs.items()):
            if name in header_free:
                continue
            # A method of ours sharing this name makes call sites ambiguous
            # to a scan. Skip rather than guess.
            if any(name in v for v in members.values()):
                continue
            calls = [text[:m.start()].count('\n') + 1
                     for m in re.finditer(r'(?<![\w:.>])' + re.escape(name)
                                          + r'\s*\(', text)]
            calls = [ln for ln in calls if ln != def_line]
            if not calls:
                continue
            checked_free += 1
            first = min(calls)
            if first < def_line and decls.get(name, def_line) >= first:
                findings.append(
                    f'{c}:{first}: {name}() is called here and defined at line '
                    f'{def_line}, with no declaration above the call')

    # ---- Rule 3: a private member reached through a field whose type we know.
    checked_private = 0
    for c in sorted(list(ROOT.rglob('*.cc')) + list(ROOT.rglob('*.h'))):
        text = strip_comments(c.read_text(encoding='utf-8'))
        own = {m.group(1) for m in re.finditer(r'\b(\w+)::~?\w+\s*\(', text)}
        # Every field of every class this file implements, plus locals with a
        # spelled-out type: `AskSession* ask = service_->ask();`
        typed = {}
        for cls in own:
            typed.update(fields.get(cls, {}))
        for m in re.finditer(r'\b([A-Z]\w+)\s*\*\s*(\w+)\s*=', text):
            typed[m.group(2)] = m.group(1)
        for m in re.finditer(r'\b(\w+)\s*->\s*(\w+)\s*\(', text):
            recv, name = m.group(1), m.group(2)
            cls = typed.get(recv)
            if cls is None or cls not in members or name not in members[cls]:
                continue
            if members[cls][name] == 'public' or cls in own:
                continue
            if friends.get(cls, set()) & own:
                continue
            checked_private += 1
            line = text[:m.start()].count('\n') + 1
            findings.append(
                f'{c}:{line}: {name}() is {members[cls][name]} in {cls} and '
                f'called from outside it')

    if findings:
        print('check-cpp-visibility: names that will not resolve where they '
              'are used.\n')
        for f in sorted(set(findings)):
            print(f'  {f}')
        print(f'\n{len(set(findings))} finding(s)')
        return 1
    print(f'C++ visibility OK ({checked_defs} out-of-line definitions, '
          f'{checked_free} file-local functions, '
          f'{checked_private} private calls judged)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
