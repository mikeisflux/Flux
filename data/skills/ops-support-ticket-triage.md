---
name: Support ticket triage
command: ops-support-ticket-triage
description: Categorize a ticket, set P1–P4 priority, check duplicates, and route it
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Zendesk
    transport: browser
  - id: Intercom
    transport: browser
  - id: Linear
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Categorize a ticket, set P1–P4 priority, check duplicates, and route it.

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
