---
name: NDA triage
command: research-nda-triage
description: Screen an NDA against standard carveouts and classify it green, yellow, or red
categories: [Research]
roles: [analysts, founders]
worksWith:
  - id: Docs
    transport: api
  - id: Google Drive
    transport: api
  - id: Acrobat
    transport: browser
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Screen an NDA against standard carveouts and classify it green, yellow, or red.

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
