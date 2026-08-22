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
body_status: authored
---

## When to use

Chasing money that is late, without damaging the relationship.

## Check your own side first

Before chasing, verify: the invoice was actually sent, to the right address and the right entity, with a purchase order number if one is required, and that no dispute or credit note is outstanding. A meaningful share of overdue invoices are overdue because of something on the seller's side, and chasing those costs goodwill for nothing.

## Escalate on a schedule, in a fixed sequence

- **Day 1 past due** - a short, friendly reminder with the invoice attached. Assume it was missed.
- **Day 7** - a call or a direct message to the known contact, asking whether there is a problem with the invoice.
- **Day 14** - escalate to accounts payable and copy the commercial contact, with a statement of all outstanding items.
- **Day 30** - a formal notice referencing the contractual terms, and internal escalation.

The sequence matters more than the wording: predictable, escalating contact collects faster than sporadic chasing.

## Find the actual obstacle

Most late payments are process, not refusal: a missing purchase order, an unapproved invoice, a payment run date, a changed contact. Ask directly what is needed to get it paid and by when, and record the answer.

## Gotchas

- Never threaten anything you will not do, and never mention legal action without authority.
- Get every promise to pay in writing with a date, and diarise the date.
- Repeated late payment from one customer is a credit control decision, not a collections problem.
