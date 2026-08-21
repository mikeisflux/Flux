---
name: CRM Cleanup Pass
command: sales-crm-cleanup-pass
description: Find stale deals, duplicate contacts, and missing fields in your CRM, then fix them
categories: [Sales]
roles: [sales, founders]
worksWith:
  - id: HubSpot
    transport: api
  - id: Salesforce
    transport: api
writeScope: readonly
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Find stale deals, duplicate contacts, and missing fields in your CRM, then fix them.

## Approach

1. **Gather context.**
2. **Qualify.**
3. **Draft the outreach.**
4. **Log it.**

## Heuristics

- State what you could not determine rather than filling the gap.
- Cite the source for every claim a reader would want to check.
- Stop and ask when the request is ambiguous in a way that changes the output.

## Gotchas

Verify the result against its source before reporting it as done.
