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
body_status: authored
---

## When to use

Assembling the pack that evidences the close and goes to whoever reviews or audits it.

## What the packet contains

- Trial balance, current and prior period, with variances.
- One reconciliation per balance sheet account, each tied to external support.
- The journal listing for the period, with manual entries flagged separately.
- Supporting schedules: accruals, prepayments, fixed assets, deferred revenue.
- The variance commentary, explaining every movement above materiality.
- The close checklist, signed off, with dates and preparers.

## Make every number traceable

Any figure in the packet should be traceable to its source in one step: a reconciliation to a statement, a schedule to a sub-ledger, a journal to its support. A number a reviewer cannot follow is a number that gets queried, and queries are the whole cost of a review.

## Separate preparer and reviewer

Different people, evidenced with dates. Self-review is the control weakness auditors find first, and it is the cheapest one to fix.

## Gotchas

- Version the packet. A packet updated in place after review makes the review meaningless.
- Include the unreconciled items with an explanation rather than omitting them; a known unexplained difference is a finding, a hidden one is a problem.
- Keep the packet immutable once the period is locked.
