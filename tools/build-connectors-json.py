#!/usr/bin/env python3
"""Pack data/connectors/*.json into the resource the browser process reads.

The per-connector files are the authoring source: they carry prose notes,
provenance, and the gotchas that took a day each to find. None of that can be
read at runtime - the browser process has no path to this repo after the
installer runs - so the parts a request actually needs are packed into one
resource that goes into flux_resources.pak.

What is packed is deliberately narrower than what is authored:

  kept     id, auth, api, operations, gotchas, rate_limits.verbatim-ish
  dropped  source (provenance is for review, not for runtime), pending_
           operations, not_implemented, and the other per-file scratch keys

`gotchas` is kept because it is the one prose field the runtime has a use for:
the execution layer surfaces it to the agent before it touches a connector for
the first time, which is cheaper than the agent rediscovering that a Basecamp
message posts as a draft.

check-webui.sh regenerates and diffs, so the packed copy cannot drift from the
authored files. Run with --write to update it.
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / 'data' / 'connectors'
OUT = ROOT / 'src' / 'resources' / 'connector_defs.json'

# Everything the runtime needs to build and authorize a request, and nothing
# that only a reviewer needs. Order is the emitted key order, so it is stable.
RUNTIME_KEYS = ('id', 'auth', 'api', 'operations', 'gotchas', 'rate_limits')

# Prose that exists to explain a decision to a human reading the file. Packing
# it would roughly double the resource for no runtime gain.
AUTH_DROP = ('notes',)


def pack_auth(auth):
    """Keep the fields an OAuth exchange or a header actually uses."""
    out = {k: v for k, v in auth.items() if k not in AUTH_DROP}
    # personal_token.notes is guidance for the settings screen, which does
    # render it, so unlike auth.notes it stays.
    return out


def build():
    connectors = []
    for path in sorted(SRC.glob('*.json')):
        d = json.loads(path.read_text(encoding='utf-8'))
        packed = {}
        for key in RUNTIME_KEYS:
            if key not in d:
                continue
            packed[key] = pack_auth(d[key]) if key == 'auth' else d[key]
        if 'id' not in packed:
            raise SystemExit(f'{path.name}: no id')
        connectors.append(packed)

    return {
        'version': 1,
        'note': ('Generated from data/connectors/*.json by '
                 'tools/build-connectors-json.py. Edit those, not this file.'),
        'connectors': connectors,
    }


def main():
    packed = json.dumps(build(), indent=2, ensure_ascii=False) + '\n'
    if '--write' in sys.argv:
        OUT.write_text(packed, encoding='utf-8')
        n = len(build()['connectors'])
        print(f'wrote {OUT.relative_to(ROOT)} ({n} connectors, '
              f'{len(packed) // 1024} KB)')
        return 0
    if not OUT.exists() or OUT.read_text(encoding='utf-8') != packed:
        print(f'{OUT.relative_to(ROOT)} is stale - run '
              'tools/build-connectors-json.py --write', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
