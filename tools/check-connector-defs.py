#!/usr/bin/env python3
"""Connector definitions the client cannot actually execute.

Every failure here happens on the user's machine, against a live API, at the
moment the agent tries to use the connector - which is the most expensive
place for it to surface and the hardest to attribute.

Two things go wrong, and both did:

  facebook_pages declared five operations with paths relative to a base URL it
  did not have. ConnectorClient concatenates an empty base with "/{page_id}/feed"
  and hands GURL a scheme-less string, so all five failed before a request was
  made. Nothing in the definition looks wrong - the `api` block is simply
  absent, and absence is not something reading notices.

  plain is GraphQL: one endpoint, one method, and the operation is a named
  mutation rather than a path. Its operations parse with an empty method and
  an empty path, because the parser only knows `method` and `path`. The
  connector reported itself connectable and could not make a single call.

MCP connectors are exempt: their operations name a `tool`, and the MCP server
supplies the transport.
"""
import json
import pathlib
import sys

issues = []
defs = sorted(pathlib.Path('data/connectors').glob('*.json'))

for f in defs:
    d = json.loads(f.read_text(encoding='utf-8'))
    cid = d.get('id', f.stem)
    kind = (d.get('auth') or {}).get('type')
    if kind not in ('oauth2', 'api_key'):
        continue

    ops = d.get('operations') or {}
    base = (d.get('api') or {}).get('base_url', '')

    executable = 0
    for name, op in ops.items():
        if not isinstance(op, dict):
            continue
        method, path = op.get('method', ''), op.get('path', '')
        if not method or not path:
            continue
        executable += 1
        if not path.startswith('http') and not base:
            issues.append(
                f'{cid}.{name}: path "{path}" is relative and the definition '
                f'has no api.base_url - the client builds a scheme-less URL '
                f'and the call fails before it is sent')

    # A definition that names the transport the client lacks is a deliberate
    # gap, not an oversight. GetStatus reaches the same conclusion by
    # inference, so the two cannot disagree; this only asks that the gap be
    # written down.
    transport = (d.get('api') or {}).get('transport', 'rest')
    if ops and not executable and transport == 'rest':
        issues.append(
            f'{cid}: {len(ops)} operation(s), none with both a method and a '
            f'path - ConnectorClient cannot execute any of them. Either give '
            f'the client that transport or make GetStatus report it as not '
            f'connectable, so the console stops offering a form that leads '
            f'nowhere.')

for i in issues:
    print(i)
if issues:
    print(f'\n{len(issues)} unexecutable definition(s)')
    sys.exit(1)
print(f'Connector definitions OK ({len(defs)} checked)')
