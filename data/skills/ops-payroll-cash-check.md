---
name: Payroll Cash Check
command: ops-payroll-cash-check
description: Confirm you can make payroll: forecast cash, rank overdue invoices, stage reminders
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: QuickBooks
    transport: api
  - id: PayPal
    transport: browser
  - id: Stripe
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Confirming, before the payroll run, that the money will be there and that the run is right.

## Check the cash first

Payroll is the one payment that cannot be delayed. Confirm the cleared balance on the funding date - not today's balance, and not including receipts expected but not received. Account for the employer taxes and pension payments, which usually leave on a different date and are routinely forgotten in the check.

## Then check the run against last period

Total gross, total net, headcount, and employer cost, each compared with the prior run with every variance explained. A movement with no explanation is an error until proven otherwise. Look specifically at: starters and leavers, contract changes, one-off payments, and anyone whose net moved more than a few percent.

## Check the leavers and the starters individually

These are where errors concentrate. A leaver still on the run, a starter missing, a final payment without the correct accrued leave. Each one is both a cash error and an employee-relations problem.

## Gotchas

- Bank cut-off times and non-working days move the funding date. Count backwards from the pay date using the actual banking calendar.
- Changes to bank details for an employee are a fraud vector; verify out of band, always, and never from the email that requested it.
- Keep the check evidenced. This is a control, not a habit.
