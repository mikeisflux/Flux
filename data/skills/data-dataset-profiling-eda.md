---
name: Dataset profiling (EDA)
command: data-dataset-profiling-eda
description: Assess a dataset's shape, quality, and patterns so you trust it before analysis
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

Assess a dataset's shape, quality, and patterns so you trust it before analysis.

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
