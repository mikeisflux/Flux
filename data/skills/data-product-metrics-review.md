---
name: Product Metrics Review
command: data-product-metrics-review
description: Turn product metrics into a scorecard with trends, drivers, and next actions
categories: [Data]
roles: [analysts, engineering]
writeScope: readonly
body_status: authored
---

## When to use

A recurring review of how the product is doing, or a one-off read before a planning cycle.

## Pick metrics that can move a decision

Start from the decision, not from what is easy to query. For each metric, be able to say what you would do if it moved 20% either way. If there is no answer, it is a vanity metric and it goes in an appendix.

A workable core:

- **Acquisition** - new signups, and where they came from.
- **Activation** - the share reaching the moment the product first works for them. Define that moment explicitly.
- **Retention** - a cohort curve, not a single churn number.
- **Depth** - actions per active user per period.
- **Revenue** - by cohort, so growth is not mistaken for a pricing change.

## Read the cohort curve properly

Retention is only meaningful as a curve by cohort. Look for whether it **flattens** - a curve that flattens has a retained core, a curve that keeps declining has none. Compare recent cohorts against older ones at the same age, never at the same date.

## Attribute movement before explaining it

When a metric moves, rule out in this order: an instrumentation change, a mix change, a seasonal effect, then a real product change. Most surprising metric moves are the first two.

## Gotchas

- An average hides a bimodal distribution. Show a percentile spread for anything user-level.
- Weekly actives divided by monthly actives is a ratio, not an engagement strategy.
- If the definition of a metric changed, the series before and after is two series. Do not plot them as one line.
