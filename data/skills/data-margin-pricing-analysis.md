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
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Compute unit economics by product and model +5/+10/+15% pricing scenarios; data only.

## Approach

1. **Get the data.**
2. **Validate it.**
3. **Transform.**
4. **Deliver.**

## Heuristics

- State what you could not determine rather than filling the gap.
- Cite the source for every claim a reader would want to check.
- Stop and ask when the request is ambiguous in a way that changes the output.

## Gotchas

Verify the result against its source before reporting it as done.
