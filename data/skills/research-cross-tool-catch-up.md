---
name: Cross-tool catch-up digest
command: research-cross-tool-catch-up
description: Daily or weekly digest of action items, decisions, and mentions
categories: [Research]
roles: [analysts, founders]
worksWith:
  - id: Gmail
    transport: api
  - id: Slack
    transport: api
  - id: Google Drive
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Daily or weekly digest of action items, decisions, and mentions.

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
