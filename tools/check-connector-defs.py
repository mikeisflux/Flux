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

# An operation field the registry never parses is authored data the agent
# never sees. `params` and `body` were in exactly that state for 35 operations
# while connector_list's own description told the model that "operation names
# and their required parameters are not guessable" - true, and then it did not
# supply them.
#
# Everything else at this level is documentation for whoever maintains the
# definition, and listing it here is what makes that a decision rather than an
# accident: a field added later and not parsed is not on this list, so it
# fires.
DOCUMENTED_ONLY = {
    'scope',        # the provider's own OAuth scope string, not our write scope
    'permissions',  # what the token needs; scopes are requested wholesale
    'rate_limit',   # per-operation limits, not enforced by the client
    'response',     # the shape that comes back
    'mutation',     # GraphQL operation name; no GraphQL transport yet
    'tool',         # MCP tool name; there is no MCP client in the tree
}

registry = pathlib.Path(
    'src/browser/connectors/connector_registry.cc').read_text(encoding='utf-8')
authored = set()
for f in defs:
    d = json.loads(f.read_text(encoding='utf-8'))
    for op in (d.get('operations') or {}).values():
        if isinstance(op, dict):
            authored |= set(op)

for field in sorted(authored - DOCUMENTED_ONLY):
    if f'"{field}"' not in registry:
        issues.append(
            f'operations.{field}: authored in data/connectors and never read '
            f'by connector_registry.cc. It reaches neither the client nor the '
            f'model - parse it into ConnectorOperation, or add it to '
            f'DOCUMENTED_ONLY in this check if it is a note for maintainers.')

# Every operation needs a human label. The Connectors screen lists them as the
# actions the agent will be able to take once the account is connected, and a
# raw id like "gmail_modify_labels" in that list is the difference between a
# screen someone reads and one they skim past.
for f in defs:
    d = json.loads(f.read_text(encoding='utf-8'))
    cid = d.get('id', f.stem)
    for name, op in (d.get('operations') or {}).items():
        if isinstance(op, dict) and not (op.get('label') or '').strip():
            issues.append(
                f'{cid}.{name}: no "label". The connector dialog lists the '
                f'actions by label, and an operation without one shows up as '
                f'its raw id.')

# A built-in OAuth app that is unsafe or half-written.
#
# auth.client is an app Flux registered with the provider, so the user does not
# have to. That is only sound because the authorization code is bound to a
# PKCE verifier: the "secret" ships inside a downloadable binary, so anyone has
# it, and without PKCE an intercepted redirect is enough to exchange the code
# for someone's tokens. With PKCE the secret stops being load-bearing, which is
# exactly why Google's installed-app flow mandates it.
#
# So: shipping a client for a provider REQUIRES pkce on that definition. A
# provider that does not support PKCE is not a candidate for a built-in app,
# and the honest answer there is to leave the user registering their own.
for f in defs:
    d = json.loads(f.read_text(encoding='utf-8'))
    cid = d.get('id', f.stem)
    auth = d.get('auth') or {}
    client = auth.get('client')
    if not isinstance(client, dict):
        continue
    if not (client.get('id') or '').strip():
        issues.append(
            f'{cid}: auth.client has no "id". A half-written block reads as a '
            f'registered app and connects with nothing.')
    if not (client.get('secret') or '').strip():
        issues.append(
            f'{cid}: auth.client has no "secret". If the provider genuinely '
            f'issues none, say so with "secret": "none" rather than omitting '
            f'it, so the gap is a decision.')
    if not auth.get('pkce'):
        issues.append(
            f'{cid}: ships auth.client but the definition does not set pkce. '
            f'The secret is in a downloadable binary, so PKCE is the only '
            f'thing binding the code to this client - without it an '
            f'intercepted redirect is enough to take the tokens. Either turn '
            f'pkce on, or drop the built-in app and let the user register '
            f'their own.')

for i in issues:
    print(i)
if issues:
    print(f'\n{len(issues)} definition problem(s)')
    sys.exit(1)
print(f'Connector definitions OK ({len(defs)} checked)')
