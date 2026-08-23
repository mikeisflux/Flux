#!/usr/bin/env python3
"""Calls to this project's own methods with the wrong number of arguments.

There is no compiler in this container, so an arity mismatch reaches the user's
machine and costs them a build. One did: RecordAction grew from two parameters
to four and one of its three call sites - inside ResolveApproval, the declined
branch - was left at two. It reads perfectly, every check here passed, and it
would have failed the build twenty minutes in.

Deliberately narrow. It only looks at methods declared in this project's own
headers, with unique names, whose declaration and call sites it can parse
without ambiguity. Overloads, templates, macros and anything it cannot count
confidently are skipped, because a false positive on a real build is worse
than a miss - this exists to catch the refactor that forgot a call site, not
to be a type checker.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path('src/browser')

# name -> (min_args, max_args, where) for methods declared exactly once.
decls = {}
ambiguous = set()

DECL = re.compile(
    r'^\s{2,}(?:virtual\s+|static\s+|explicit\s+)*'      # leading specifiers
    r'(?:[\w:]+(?:<[^;()]*>)?[\s&*]+)'                   # return type
    r'(\w+)\('                                           # name
    r'(.*)\)\s*(?:const\s*)?(?:override\s*)?'                # `.*` not `[^;{]*`: a `= {}` default argument
                                                     # contains a brace, and refusing it
                                                     # dropped the declaration entirely.
    r'(?:=\s*0\s*)?[;{]')


def count_params(text):
    """(min, max) arguments, honouring defaults. None if not countable."""
    text = text.strip()
    if not text or text == 'void':
        return (0, 0)
    if '...' in text:
        return None
    depth, cur, parts = 0, '', []
    for ch in text:
        if ch in '<([{':
            depth += 1
        elif ch in '>)]}':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += ch
    parts.append(cur)
    required = sum(1 for p in parts if '=' not in p)
    return (required, len(parts))


for header in sorted(ROOT.rglob('*.h')):
    lines = header.read_text(encoding='utf-8').splitlines()
    for idx, raw in enumerate(lines):
        if raw.lstrip().startswith('//'):
            continue
        # Join continuation lines before matching. A declaration whose
        # parameter list wraps - which is every method with more than two or
        # three parameters, i.e. exactly the ones that grow one and break a
        # call site - was invisible to a line-at-a-time match. RecordAction,
        # the method this check exists for, was among them.
        joined, k = raw, idx
        while (joined.count('(') > joined.count(')') and k + 1 < len(lines)
               and k - idx < 8):
            k += 1
            joined = joined.rstrip() + ' ' + lines[k].strip()
        m = DECL.match(joined)
        if not m:
            continue
        name, params = m.group(1), m.group(2)
        if name in ('if', 'for', 'while', 'switch', 'return', 'sizeof'):
            continue
        counted = count_params(params)
        if counted is None:
            ambiguous.add(name)
            continue
        if name in decls and decls[name][:2] != counted:
            ambiguous.add(name)          # overload
        decls[name] = (*counted, f'{header}')

for name in ambiguous:
    decls.pop(name, None)

# `~` in the lookbehind: PageContext::~PageContext() is a destructor
# definition, not a zero-argument call to a one-argument constructor.
# Without it every class in the tree reported a mismatch.
# A definition in a .cc always writes `Class::name(`, and `::` is already in
# the lookbehind, so no declaration-guard is needed here. The first version had
# one, and it skipped any statement ending in `;` - which is every single-line
# call, including the exact one this check exists to catch.
CALL = re.compile(r'(?<![\w:>.~])(\w+)\(')


def split_args(text, start):
    """Top-level argument list beginning at `text[start]` == '('.

    Returns (count, end_index) or (None, None) if it does not close. Tracks
    (), [], {} and skips string and char literals - the first version counted
    commas without doing either, and read a four-argument call containing a
    base::StrCat({...}) and a nested ternary as two.
    """
    assert text[start] == '('
    depth, i, n = 0, start, len(text)
    args, cur = [], ''
    while i < n:
        ch = text[i]
        if ch in '"\'':
            quote, i = ch, i + 1
            cur += quote
            while i < n:
                if text[i] == '\\':
                    cur += text[i:i + 2]
                    i += 2
                    continue
                cur += text[i]
                if text[i] == quote:
                    break
                i += 1
            i += 1
            continue
        if ch in '([{':
            depth += 1
            if depth == 1 and ch == '(':
                i += 1
                continue
        elif ch in ')]}':
            depth -= 1
            if depth == 0:
                args.append(cur)
                joined = [a for a in args if a.strip()]
                return len(joined), i
        if ch == ',' and depth == 1:
            args.append(cur)
            cur = ''
        else:
            cur += ch
        i += 1
    return None, None


def _self_test():
    """The check must be able to count these before it is allowed to run.

    Every one of them is a shape that broke an earlier version.
    """
    cases = [
        ('f()', 0),
        ('f(a)', 1),
        ('f(a, b)', 2),
        ('f(a, g(b, c))', 2),
        ('f(base::StrCat({"x, y", z}), t)', 2),
        ('f(a, c ? d(e) : g, h, i)', 4),
        ('f(a, "text with, comma")', 2),
        ("f(a, ',')", 2),
        ('f(std::vector<int>{1, 2}, b)', 2),
    ]
    for text, want in cases:
        got, _ = split_args(text, text.index('('))
        if got != want:
            print(f'check-arity self-test FAILED: {text} counted {got}, '
                  f'expected {want}', file=sys.stderr)
            sys.exit(2)


_self_test()

bad = 0
for path in sorted(ROOT.rglob('*.cc')):
    text = path.read_text(encoding='utf-8')
    # One flat buffer, so a call wrapped over lines is counted as one call.
    for m in CALL.finditer(text):
        name = m.group(1)
        if name not in decls:
            continue
        line_start = text.rfind('\n', 0, m.start()) + 1
        before = text[line_start:m.start()]
        if before.lstrip().startswith('//'):
            continue
        # `ScopedDictPrefUpdate skills(prefs, key);` is a variable declaration
        # with constructor arguments, not a call to something called skills().
        # An identifier immediately before the name means a type precedes it.
        if re.search(r'[\w>\]]\s+$', before):
            continue
        given, _ = split_args(text, m.end() - 1)
        if given is None:
            continue
        lo, hi, where = decls[name]
        if not (lo <= given <= hi):
            lineno = text.count('\n', 0, m.start()) + 1
            snippet = text[line_start:text.find('\n', m.start())].strip()
            print(f'{path}:{lineno}: {name}() called with {given} '
                  f'argument(s); declared taking '
                  f'{lo if lo == hi else f"{lo}-{hi}"} in {where}')
            print(f'    {snippet}')
            bad += 1

if bad:
    print(f'\n{bad} arity mismatch(es)')
    sys.exit(1)
print(f'Arity OK ({len(decls)} uniquely-named methods checked)')
