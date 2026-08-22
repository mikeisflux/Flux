---
name: Budget vs actual variance analysis
command: data-budget-vs-actual-variance
description: Decompose budget-vs-actual variances into drivers with a clear narrative
categories: [Data]
roles: [analysts, engineering]
worksWith:
  - id: Docs
    transport: api
  - id: Sheets
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Month or quarter end, and someone needs to know where actuals diverged from plan and why.

## Build the variance table

One row per line item, with: budget, actual, variance in currency, variance as a percentage, and year-to-date versions of each. Sort by absolute currency variance, not by percentage - a 400% overrun on a £200 line is noise and a 6% overrun on a £2m line is the story.

## Explain, do not just report

A variance report that only lists numbers is a spreadsheet, not an analysis. For every material variance, establish which of these it is:

- **Timing** - the spend is real but landed in a different period. Reverses next month.
- **Volume** - more or fewer units than planned at the planned rate.
- **Rate** - planned volume at a different unit price.
- **Scope** - something happened that was not in the plan at all.

Timing variances need a note; rate and scope variances need an owner.

## Set the threshold before you look

Agree materiality up front - typically the greater of a fixed amount and a percentage, for example £10k or 5%. Deciding what counts as material after seeing the numbers is how a report becomes an argument.

## Gotchas

- Accruals and prepayments move cost between periods. Check whether the comparison is on a cash or accruals basis and say which.
- A favourable variance is not automatically good news: underspend on hiring is a missed plan, not a saving.
- Reforecast and budget are different baselines. Compare against the one the reader is accountable for.
