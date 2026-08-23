#!/usr/bin/env python3
"""A pointer from base::Value::Find*/GetIf* dereferenced without a null check.

Find* and GetIf* return null when the key is absent or the type is wrong, and
what they are reading is never ours: a tool call written by a language model,
a JSON body from a provider or a connector's API, an OAuth token response.
"Required" in a schema is a request, not a guarantee. An absent key that
reaches a dereference takes the whole browser process down, which turns a
malformed reply from someone else's server into a crash on the user's machine.
A bad response has to fail the call, not the browser.

Almost every site in this tree is already written correctly, in one of two
idioms that a naive scan reads as a bug:

    if (const base::ListValue* c = root.FindList("content")) { ... *c ... }
    const std::string* s = root.FindString("stop"); x = s && *s == "tool_use";

The first declares inside the condition, so the body only runs non-null; the
second short-circuits on the same line. The first version of this check
understood neither and reported all 43 call sites in src/browser, every one of
them a false positive - a check nobody can act on, which is worse than no
check, because the next real one is buried in the noise.
"""
import pathlib
import re
import sys

# The accessors that return null rather than aborting. Find*() misses on an
# absent key or a type mismatch; GetIf*() on a type mismatch.
ACCESSOR = re.compile(r'(?:->|\.)(?:Find[A-Za-z]*|GetIf[A-Za-z]+)\s*\(')

# "const std::string* name =" / "base::DictValue *name =" / "auto* name ="
DECL = re.compile(
    r'(?:^|[;{}(,]|\bconst\b|\breturn\b)\s*'
    r'(?:const\s+)?(?:[\w:]+(?:<[^;]*>)?|auto)\s*\*\s*(\w+)\s*=')

CONDITION = re.compile(r'\b(?:if|while|for|switch)\s*\(')


def guards(text: str, name: str) -> bool:
    """True if `text` null-checks `name` before using it."""
    n = re.escape(name)
    return bool(re.search(
        # if (name) / if (!name) / while (name) ...
        rf'\b(?:if|while)\s*\([^)]*?(?<![\w.>]){n}\b'
        # name && ... / ... && name / name || / !name ||
        rf'|(?<![\w.>])!?{n}\s*(?:&&|\|\|)'
        rf'|(?:&&|\|\|)\s*!?{n}\b'
        # name ? ... : ...  (but not name->x ? ...)
        rf'|(?<![\w.>])!?{n}\s*\?'
        # CHECK(name) / DCHECK(name) / DCHECK_NE(name, nullptr)
        rf'|\b(?:CHECK|DCHECK|CHECK_NE|DCHECK_NE)\s*\(\s*{n}\b'
        # name == nullptr / name != nullptr
        rf'|(?<![\w.>]){n}\s*[!=]=\s*(?:nullptr|NULL)',
        text))


def uses(text: str, name: str) -> bool:
    """True if `text` dereferences `name`."""
    n = re.escape(name)
    return bool(re.search(rf'(?<![\w>*])\*{n}\b|(?<![\w.>]){n}\s*->', text))


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'src/browser')
    bad = 0
    for path in sorted(root.rglob('*.cc')):
        lines = path.read_text(encoding='utf-8').splitlines()
        for i, line in enumerate(lines):
            if not ACCESSOR.search(line):
                continue
            decl = DECL.search(line)
            if not decl:
                continue
            name = decl.group(1)
            # Declared inside the condition itself - `if (T* x = Find(...))` -
            # so the body is only reached when it is non-null. This is the
            # idiom the tree actually uses, and reading it as a bug is what
            # made the first version of this check useless.
            if CONDITION.search(line[:decl.start(1)]):
                continue
            # A guard on the declaration's own line: `T* s = Find(...);` on one
            # line and `x = s && *s == "..."` on the next is the second idiom,
            # so the scan has to start at the declaration, not after it.
            for j in range(i, min(i + 25, len(lines))):
                text = lines[j] if j > i else line[decl.end():]
                if guards(text, name):
                    break
                if uses(text, name):
                    print(f'{path}:{j + 1}: {name} is dereferenced but '
                          f'Find*/GetIf* on line {i + 1} can return null')
                    print(f'    {lines[j].strip()}')
                    bad += 1
                    break
                # Leaving the block without a use means it went somewhere this
                # scan cannot follow; stop rather than guess.
                if re.match(r'^\s*\}\s*$', lines[j]):
                    break

    if bad:
        print(f'\n{bad} unguarded Find*/GetIf* dereference(s). A null here is '
              f'a browser crash, not a failed call.')
        return 1
    print('null-deref: every Find*/GetIf* result is null-checked before use')
    return 0


if __name__ == '__main__':
    sys.exit(main())
