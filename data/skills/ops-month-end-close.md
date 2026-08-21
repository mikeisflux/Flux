---
name: Month-end close
command: ops-month-end-close
description: Sequence and track month-end close tasks so nothing slips before the deadline
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Docs
    transport: api
  - id: Sheets
    transport: api
  - id: QuickBooks
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Sequence and track month-end close tasks so nothing slips before the deadline.

## Approach

1. **Locate the records.**
2. **Reconcile.**
3. **Act.**
4. **Leave an audit trail.**

## Heuristics

- State what you could not determine rather than filling the gap.
- Cite the source for every claim a reader would want to check.
- Stop and ask when the request is ambiguous in a way that changes the output.

## Gotchas

Verify the result against its source before reporting it as done.
