#!/usr/bin/env python3
"""A pref that nothing outside the console ever reads.

Every pref here exists so that something *behaves* differently: the agent
reads the user's standing instructions, the scheduler reads the saved
workflows, the sidebar view reads its own collapsed state. A pref the console
writes and only the console reads back - to redraw the screen that wrote it -
is a round trip no other part of the browser observes. The screen works, the
value survives a restart, and nothing happens.

That is not hypothetical. Three prefs were in exactly that state at once:

  kInstructions  Customize > Instructions saved to disk and reached no model,
                 while flux_prefs.h described it as "Prepended to every task".
  kLearnedFacts  the "remember" tool wrote a diary the next run could not open.
  kUserSkills    read by nothing at all, so adopting one of the 138 shipped
                 skills changed no behaviour.

All three compile, link, run and show a working screen. This is the pref-shaped
version of what check-never-assigned.py finds in C++ fields, and it needs its
own check for the same reason: the declaration reads like proof the feature is
wired up.

Registration in flux_prefs.cc is not a read, and a write is not a read. What
counts is a Get* somewhere outside src/browser/webui - the layer that draws the
screens.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BROWSER = ROOT / 'src' / 'browser'
CONSOLE = BROWSER / 'webui'

READ = re.compile(r'->\s*Get(?:String|List|Dict|Boolean|Integer|Double|Value)'
                  r'\s*\(\s*(?:flux::)?prefs::(\w+)')

# Every mention of a pref symbol, so the ones that are neither a Get* nor a
# Set* here can be recognised as having been handed to something else.
ANY = re.compile(r'(?:flux::)?prefs::(k\w+)')
WRITE = re.compile(
    r'->\s*Set(?:String|List|Dict|Boolean|Integer|Double|Value)\s*\(\s*'
    r'(?:flux::)?prefs::(\w+)'
    r'|Scoped(?:List|Dict)PrefUpdate\s+\w+\s*\([^,]+,\s*(?:flux::)?prefs::(\w+)')


def main() -> int:
    header = (BROWSER / 'flux_prefs.h').read_text(encoding='utf-8')
    prefs = re.findall(r'inline constexpr char (k\w+)\[\]', header)

    reads: dict[str, set[pathlib.Path]] = {p: set() for p in prefs}
    writes: dict[str, set[pathlib.Path]] = {p: set() for p in prefs}
    escaped: set[str] = set()

    for cc in sorted(BROWSER.rglob('*.cc')):
        # flux_prefs.cc only registers them; registering is not using.
        if cc.name == 'flux_prefs.cc':
            continue
        body = cc.read_text(encoding='utf-8')
        for match in READ.finditer(body):
            if match.group(1) in reads:
                reads[match.group(1)].add(cc)
        for match in WRITE.finditer(body):
            name = match.group(1) or match.group(2)
            if name in writes:
                writes[name].add(cc)

        # A pref name passed somewhere - SecretStore(profile, prefs::kApiKeys)
        # - is read through a member, not through a literal, and no scan of
        # this kind can follow it. Saying nothing is the only honest answer:
        # the first version of this check called all three SecretStore prefs
        # unread, which is the failure mode that makes a report unreadable.
        accounted = {(m.group(1) or m.group(2)) for m in WRITE.finditer(body)}
        accounted |= {m.group(1) for m in READ.finditer(body)}
        for match in ANY.finditer(body):
            name = match.group(1)
            if name in reads and name not in accounted:
                escaped.add(name)

    problems = []
    for pref in prefs:
        if pref in escaped:
            continue
        outside = {f for f in reads[pref] if CONSOLE not in f.parents}
        if not reads[pref]:
            problems.append(
                f'{pref}: written but never read. Whatever it configures does '
                f'not consult it.')
        elif not outside and not {f for f in writes[pref]
                                  if CONSOLE not in f.parents}:
            # Console-only in BOTH directions. If something outside writes it -
            # the agent saving a template the Templates screen then lists -
            # then the console reading it back is a real flow, and reporting it
            # would be a false positive with a confidently wrong explanation
            # attached ("the screen that writes it"), which is worse than
            # saying nothing.
            where = ', '.join(sorted(f.name for f in reads[pref]))
            problems.append(
                f'{pref}: written and read only by the console ({where}). '
                f'Nothing outside the screens that edit it ever consults it, '
                f'so setting it changes no behaviour anywhere.')
        elif not writes[pref]:
            problems.append(
                f'{pref}: read but never written. It can only ever hold its '
                f'registered default.')

    if problems:
        for problem in problems:
            print(problem)
        print(f'\n{len(problems)} pref(s) wired to nothing.')
        return 1

    print(f'pref flow OK ({len(prefs)} prefs, {len(escaped)} passed elsewhere '
          f'and not judged, the rest read outside the console)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
