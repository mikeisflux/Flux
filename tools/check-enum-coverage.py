#!/usr/bin/env python3
"""Enum-keyed lookup tables in the console that do not cover every value.

Adding a value to a mojom enum silently breaks every table keyed on it, in
every file, and the miss only shows for the state nobody tested - which is
usually the failure or waiting state. Adding kAwaitingInput broke the
sidebar's STATE_NAME and nothing else noticed.

A missing key yields undefined. With `?? 'fallback'` that is survivable; without
one it renders the string "undefined" or drops the element - and it only shows
up for the state nobody tested, which is usually the failure state.
"""
import pathlib
import re
import sys

mojom = pathlib.Path('src/browser/mojom/flux.mojom').read_text()
enums = {}
for name, body in re.findall(r'^enum (\w+) \{(.*?)\n\};', mojom, re.S | re.M):
    enums[name] = [v for v in re.findall(r'^\s+(k\w+)', body, re.M)]

bad = 0
for p in sorted(pathlib.Path('src/resources').glob('*.ts')):
    text = p.read_text()
    # const X: Record<number, ...> = { [Enum.kA]: ..., ... }
    for m in re.finditer(
            r'(const (\w+)[^=]*=\s*\{)([^}]*)\}', text, re.S):
        body = m.group(3)
        used = re.findall(r'\[(\w+)\.(k\w+)\]', body)
        if not used:
            continue
        enum_name = used[0][0]
        if enum_name not in enums:
            continue
        covered = {v for _, v in used}
        missing = [v for v in enums[enum_name] if v not in covered]
        if missing:
            line = text[:m.start()].count('\n') + 1
            print(f'{p}:{line}: {m.group(2)} covers '
                  f'{len(covered)}/{len(enums[enum_name])} of {enum_name}, '
                  f'missing {", ".join(missing)}')
            bad += 1

    # switch (x) over an enum
    for m in re.finditer(r'switch\s*\([^)]*\)\s*\{(.*?)\n\s*\}', text, re.S):
        cases = re.findall(r'case (\w+)\.(k\w+)', m.group(1))
        if not cases:
            continue
        enum_name = cases[0][0]
        if enum_name not in enums:
            continue
        if 'default:' in m.group(1):
            continue
        covered = {v for _, v in cases}
        missing = [v for v in enums[enum_name] if v not in covered]
        if missing:
            line = text[:m.start()].count('\n') + 1
            print(f'{p}:{line}: switch over {enum_name} has no default and '
                  f'misses {", ".join(missing)}')
            bad += 1

if bad:
    print(f'\n{bad} incomplete table(s)')
    sys.exit(1)
print(f'Enum tables OK ({len(enums)} mojom enums)')
