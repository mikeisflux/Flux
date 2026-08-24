#!/usr/bin/env python3
"""A nullable mojom field used as if it held a value.

A `string?` in a .mojom is `std::optional<std::string>` in C++, and an optional
does not convert to a string_view. So this:

    base::StrAppend(&joined, {answer->text, "\\n"});

does not compile, because QuestionAnswer::text is `string?`. It reads perfectly
and it is the obvious thing to write. Nothing in this container is a compiler,
so it reached a commit.

The distinction it erases matters too: that field documents null as "skipped"
and an empty string as "there is no value", which lead the agent somewhere
different. Code treating the optional as a string has already lost that before
it fails to build.

The first version matched field NAMES anywhere, and reported seven things of
which every one was wrong: CompletionResponse::error and Message::text are
plain std::strings that happen to share a name with a nullable mojom field.
`text`, `error` and `detail` are far too common for a name to be evidence. So
this one is type-directed: it only looks at a variable it has watched being
declared as a `mojom::XPtr`, and only at fields nullable in that exact struct.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MOJOM = ROOT / 'src' / 'browser' / 'mojom' / 'flux.mojom'
BROWSER = ROOT / 'src' / 'browser'


def nullable_by_struct(text: str) -> dict[str, set[str]]:
    out: dict[str, set[str]] = {}
    for match in re.finditer(r'struct (\w+)\s*\{(.*?)\n\};', text, re.S):
        name, body = match.group(1), match.group(2)
        fields = set()
        for line in body.splitlines():
            line = line.split('//')[0].strip()
            m = re.match(r'^[\w\.<>]+\?\s+(\w+)\s*;$', line)
            if m:
                fields.add(m.group(1))
        if fields:
            out[name] = fields
    return out


def main() -> int:
    structs = nullable_by_struct(MOJOM.read_text(encoding='utf-8'))
    if not structs:
        print('no nullable mojom fields found - check the parser')
        return 1

    # A use is safe when the optional is unwrapped or guarded.
    safe = re.compile(r'\.(?:value|has_value|value_or)\s*\(')
    bad = 0
    checked = 0
    for cc in sorted(BROWSER.rglob('*.cc')):
        lines = cc.read_text(encoding='utf-8').splitlines()
        # name -> mojom struct, from declarations seen so far in this file.
        bound: dict[str, str] = {}
        for i, line in enumerate(lines):
            for m in re.finditer(r'mojom::(\w+)Ptr[&\s]+(\w+)\b', line):
                if m.group(1) in structs:
                    bound[m.group(2)] = m.group(1)
            for name, struct in bound.items():
                for field in structs[struct]:
                    use = re.search(rf'(?<!\*){re.escape(name)}->{field}\b', line)
                    if not use or safe.search(line):
                        continue
                    checked += 1
                    # Only the string-building shapes: that is where an
                    # optional silently fails to convert.
                    if not re.search(r'base::Str(?:Cat|Append)\s*\(', line):
                        continue
                    print(f'{cc.relative_to(ROOT)}:{i + 1}: {struct}::{field} '
                          f'is `?` in flux.mojom, so `{name}->{field}` is a '
                          f'std::optional and will not convert to a '
                          f'string_view')
                    print(f'    {line.strip()}')
                    bad += 1

    if bad:
        print(f'\n{bad} nullable mojom field(s) used as values. That is a '
              f'compile error, and there is no compiler here to catch it.')
        return 1
    print(f'nullable mojom fields OK ({len(structs)} structs with nullable '
          f'fields, {checked} typed uses seen)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
