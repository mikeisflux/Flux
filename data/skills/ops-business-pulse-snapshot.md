---
name: Business Pulse Snapshot
command: ops-business-pulse-snapshot
description: One-page cross-functional snapshot of cash, sales, pipeline, and what needs you today
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: QuickBooks
    transport: api
  - id: HubSpot
    transport: api
  - id: Gmail
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

A short, regular read on the health of the business across functions, for a leadership audience.

## Pick one number per function and hold it steady

Revenue, pipeline, cash runway, headcount, churn, and one product or operational measure. Six numbers, the same six every time, each with its prior period and its target. Adding a metric because it looks good this period is how a pulse becomes a highlight reel.

## Show direction, not just level

Each number with its change and, where the series is noisy, a short trend rather than a single comparison. A number without a direction cannot be acted on, and month-on-month alone over-reacts to seasonality.

## Flag by exception

Most weeks most numbers are fine. Mark the one or two that are off, say why, and say who owns the response. A snapshot where everything is commented is a snapshot nobody reads.

## Gotchas

- Definitions must be fixed and written down; a metric redefined mid-series destroys the comparison silently.
- Cash runway needs a stated burn assumption or it is not a number.
- Resist adding a seventh metric. The discipline is the product.
