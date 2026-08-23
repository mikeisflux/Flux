#!/usr/bin/env python3
"""Rename Chromium to Flux in the unbranded UI strings.

Chromium keeps the product name for an unbranded build as a LITERAL in
chrome/app/chromium_strings.grd - 742 of them - rather than as a placeholder
that IDS_PRODUCT_NAME could fill. Patch 0009 renames the two that define
IDS_PRODUCT_NAME itself, which is what the window title and the about page
read. The other 740 are strings like

    Continue where you left off: Chromium restores your tabs every time you
    restart. To turn this off, go to Settings.

and each one is a place the browser tells the user it is Chromium.

A step here rather than a patch, deliberately. A 742-hunk patch against a
string file would conflict on essentially every uprev - Chromium edits these
constantly - and rebasing it would be the single most expensive thing in the
series. This is mechanical and derives its result from whatever the tree
currently says, so an uprev costs nothing. It runs after the patch series for
the same reason the icon copy does: the tree has to be pristine first.

Two things are protected:

  - "The Chromium Authors" is a copyright attribution, not a product name.
    Renaming it would be false, and it is in a string the about page shows.
  - Anything inside a URL. There are none with a capital C today, but a
    renamed host in a support link is a broken link, and the guard costs
    nothing.

Case-sensitive on purpose: chromium.org, chrome:// and _google_chrome are
lowercase and must not move.
"""

import pathlib
import re
import sys

TARGETS = [
    'chrome/app/chromium_strings.grd',
    'chrome/app/settings_chromium_strings.grdp',
]

# Spans that keep the word Chromium no matter what.
PROTECTED = [
    re.compile(r'The Chromium Authors'),
    re.compile(r'https?://[^\s"<>]*'),
]

WORD = re.compile(r'\bChromium\b')


def rebrand(text: str) -> tuple[str, int]:
    """Replaces Chromium with Flux outside the protected spans."""
    holes: list[str] = []

    def stash(match: re.Match[str]) -> str:
        holes.append(match.group(0))
        # \x00 cannot occur in the source, so the placeholder cannot collide
        # with anything real, and it carries no word characters that \b could
        # catch on.
        return f'\x00{len(holes) - 1}\x00'

    for pattern in PROTECTED:
        text = pattern.sub(stash, text)

    text, count = WORD.subn('Flux', text)

    for index, original in enumerate(holes):
        text = text.replace(f'\x00{index}\x00', original)
    return text, count


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print('usage: rebrand-strings.py <chromium src root>', file=sys.stderr)
        return 2
    root = pathlib.Path(argv[1])

    total = 0
    for relative in TARGETS:
        path = root / relative
        if not path.is_file():
            print(f'    MISSING  {relative}', file=sys.stderr)
            return 1

        # newline='' so the file's own line endings survive the round trip -
        # Chromium's tree is LF, and rewriting a string file as CRLF would put
        # a whole-file diff into every later patch attempt.
        with open(path, encoding='utf-8', newline='') as handle:
            original = handle.read()
        updated, count = rebrand(original)
        if updated != original:
            with open(path, 'w', encoding='utf-8', newline='') as handle:
                handle.write(updated)
        total += count
        print(f'    rebranded  {relative} ({count})')

    if total == 0:
        print('    nothing to rebrand - already applied, or the strings moved',
              file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
