#!/usr/bin/env python3
"""A mojom field that a serializer writes but never reads, or never writes.

Workflows are the only mojom structs this fork persists: the scheduler turns a
TaskSpec into a pref dictionary and back. A field missed by that pair does not
degrade gracefully, because the restored struct is handed straight to code that
assumes it is complete:

  model          Never written and never read. TaskSpec::New() leaves it a null
                 StructPtr, PumpQueue does MakeProvider(*spec->model) to build
                 the provider, and mojo's StructPtr CHECKs on a null deref - so
                 clicking "Run now" on any saved workflow killed the browser
                 process. A missing field presented as a crash, not as a
                 workflow with no model.

  credit_budget  Round-tripped, but every workflow was saved with zero, and
                 StartRun refuses a zero budget. Same file, same pair.

The fields are read out of the .mojom rather than listed here, so adding one to
TaskSpec fails this check until the serializer carries it. That is the whole
point: the next field will be added by someone who has never read this file.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MOJOM = ROOT / 'src' / 'browser' / 'mojom' / 'flux.mojom'
SERIALIZER = ROOT / 'src' / 'browser' / 'scheduler' / 'workflow_scheduler.cc'

# The structs the scheduler flattens into one pref dictionary. ModelConfig is
# nested inside TaskSpec and its fields are written at the same level.
PERSISTED = ['TaskSpec', 'ModelConfig']

# Fields deliberately not persisted, each with the reason. Anything not named
# here has to round-trip.
EXEMPT = {
    # Vestigial: nothing reads it, and the mojom says so. Kept because older
    # saved workflows still carry the key.
    'profile_id',
}


def fields_of(mojom: str, struct: str) -> list[str]:
    match = re.search(rf'struct {struct}\s*\{{(.*?)\n\}};', mojom, re.S)
    if not match:
        raise SystemExit(f'{struct} not found in flux.mojom')
    out = []
    for line in match.group(1).splitlines():
        line = line.split('//')[0].strip()
        m = re.match(r'^[\w\.\?<>]+(?:\s*\?)?\s+(\w+)\s*;$', line)
        if m:
            out.append(m.group(1))
    return out


def main() -> int:
    mojom = MOJOM.read_text(encoding='utf-8')
    body = SERIALIZER.read_text(encoding='utf-8')

    written = set(re.findall(r'\.Set\(\s*"([\w_]+)"', body))
    read = set(re.findall(r'\.Find(?:String|Int|Bool|Double|List|Dict)\s*\(\s*"([\w_]+)"',
                          body))

    problems = []
    for struct in PERSISTED:
        for field in fields_of(mojom, struct):
            if field in EXEMPT:
                continue
            if field not in written:
                problems.append(
                    f'{struct}.{field}: never written by '
                    f'{SERIALIZER.name}. It is lost on restart.')
            elif field not in read:
                problems.append(
                    f'{struct}.{field}: written by {SERIALIZER.name} but never '
                    f'read back. The saved value is dead weight and the '
                    f'restored struct gets a default.')

    if problems:
        for problem in problems:
            print(problem)
        print(f'\n{len(problems)} field(s) missing from the workflow '
              f'serializer.')
        return 1

    total = sum(len(fields_of(mojom, s)) for s in PERSISTED)
    print(f'persisted spec OK ({total} fields across {len(PERSISTED)} structs, '
          f'{len(EXEMPT)} exempt)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
