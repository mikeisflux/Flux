---
name: Month-End Journal Entry Playbook
command: ops-month-end-journal-entry
description: Standard entry types, documentation, approval matrix, and common-error checks
categories: [Ops]
roles: [ops, founders]
writeScope: readonly
body_status: authored
---

## When to use

The set of recurring entries that has to be posted every close, done consistently.

## Keep a standing schedule

One list of every recurring entry: what it is, how the amount is derived, which accounts, whether it reverses, and who prepares and approves it. Without this list the close depends on one person's memory, and the entry that gets missed is always the one only they knew about.

## Recalculate rather than copy

For each entry, recompute the amount from its source this period. Copying last month's entry and editing the date is the single most common cause of a stale accrual running unnoticed for months. Where the amount genuinely does not change, state that it was checked and unchanged.

## Post in a deliberate order

Reversals of last period's accruals first, so the accounts are clean, then this period's accruals. Doing it the other way round double-counts until the reversal lands, and interim reporting in that window is wrong.

## Gotchas

- An accrual with no reversal date set will still be on the balance sheet at year end.
- Review the standing schedule quarterly; entries survive the thing they were accruing for.
- Materiality applies here too - an entry below the threshold that takes an hour to prepare should be questioned, not automated.
