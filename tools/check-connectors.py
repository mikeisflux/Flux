#!/usr/bin/env python3
"""Validate the connector definitions and keep them in step with the catalogue.

Two things go wrong with a directory of hand-written integration definitions:
a field quietly missing, and the definition drifting from what the console
advertises. This checks both, plus the one thing that actually matters at
runtime - that every definition says where its endpoints came from, so a
connector nobody could verify is never silently presented as if it were.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFS = ROOT / 'data' / 'connectors'
CATALOGUE = ROOT / 'src' / 'resources' / 'connectors.json'

WRITE_SCOPES = {'readonly', 'draft', 'send', 'purchase'}
# How well the endpoints are known:
# `discovery`   - fetched from the provider's own OAuth metadata document.
# `vendor-docs` - taken verbatim from the vendor's own documentation.
# `search`      - corroborated across documentation search results.
# The browser re-checks anything below `discovery` against the provider before
# its first use.
VERIFICATION = {'discovery', 'vendor-docs', 'search', 'not-applicable'}
AUTH_TYPES = {'oauth2', 'api_key', 'local', 'unsupported'}


def fail(path, message, errors):
    errors.append(f'{path}: {message}')


def check_definition(path, errors):
    try:
        d = json.loads(path.read_text(encoding='utf-8'))
    except json.JSONDecodeError as e:
        fail(path.name, f'not valid JSON - {e}', errors)
        return None

    if d.get('id') != path.stem:
        fail(path.name, f'id "{d.get("id")}" does not match the filename', errors)

    auth = d.get('auth')
    if not isinstance(auth, dict):
        fail(path.name, 'no auth block', errors)
        return d
    if auth.get('type') not in AUTH_TYPES:
        fail(path.name, f'auth.type must be one of {sorted(AUTH_TYPES)}', errors)

    # A personal token pasted by the user is a first-class alternative to the
    # OAuth dance, not a fallback: for a single-user browser it avoids shipping
    # a client secret in a binary, which cannot be kept secret anyway.
    if auth.get('personal_token'):
        pt = auth['personal_token']
        for field in ('label', 'where'):
            if not pt.get(field):
                fail(path.name, f'personal_token needs {field}', errors)
        # Present but possibly empty: Cloudflare tokens have no fixed prefix,
        # and the field existing is what says that was checked rather than
        # forgotten.
        if 'prefix' not in pt:
            fail(path.name,
                 'personal_token needs prefix (empty string if there is none)',
                 errors)

    if auth.get('type') == 'oauth2':
        for field in ('authorize_url', 'token_url'):
            if not auth.get(field):
                fail(path.name, f'oauth2 needs auth.{field}', errors)
        # An empty scope list is legitimate - Notion has no scope parameter at
        # all - but the key must be present and deliberate, not forgotten.
        if not isinstance(auth.get('scopes'), list):
            fail(path.name, 'oauth2 needs auth.scopes (may be an empty list)',
                 errors)
        for field in ('authorize_url', 'token_url'):
            url = auth.get(field, '')
            if url and not url.startswith('https://'):
                fail(path.name, f'auth.{field} must be https', errors)
        if auth.get('client_auth') not in ('basic', 'post', None):
            fail(path.name, 'auth.client_auth must be "basic" or "post"', errors)

    source = d.get('source')
    if not isinstance(source, dict):
        fail(path.name, 'no source block - every definition must say where its '
                        'endpoints came from', errors)
    else:
        if source.get('verified') not in VERIFICATION:
            fail(path.name,
                 f'source.verified must be one of {sorted(VERIFICATION)}', errors)
        if not re.fullmatch(r'\d{4}-\d{2}-\d{2}', str(source.get('checked', ''))):
            fail(path.name, 'source.checked must be an ISO date', errors)
        if source.get('verified') != 'not-applicable' and not source.get('refs'):
            fail(path.name, 'source.refs must cite where this came from', errors)

    for name, op in (d.get('operations') or {}).items():
        if op.get('write_scope') not in WRITE_SCOPES:
            fail(path.name,
                 f'operation "{name}" needs a write_scope in {sorted(WRITE_SCOPES)}',
                 errors)
        if auth.get('type') == 'oauth2' and not op.get('method'):
            fail(path.name, f'operation "{name}" needs a method', errors)
    return d


def main():
    errors = []
    catalogue = json.loads(CATALOGUE.read_text(encoding='utf-8'))['connectors']
    by_id = {c['id']: c for c in catalogue}

    defined = set()
    for path in sorted(DEFS.glob('*.json')):
        d = check_definition(path, errors)
        if d is None:
            continue
        defined.add(path.stem)
        if path.stem not in by_id:
            fail(path.name, 'not in src/resources/connectors.json', errors)

    # The catalogue's `definition` field is what the console renders a connect
    # button from, so a mismatch here is a button that lies.
    for c in catalogue:
        has_file = c['id'] in defined
        claims = c['definition'] == 'authored'
        if claims and not has_file:
            fail('connectors.json',
                 f'{c["id"]} is marked authored but data/connectors/{c["id"]}.json '
                 'does not exist', errors)
        if has_file and not claims:
            fail('connectors.json',
                 f'{c["id"]} has a definition file but is still marked pending',
                 errors)

    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        print(f'{len(errors)} connector problem(s)', file=sys.stderr)
        return 1

    authored = sum(1 for c in catalogue if c['definition'] == 'authored')
    print(f'Connectors OK - {authored}/{len(catalogue)} authored')
    return 0


if __name__ == '__main__':
    sys.exit(main())
