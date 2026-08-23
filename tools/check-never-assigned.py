#!/usr/bin/env python3
"""Pointer-ish member fields that are never populated.

AgentRunner declared

    std::unique_ptr<PageContext> page_;
    raw_ptr<content::WebContents> web_contents_ = nullptr;

handed both to every tool on every call, and assigned neither, anywhere. The
agent had no browsing context at all: a run started, talked to the model, and
the first read_page had nothing to read. It compiles, it links, it runs, and
the feature simply does not exist. Nothing in a review catches this - the
declaration is right there and looks like proof.

Restricted to raw_ptr, unique_ptr, optional, WeakPtr and bare pointers,
because for those "never assigned" means "always null" and every read is a
silent no-op. Containers are excluded: they are filled through method calls,
not assignment, and flagging them is pure noise.

An `= nullptr` initializer does NOT count as an assignment. That is exactly
what web_contents_ had, and counting it is how the first version of this check
missed the bug it was written for.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'src/browser')
PTR = re.compile(r'^(raw_ptr|std::unique_ptr|std::optional|base::WeakPtr)\s*<|\*$')

decls = {}
for header in sorted(ROOT.rglob('*.h')):
    for i, line in enumerate(header.read_text(encoding='utf-8').splitlines(), 1):
        if line.strip().startswith('//'):
            continue
        m = re.match(
            r'^\s{2,}((?:const\s+)?[\w:]+\s*(?:<.*>)?\s*\*?)\s+(\w+_)\s*'
            r'(?:=\s*[^;]*)?;\s*$', line)
        if not m:
            continue
        if not PTR.search(m.group(1).strip()):
            continue
        decls[m.group(2)] = (header, i, line.strip())

decl_lines = {(h, i) for h, i, _ in decls.values()}
assigned = set()
for path in list(ROOT.rglob('*.cc')) + list(ROOT.rglob('*.h')):
    text = '\n'.join(
        line for n, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1)
        if (path, n) not in decl_lines)
    for name in decls:
        if (re.search(rf'(?<![\w.>-]){name}\s*=(?!=)', text)
                or re.search(rf'(?<![\w.>-]){name}\.reset\(', text)
                or re.search(rf'(?<![\w.>-]){name}\.emplace\(', text)
                or re.search(rf'[:,]\s*{name}\(', text)):
            assigned.add(name)

bad = [(h, i, line) for name, (h, i, line) in sorted(decls.items())
       if name not in assigned]
for h, i, line in bad:
    print(f'{h}:{i}: {line}')
    print('    never assigned anywhere - it is always null, and every read of '
          'it is a silent no-op')

if bad:
    sys.exit(1)
print(f'Fields OK ({len(decls)} pointer fields, all assigned)')
