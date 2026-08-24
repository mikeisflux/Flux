#!/usr/bin/env python3
"""What to register with each OAuth provider, so Flux can ship its own app.

A built-in `auth.client` turns a connector into one button. Creating one is
account work behind each provider's developer console, and this is the
checklist for it - derived from the definitions rather than written down
beside them, so it cannot drift from what the client actually sends.

The split that matters is PKCE. Flux's "secret" ships in a downloadable
binary, so everyone has it; PKCE is what binds an authorization code to the
client that asked for it, and without it an intercepted redirect is enough to
exchange the code for someone's tokens. So a provider is only a candidate for
a built-in app if it supports PKCE - which is a fact about that provider, to
be read out of its documentation, not guessed from here.
"""
import json
import pathlib
import sys
from urllib.parse import urlparse

ROOT = pathlib.Path(__file__).resolve().parent.parent
REDIRECT = 'http://127.0.0.1/flux/oauth'


def main() -> int:
    ready, blocked, done = [], [], []
    for f in sorted((ROOT / 'data' / 'connectors').glob('*.json')):
        d = json.loads(f.read_text(encoding='utf-8'))
        auth = d.get('auth') or {}
        if auth.get('type') != 'oauth2':
            continue
        row = {
            'id': d.get('id', f.stem),
            'host': urlparse(auth.get('authorize_url', '')).netloc,
            'scopes': auth.get('scopes') or [],
            'pkce': bool(auth.get('pkce')),
        }
        if isinstance(auth.get('client'), dict):
            done.append(row)
        elif row['pkce']:
            ready.append(row)
        else:
            blocked.append(row)

    def show(rows, title):
        print(f'\n{title} ({len(rows)})')
        for r in rows:
            print(f"  {r['id']:<18} {r['host']}")
            print(f"    redirect  {REDIRECT}")
            print(f"    scopes    {' '.join(r['scopes']) or '(none declared)'}")

    show(done, 'Flux already ships an app')
    show(ready, 'Ready for one - the definition sets PKCE')
    show(blocked,
         'Not yet - no PKCE in the definition. Check the provider\'s docs: if '
         'it supports PKCE, set pkce in the definition first; if it does not, '
         'this one keeps the bring-your-own-app form.')

    print(f'\nRegister each as an installed / desktop / native application '
          f'where the provider offers that type, with exactly the redirect '
          f'above, then add auth.client to its definition.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
