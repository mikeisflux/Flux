---
name: Error queue triage
command: engineering-error-queue-triage
description: Archive non-actionable noise from your error tracker's queue, approval-gated
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Sentry
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Archive non-actionable noise from your error tracker's queue, approval-gated.

## Approach

1. **Reproduce.**
2. **Isolate.**
3. **Verify the fix.**
4. **Report.**

## Heuristics

- State what you could not determine rather than filling the gap.
- Cite the source for every claim a reader would want to check.
- Stop and ask when the request is ambiguous in a way that changes the output.

## Gotchas

Verify the result against its source before reporting it as done.
