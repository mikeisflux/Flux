---
name: Accessibility review
command: docs-accessibility-review
description: WCAG 2.1 AA audit: contrast, keyboard, focus, touch targets, screen readers
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Figma
    transport: browser
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

WCAG 2.1 AA audit: contrast, keyboard, focus, touch targets, screen readers.

## Approach

1. **Understand the intent.**
2. **Draft.**
3. **Revise against the brief.**
4. **Deliver.**

## Heuristics

- State what you could not determine rather than filling the gap.
- Cite the source for every claim a reader would want to check.
- Stop and ask when the request is ambiguous in a way that changes the output.

## Gotchas

Verify the result against its source before reporting it as done.
