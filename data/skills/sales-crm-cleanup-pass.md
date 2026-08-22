---
name: CRM Cleanup Pass
command: sales-crm-cleanup-pass
description: Find stale deals, duplicate contacts, and missing fields in your CRM, then fix them
categories: [Sales]
roles: [sales, founders]
worksWith:
  - id: HubSpot
    transport: api
  - id: Salesforce
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

The CRM has decayed to the point where reports are not trusted. A structured pass to make it queryable again.

## Fix in this order

Later steps depend on earlier ones, so the order matters:

1. **Duplicates** - merge accounts by domain, then contacts by email. Doing anything else first means doing it twice.
2. **Ownership** - unowned or ex-employee-owned records, reassigned or archived.
3. **Stage and close date integrity** - anything with a close date in the past is either closed or wrongly dated. There is no third option.
4. **Required fields** on open opportunities - amount, stage, next step.
5. **Stale records** - no activity beyond a defined threshold, closed out with a reason rather than deleted.

## Decide the rules before you start

Write down what counts as a duplicate, what counts as stale, and which record wins in a merge. Applying a rule you invented halfway through is how a cleanup becomes its own data-quality incident.

## Gotchas

- Export everything before the first write. Merges are not reversible in most CRMs.
- Work in batches with a spot-check between them, not one bulk operation.
- Closing a stale deal changes historical win rate. Note the date of the cleanup so the discontinuity in the reporting is explainable.
- Cleanup without a change to how records are created just buys a few months.
