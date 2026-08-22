#!/usr/bin/env python3
"""Pack data/skills/*.md into the resource the console and the agent both read.

The markdown files are the authoring source - frontmatter plus a freeform body,
which is the shape docs/06 settles on. Neither the WebUI nor the browser process
can read a directory of them at runtime, though: the console is sandboxed and
the browser process has no path that survives installation. So they are packed
into one JSON resource.

check-webui.sh regenerates and diffs, so the packed copy cannot drift from the
markdown. Run with --write to update it.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / 'data' / 'skills'
OUT = ROOT / 'src' / 'resources' / 'skills.json'


def parse_scalar(value):
    value = value.strip()
    if value.startswith('[') and value.endswith(']'):
        inner = value[1:-1].strip()
        return [v.strip() for v in inner.split(',') if v.strip()] if inner else []
    return value


def parse_frontmatter(text):
    """The frontmatter is a fixed, tiny subset of YAML, so it is parsed rather
    than pulling in a dependency: scalars, inline lists, and one nested list of
    `- id: / transport:` pairs under worksWith."""
    fields = {}
    key = None
    for raw in text.splitlines():
        if not raw.strip():
            continue
        if raw.startswith('  - ') or raw.startswith('    '):
            item = raw.strip().lstrip('- ')
            if ':' not in item:
                if isinstance(fields.get(key), list):
                    fields[key].append(item)
                continue
            k, v = (p.strip() for p in item.split(':', 1))
            if raw.startswith('  - '):
                fields.setdefault(key, []).append({k: v})
            elif fields.get(key):
                fields[key][-1][k] = v
            continue
        m = re.match(r'^([A-Za-z_]+):\s*(.*)$', raw)
        if not m:
            continue
        key, value = m.group(1), m.group(2)
        # A comment after a scalar is authoring commentary, not data.
        value = re.sub(r'\s+#.*$', '', value)
        fields[key] = parse_scalar(value) if value else []
    return fields


def when_to_use(body):
    """The one required section - it is the retrieval trigger that makes
    "used automatically when relevant" work at all."""
    m = re.search(r'^##\s+When to use\s*$(.*?)(?=^##\s|\Z)', body,
                  re.M | re.S)
    return m.group(1).strip() if m else ''


def build():
    skills = []
    for path in sorted(SRC.glob('*.md')):
        text = path.read_text(encoding='utf-8')
        if not text.startswith('---'):
            raise SystemExit(f'{path.name}: no frontmatter')
        _, front, body = text.split('---', 2)
        f = parse_frontmatter(front)
        body = body.strip()

        missing = [k for k in ('name', 'command', 'description', 'categories',
                               'roles', 'writeScope') if k not in f]
        if missing:
            raise SystemExit(f'{path.name}: missing {", ".join(missing)}')
        trigger = when_to_use(body)
        if not trigger:
            raise SystemExit(f'{path.name}: no "## When to use" section')

        skills.append({
            'command': f['command'],
            'name': f['name'],
            'description': f['description'],
            'categories': f['categories'],
            'roles': f['roles'],
            'worksWith': f.get('worksWith', []),
            'writeScope': f['writeScope'],
            'bodyStatus': f.get('body_status', 'authored'),
            'whenToUse': trigger,
            'body': body,
        })

    commands = [s['command'] for s in skills]
    dupes = {c for c in commands if commands.count(c) > 1}
    if dupes:
        raise SystemExit(f'duplicate commands: {sorted(dupes)}')

    return {
        'version': 1,
        'note': ('Generated from data/skills/*.md by tools/build-skills-json.py. '
                 'Edit the markdown, not this file.'),
        'skills': skills,
    }


def main():
    packed = json.dumps(build(), indent=2, ensure_ascii=False) + '\n'
    if '--write' in sys.argv:
        OUT.write_text(packed, encoding='utf-8')
        print(f'wrote {OUT.relative_to(ROOT)} ({len(build()["skills"])} skills)')
        return 0
    if not OUT.exists() or OUT.read_text(encoding='utf-8') != packed:
        print(f'{OUT.relative_to(ROOT)} is stale - run '
              'tools/build-skills-json.py --write', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
