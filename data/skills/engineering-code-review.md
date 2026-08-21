---
name: Code review
command: engineering-code-review
description: Structured review for security, performance, correctness, and maintainability
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: GitHub
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Structured review for security, performance, correctness, and maintainability.

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
