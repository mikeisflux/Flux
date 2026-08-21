---
name: Customer escalation packaging
command: ops-customer-escalation-packaging
description: Bundle full reproduction context so engineering can act on an escalation fast
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Linear
    transport: api
  - id: GitHub
    transport: api
  - id: Zendesk
    transport: browser
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Bundle full reproduction context so engineering can act on an escalation fast.

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
