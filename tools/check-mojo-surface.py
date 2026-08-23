#!/usr/bin/env python3
"""Cross-check flux.mojom against the browser, the TS stub and the console.

Three ways this contract breaks, none of which produces an error the user
would recognise as one - they all look like a screen that does nothing:

  - a mojom method with no C++ implementation
  - an observer callback the browser never fires (a screen that never updates;
    OnLearnedFact was one, so the Customize list was permanently empty)
  - a method missing from the hand-kept TS stub, which fails the real build

Those three fail. A method the console never calls is only reported, because
dead surface is a decision rather than a defect - though it is usually a
feature nobody finished wiring up.
"""
import pathlib
import re
import sys

MOJOM = pathlib.Path('src/browser/mojom/flux.mojom')
HANDLER_CC = pathlib.Path('src/browser/webui/flux_page_handler.cc')
HANDLER_H = pathlib.Path('src/browser/webui/flux_page_handler.h')
STUB = pathlib.Path('tools/webui-typecheck/stubs/flux.mojom-webui.d.ts')

mojom = MOJOM.read_text(encoding='utf-8')


def interface(name):
    m = re.search(rf'interface {name} \{{(.*?)\n\}};', mojom, re.S)
    return re.findall(r'^\s{2}(\w+)\(', m.group(1), re.M) if m else []


def lower_camel(s):
    return s[0].lower() + s[1:]


hcc = HANDLER_CC.read_text(encoding='utf-8')
hh = HANDLER_H.read_text(encoding='utf-8')
stub = STUB.read_text(encoding='utf-8')
ts = '\n'.join(p.read_text(encoding='utf-8')
               for p in sorted(pathlib.Path('src/resources').glob('*.ts')))
# The page handler alone, not the whole tree. The observer callback has to
# reach the CONSOLE, which only happens through `observer_->X(...)` here.
# Searching every .cc instead matched FluxAgentService's own C++
# Observer::OnLearnedFact and reported the callback as fired while the mojo
# forward was missing - the check passed with the bug in place, which is the
# one thing a check must never do.

fatal, notes = [], []

for m in interface('FluxPageHandler'):
    if f'FluxPageHandler::{m}(' not in hcc:
        fatal.append(f'{m}: declared in flux.mojom, no implementation in '
                     f'{HANDLER_CC.name}')
    if not re.search(rf'\b{m}\(', hh):
        fatal.append(f'{m}: not declared in {HANDLER_H.name}')
    if not re.search(rf'^\s+{lower_camel(m)}\(', stub, re.M):
        fatal.append(f'{m}: missing from the TS stub - the real build will '
                     f'reject any call to it')
    if not re.search(rf'\.{lower_camel(m)}\(', ts):
        notes.append(f'{m}: nothing in the console calls it')

for m in interface('FluxPageHandlerObserver'):
    if not re.search(rf'observer_->{m}\(', hcc):
        fatal.append(f'{m}: the browser never fires it, so whatever the '
                     f'console draws from it never updates')
    if not re.search(rf'^\s+{lower_camel(m)}\(', ts, re.M):
        notes.append(f'{m}: the console does not implement it')

for line in fatal:
    print(f'flux.mojom: {line}')
for line in notes:
    print(f'note: {line}')

if fatal:
    sys.exit(1)
print(f'Mojo surface OK ({len(interface("FluxPageHandler"))} methods, '
      f'{len(interface("FluxPageHandlerObserver"))} callbacks'
      f'{f", {len(notes)} unused" if notes else ""})')
