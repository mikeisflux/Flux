---
name: SEO audit
command: engineering-seo-audit
description: Check titles, headings, metadata, speed, and content gaps, then prioritize fixes
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Ahrefs
    transport: browser
  - id: Google
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Check titles, headings, metadata, speed, and content gaps, then prioritize fixes.

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
