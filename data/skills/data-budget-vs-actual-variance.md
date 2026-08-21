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
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Decompose budget-vs-actual variances into drivers with a clear narrative.

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
