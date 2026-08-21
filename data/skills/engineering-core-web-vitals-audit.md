---
name: Core Web Vitals Audit
command: engineering-core-web-vitals-audit
description: Profile page load performance, measure Core Web Vitals, and prioritize fixes by impact
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Chrome DevTools
    transport: browser
  - id: Chrome
    transport: browser
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Profile page load performance, measure Core Web Vitals, and prioritize fixes by impact.

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
