#!/usr/bin/env python3
"""Confirm the mojom TypeScript stub agrees with Chromium's own ts_typemaps.

A mojom struct can be typemapped to a plain TypeScript type. url/mojom/BUILD.gn
does exactly this:

    ts_typemaps = [ { types = [ { mojom = "url.mojom.Url", ts = "string" } ] } ]

so a `url.mojom.Url` field arrives in the generated bindings as a bare string,
not as `{url: string}`. The hand-kept stub declared the object form. That
type-checks perfectly against itself - tsc has no idea the stub is fiction -
and failed the real build at target 114 of 1532 with

    error TS2339: Property 'url' does not exist on type 'string'.

This is the failure check-webui.sh structurally cannot catch. It compiles the
console against the stub, so a wrong stub is a self-consistent world and every
line that agrees with it passes. The only way out is to check the stub against
something outside itself, which is what this does: read Chromium's real typemap
declarations at the pinned tag and assert the stub does not contradict them.

If the fetch fails this exits 0 and says so. That means the stub is UNVERIFIED
against the typemaps - say that rather than claiming it agrees with them.
"""
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
STUB = ROOT / 'tools' / 'webui-typecheck' / 'stubs' / 'flux.mojom-webui.d.ts'
MOJOM = ROOT / 'src' / 'browser' / 'mojom' / 'flux.mojom'


def gn_block(text: str, name: str) -> str | None:
    """The body of `name = [ ... ]`, matched by bracket depth.

    Anchoring the end on a column-zero `]` reads correctly and is wrong: GN
    indents the closing bracket of a nested list, so the block ran to the end of
    the file and swallowed the cpp_typemaps below it. Nothing broke, because
    those entries have no `ts =` line - which is exactly how a check rots. Count
    the brackets instead.
    """
    start = re.search(rf'^\s*{re.escape(name)}\s*=\s*\[', text, re.M)
    if not start:
        return None
    depth, i = 0, start.end() - 1
    while i < len(text):
        if text[i] == '[':
            depth += 1
        elif text[i] == ']':
            depth -= 1
            if depth == 0:
                return text[start.end():i]
        i += 1
    return None


def typemapped(text: str) -> list[tuple[str, str]]:
    """Every (mojom qualified name, ts type) pair inside ts_typemaps."""
    block = gn_block(text, 'ts_typemaps')
    if block is None:
        return []
    out = []
    # Each `{ ... }` inside `types = [...]` is one mapping. Parsed as a unit so
    # a mapping missing its `ts` is an error rather than a silent drop.
    for entry in re.findall(r'\{([^{}]*)\}', block):
        mojom = re.search(r'\bmojom\s*=\s*"([^"]+)"', entry)
        if not mojom:
            continue
        ts = re.search(r'\bts\s*=\s*"([^"]+)"', entry)
        if not ts:
            print(f'  ts_typemaps entry for {mojom.group(1)} has no `ts =`; '
                  f'this parser does not understand it', file=sys.stderr)
            out.append((mojom.group(1), None))
            continue
        out.append((mojom.group(1), ts.group(1)))
    return out


def main() -> int:
    if not STUB.exists():
        print(f'No stub at {STUB}', file=sys.stderr)
        return 1

    version = ''
    for line in (ROOT / 'chromium.version').read_text().splitlines():
        if line.startswith('CHROMIUM_VERSION='):
            version = line.split('=', 1)[1].strip()
    if not version:
        print('No CHROMIUM_VERSION in chromium.version', file=sys.stderr)
        return 1

    base = f'https://raw.githubusercontent.com/chromium/chromium/{version}'
    cache = pathlib.Path(os.environ.get('TMPDIR', '/tmp')) / f'flux-tstypemap-{version}'
    cache.mkdir(parents=True, exist_ok=True)

    # Every mojom flux.mojom imports from, so this grows with the mojom rather
    # than being a list of the one thing that happened to bite once.
    dirs = sorted({
        m.rsplit('/', 1)[0]
        for m in re.findall(r'^import "([a-z0-9_/]+/[a-z0-9_]+\.mojom)"',
                            MOJOM.read_text(), re.M)
    })

    stub = STUB.read_text()
    stub_lines = stub.splitlines()
    status, checked, fetched = 0, 0, 0

    for d in dirs:
        cached = cache / (d.replace('/', '_') + '.gn')
        if not cached.exists():
            try:
                subprocess.run(
                    ['curl', '-sfS', '--max-time', '20', '-o', str(cached),
                     f'{base}/{d}/BUILD.gn'], check=True, capture_output=True)
            except (subprocess.CalledProcessError, FileNotFoundError):
                cached.unlink(missing_ok=True)
                continue
        fetched += 1

        for qualified, ts_type in typemapped(cached.read_text()):
            if ts_type is None:
                status = 1
                continue
            short = qualified.rsplit('.', 1)[-1]
            checked += 1

            decl = re.compile(rf'^(export )?(interface|type|class) {short}\b')
            use = re.compile(rf'(:\s*|<\s*|\|\s*){short}\b')
            for n, line in enumerate(stub_lines, 1):
                if decl.match(line):
                    print(f'{STUB.name}:{n} declares "{short}", but '
                          f'{d}/BUILD.gn typemaps', file=sys.stderr)
                    print(f'  {qualified} to TypeScript "{ts_type}". The '
                          f'generated bindings have no such type;', file=sys.stderr)
                    print(f'  a field of it is a plain {ts_type}.', file=sys.stderr)
                    status = 1
                elif use.search(line):
                    print(f'{STUB.name}:{n} uses "{short}" as a type: {line.strip()}',
                          file=sys.stderr)
                    print(f'  {qualified} is typemapped to "{ts_type}" in '
                          f'{d}/BUILD.gn;', file=sys.stderr)
                    print(f'  write {ts_type} (or {ts_type}|null).', file=sys.stderr)
                    status = 1

    if fetched == 0:
        print(f'Could not fetch any BUILD.gn at {version} - the stub is '
              f'UNVERIFIED')
        print('against Chromium\'s ts_typemaps. Do not claim it agrees.')
        return 0

    if status == 0:
        print(f'TS typemaps OK ({checked} checked against {version})')
    return status


if __name__ == '__main__':
    sys.exit(main())
