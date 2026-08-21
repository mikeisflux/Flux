---
name: Overdue invoice collection
command: ops-overdue-invoice-collection
description: Draft tone-matched reminders for overdue invoices, scored by payment history
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: QuickBooks
    transport: api
  - id: PayPal
    transport: browser
  - id: Stripe
    transport: api
writeScope: draft
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Draft tone-matched reminders for overdue invoices, scored by payment history.

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
