---
name: Month-End Close Packet
command: ops-month-end-close-packet
description: Reconcile books vs payment processors, flag gaps, narrate the P&L, export the packet
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: QuickBooks
    transport: api
  - id: Stripe
    transport: api
  - id: PayPal
    transport: browser
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Reconcile books vs payment processors, flag gaps, narrate the P&L, export the packet.

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
