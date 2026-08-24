#!/usr/bin/env python3
"""Check the hand-written TS stub's field types against flux.mojom.

`tools/webui-typecheck/stubs/flux.mojom-webui.d.ts` stands in for bindings
that only exist inside a Chromium build, so `tsc` here compiles the console
*against* the stub. A stub that is merely incomplete is safe - anything the
console uses that the stub does not declare fails the check, which is the
signal to add it. A stub that is WRONG is not: it is a self-consistent world
where every line that agrees with the fiction passes, and the build finds out
two hours later on the user's machine.

That happened. `array<uint8> bytes` was declared `Uint8Array` in the stub
because that is what a bytes field obviously is. The generator emits
`number[]`, and `ask.ts` died at target 116 of 1536 with

    Type 'Uint8Array<ArrayBuffer>' is missing the following properties from
    type 'number[]'

`check-ts-typemaps.py` is the same idea pointed at a different half of the
problem: it verifies the stub against typemaps declared in OTHER modules'
BUILD.gn files, which nothing in the .mojom reveals. This one verifies the
stub against flux.mojom's own declarations, using the mapping table read out
of `mojom_ts_generator.py` at the pinned tag rather than from memory - which
is where the Uint8Array came from.

It is deliberately narrow, for the reason check-arity.py is: a false positive
on a real build costs more than a miss. Only fields declared in BOTH the
mojom and the stub are compared, only for types it can resolve without
leaving flux.mojom - imported types (`url.mojom.Url`, `mojo_base.mojom.Time`)
are check-ts-typemaps.py's job, because a typemap can rewrite them to
anything and the .mojom does not say so.
"""

import re
import sys
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MOJOM = ROOT / 'src' / 'browser' / 'mojom' / 'flux.mojom'
STUB = ROOT / 'tools' / 'webui-typecheck' / 'stubs' / 'flux.mojom-webui.d.ts'

# mojo/public/tools/bindings/generators/mojom_ts_generator.py, _kind_to_ts_type
# at 152.0.7977.60. Every integer width is "number" except 64-bit, which is
# "bigint" - that distinction is the other way this stub can be quietly wrong.
PRIMITIVE = {
    'bool': 'boolean',
    'int8': 'number', 'uint8': 'number',
    'int16': 'number', 'uint16': 'number',
    'int32': 'number', 'uint32': 'number',
    'float': 'number', 'double': 'number',
    'int64': 'bigint', 'uint64': 'bigint',
    'string': 'string',
}


def strip_comments(text):
    return re.sub(r'//[^\n]*', '', text)


def parse_mojom(text):
    """Returns (structs, local_type_names).

    structs maps a struct name to a list of (field_name, mojom_type).
    """
    text = strip_comments(text)
    local = set(re.findall(r'^\s*(?:struct|enum|union)\s+(\w+)', text, re.M))
    structs = {}
    for name, body in re.findall(r'^\s*struct\s+(\w+)\s*\{(.*?)^\s*\};',
                                 text, re.M | re.S):
        fields = []
        for decl in body.split(';'):
            # "array<uint8> bytes", "string? detail", "TaskSpec spec"
            m = re.match(r'^([\w.]+\??|(?:array|map)<[^>]+>\??)\s+(\w+)$',
                         decl.replace('\n', ' ').strip())
            if m:
                fields.append((m.group(2), m.group(1)))
        structs[name] = fields
    return structs, local


def to_camel(name):
    head, *rest = name.split('_')
    return head + ''.join(p[:1].upper() + p[1:] for p in rest)


class Unresolvable(Exception):
    pass


def ts_type(kind, local):
    """The TS type mojom_ts_generator.py emits for this field kind."""
    nullable = kind.endswith('?')
    bare = kind[:-1] if nullable else kind
    if bare.startswith('map<'):
        raise Unresolvable(bare)
    if bare.startswith('array<'):
        inner = bare[len('array<'):-1].strip()
        if inner.endswith('?'):
            # Array<(T | null)> - no field in flux.mojom takes this shape, and
            # guessing at one that does not exist is how a check rots.
            raise Unresolvable(bare)
        inner_ts = ts_type(inner, local)
        out = f'{inner_ts}[]'
    elif bare in PRIMITIVE:
        out = PRIMITIVE[bare]
    elif '.' in bare:
        # Imported. A ts_typemap in the owning module's BUILD.gn can rewrite
        # this to anything; check-ts-typemaps.py is what reads those.
        raise Unresolvable(bare)
    elif bare in local:
        out = bare
    else:
        raise Unresolvable(bare)
    return f'({out} | null)' if nullable else out


def parse_stub(text):
    text = strip_comments(text)
    out = {}
    for name, body in re.findall(
            r'export interface (\w+)\s*\{(.*?)^\}', text, re.M | re.S):
        fields = {}
        for decl in body.split(';'):
            decl = ' '.join(decl.split())
            m = re.match(r'^(\w+)\s*:\s*(.+)$', decl)
            if m:
                fields[m.group(1)] = m.group(2)
        out[name] = fields
    return out


def normalize(ts):
    """`(string | null)` and `string|null` are the same declaration."""
    ts = ts.replace(' ', '')
    while ts.startswith('(') and ts.endswith(')'):
        ts = ts[1:-1]
    return '|'.join(sorted(ts.split('|')))


def check(mojom_text, stub_text):
    structs, local = parse_mojom(mojom_text)
    stub = parse_stub(stub_text)
    findings = []
    compared = 0
    for struct, fields in sorted(structs.items()):
        if struct not in stub:
            continue  # Incomplete is allowed; wrong is not.
        for field, kind in fields:
            prop = to_camel(field)
            if prop not in stub[struct]:
                continue
            try:
                expected = ts_type(kind, local)
            except Unresolvable:
                continue
            compared += 1
            actual = stub[struct][prop]
            if normalize(actual) != normalize(expected):
                findings.append(
                    f'{struct}.{prop}: mojom `{kind}` generates '
                    f'`{expected}`, stub declares `{actual}`')
    return findings, compared


def self_test():
    """Break it on purpose. A check that prints nothing has proved nothing."""
    mojom = textwrap.dedent('''
    struct Thing {
      string name;
      string? detail;
      array<uint8> bytes;
      array<string> tags;
      uint64 total;
      uint32 count;
      bool done;
      Other other;
      url.mojom.Url page;
    };
    struct Other {
      string id;
    };
    ''')
    good = textwrap.dedent('''
    export interface Thing {
      name: string;
      detail: string|null;
      bytes: number[];
      tags: string[];
      total: bigint;
      count: number;
      done: boolean;
      other: Other;
      page: string;
    }
    export interface Other {
      id: string;
    }
    ''')
    findings, compared = check(mojom, good)
    assert not findings, findings
    # 8 on Thing (`page` is imported and skipped) plus Other.id.
    assert compared == 9, compared

    breaks = [
        ('bytes: number[];', 'bytes: Uint8Array;'),   # the real bug
        ('total: bigint;', 'total: number;'),         # 64-bit is bigint
        ('detail: string|null;', 'detail: string;'),  # nullability dropped
        ('done: boolean;', 'done: string;'),
        ('other: Other;', 'other: string;'),
    ]
    for old, new in breaks:
        assert good.count(old) == 1, old
        findings, _ = check(mojom, good.replace(old, new))
        assert len(findings) == 1, (new, findings)

    # And prove the nullability normalizer is not just accepting everything.
    findings, _ = check(mojom, good.replace('detail: string|null;',
                                            'detail: (string | null);'))
    assert not findings, findings
    return True


def main():
    self_test()
    if not MOJOM.exists() or not STUB.exists():
        print(f'check-mojom-stub-types: missing {MOJOM} or {STUB}')
        return 1
    findings, compared = check(MOJOM.read_text(encoding='utf-8'),
                               STUB.read_text(encoding='utf-8'))
    if findings:
        print('check-mojom-stub-types: the TS stub disagrees with flux.mojom.')
        print('The console compiles against the stub, so this passes here and')
        print('fails inside the build.\n')
        for f in findings:
            print(f'  {f}')
        return 1
    print(f'check-mojom-stub-types: {compared} stub field types agree with '
          f'flux.mojom')
    return 0


if __name__ == '__main__':
    sys.exit(main())
