#!/usr/bin/env python3
"""Merge authored prompts and roles into src/resources/templates.json.

The catalogue shipped with a title, an outcome and a list of services, which
is enough for a card and not enough for anything else. The prompt is what the
agent is actually told, and it is the only part of a template a person can
read to decide whether to trust it with their inbox - so it is authored, one
at a time, rather than generated from the title.

Run with a JSON file of {id: {"prompt": ..., "roles": [...]}} on stdin:

    python3 tools/add-template-prompts.py < batch.json
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CATALOG = ROOT / 'src' / 'resources' / 'templates.json'


def main() -> int:
    batch = json.load(sys.stdin)
    data = json.loads(CATALOG.read_text(encoding='utf-8'))

    by_id = {t['id']: t for t in data['templates']}
    unknown = [tid for tid in batch if tid not in by_id]
    if unknown:
        print(f'unknown template ids: {", ".join(unknown)}', file=sys.stderr)
        return 1

    for tid, fields in batch.items():
        template = by_id[tid]
        if 'prompt' in fields:
            template['prompt'] = fields['prompt']
        if 'roles' in fields:
            template['roles'] = fields['roles']

    CATALOG.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

    done = sum(1 for t in data['templates'] if t.get('prompt'))
    print(f'{len(batch)} merged; {done}/{len(data["templates"])} have a prompt')
    return 0


if __name__ == '__main__':
    sys.exit(main())
