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
body_status: authored
---

## When to use

Establishing whether something is permitted, and what it requires, before it ships.

## Scope it before searching

Which jurisdictions, which regulator, which activity, and which date. Regulation is jurisdiction-specific and time-specific, and an answer without both stated is not an answer. If the activity crosses borders, each jurisdiction is a separate question.

## Go to the primary source

The regulation or the regulator's own guidance, not a summary of it. Summaries lag amendments and drop the exceptions, and the exceptions are usually where the answer is. Record the specific provision and its version.

## Separate the three questions

- **Is it permitted?**
- **What must be in place** - registration, disclosure, consent, records, a named responsible person?
- **What must be evidenced**, and for how long? The evidencing requirement is the part most often missed and the part an audit tests.

## Gotchas

- Guidance is not law but regulators enforce against it; note which is which.
- An exemption usually has conditions that must be continuously met, not met once.
- Where the answer is genuinely unclear, say so and recommend qualified advice. A confident wrong answer here is the most expensive kind of output this produces.
