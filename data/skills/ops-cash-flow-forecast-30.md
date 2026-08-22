---
name: Cash Flow Forecast (30/60/90)
command: ops-cash-flow-forecast-30
description: Forecast cash with confidence bands and named risk flags; answer "will I make payroll?"
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

Forecasting cash at 30, 60 and 90 days, so that a shortfall is visible before it is a crisis.

## Start from the bank, not from the P&L

Cash is not profit. Begin with the actual bank balance today, then model receipts and payments by the date money moves, not the date revenue or cost is recognised. Every timing difference - payment terms, VAT, payroll dates - matters more than the amounts.

## Build the three horizons differently

- **30 days** - almost entirely known: the receivables ledger with expected collection dates, the payables run, payroll, rent, tax. This should be accurate to within a few percent.
- **60 days** - known commitments plus expected collections from invoices not yet issued.
- **90 days** - increasingly a forecast; model it as a range rather than a point.

## Apply real collection behaviour

Do not assume invoices are paid on terms. Use the actual historical distribution per customer - the large slow payer is usually the entire forecasting problem. Flag any single customer whose late payment would breach the minimum balance.

## State the minimum and the floor

The lowest projected balance and the date it occurs is the output. Everything else is workings. Set a floor below which action is required, and say what the action is.

## Gotchas

- Include the non-obvious outflows: quarterly VAT, annual insurance, tax instalments, and anything that only occurs twice a year.
- Model a downside: the largest customer pays 30 days late. If that breaches the floor, that is the finding.
- Re-forecast weekly at 30 days. A monthly cash forecast is a monthly surprise.
