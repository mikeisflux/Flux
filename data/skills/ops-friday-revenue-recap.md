---
name: Friday Revenue Recap
command: ops-friday-revenue-recap
description: End-of-week pulse: revenue vs last week, top and bottom sellers, wins and watches
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: PayPal
    transport: browser
  - id: HubSpot
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

End-of-week pulse: revenue vs last week, top and bottom sellers, wins and watches.

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
