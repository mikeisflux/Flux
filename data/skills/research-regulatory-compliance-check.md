---
name: Regulatory compliance check
command: research-regulatory-compliance-check
description: Map the laws, licenses, and disclosures that apply to a planned action or launch
categories: [Research]
roles: [analysts, founders]
worksWith:
  - id: FTC
    transport: browser
  - id: gov registry
    transport: browser
  - id: Google
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Map the laws, licenses, and disclosures that apply to a planned action or launch.

## Approach

1. **Scope the question.**
2. **Gather primary sources.**
3. **Cross-check.**
4. **Report with citations.**

## Heuristics

- State what you could not determine rather than filling the gap.
- Cite the source for every claim a reader would want to check.
- Stop and ask when the request is ambiguous in a way that changes the output.

## Gotchas

Verify the result against its source before reporting it as done.
