---
name: Margin & Pricing Analysis
command: data-margin-pricing-analysis
description: Compute unit economics by product and model +5/+10/+15% pricing scenarios; data only
categories: [Data]
roles: [analysts, engineering]
worksWith:
  - id: QuickBooks
    transport: api
  - id: PayPal
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

A pricing decision, a margin that moved, or a question about which products and customers actually make money.

## Get the cost base right first

Margin analysis is mostly a costing problem wearing an analysis hat.

- **Gross margin** = revenue minus direct cost. Be explicit about what is direct: COGS, payment fees, delivery, and for software, hosting and third-party API cost per unit.
- **Contribution margin** subtracts variable selling cost too - commission, ad spend attributable to the sale.
- Allocated overhead does not belong in either. Once overhead is allocated, every per-unit conclusion becomes an artefact of the allocation key.

## Segment before concluding

Blended margin hides everything worth knowing. Cut by:

- Product or SKU
- Customer or customer tier
- Channel
- Cohort or vintage

Rank by contribution in currency and by margin percentage separately. The two rankings disagreeing is usually the finding: the biggest revenue line is often not the biggest profit line.

## Pricing moves

- Compute the **volume you can afford to lose** at a given price rise: at a 40% margin, a 10% price rise can lose 20% of volume and break even. Do that arithmetic before the debate, not during it.
- Check discount leakage: list price against realised price per segment. A discount policy nobody measures is a discount policy nobody follows.
- Look for price points crossing a psychological or contractual threshold before recommending a rise.

## Gotchas

- Mix shift moves blended margin with no price or cost change at all. Always decompose margin movement into price, cost and mix before explaining it.
- Refunds and chargebacks belong against the period of the original sale, not the period they land in.
- A negative-margin product can be correct if it drives attach. Say so explicitly rather than recommending a cut.
