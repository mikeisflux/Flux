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
body_status: authored
---

## When to use

The books need closing for a period, on a schedule, without discovering a problem in the last hour.

## Work the checklist in dependency order

1. **Cut-off** - confirm nothing is still posting to the period. This first, or everything after it moves.
2. **Bank and cash** reconciled to statement.
3. **Receivables and payables** aged and agreed to the sub-ledger.
4. **Accruals and prepayments** - what was incurred but not invoiced, and what was paid in advance.
5. **Payroll** posted and reconciled.
6. **Fixed assets** - depreciation run, additions and disposals recorded.
7. **Intercompany** balances agreed both sides.
8. **Trial balance review** - variance against prior period, with every material movement explained.
9. **Lock the period.**

## Review the variance before signing

Compare every account against the prior period and against budget. Investigate anything above the materiality threshold in either direction. An unexplained favourable variance is as much a signal of a missing entry as an adverse one, and it is the one nobody chases.

## Leave an audit trail as you go

Every reconciliation supported by the statement or the schedule that evidences it, filed with the period. Reconstructing support in an audit six months later costs several times what filing it at the time does.

## Gotchas

- Manual journals posted after the review are the most common source of a restated close. Re-run the trial balance after any late entry.
- Foreign currency: fix the rate source and the rate date in policy, not per close.
- Track the close calendar against actual completion. A close that consistently overruns has a specific bottleneck, and it is usually one upstream dependency.
