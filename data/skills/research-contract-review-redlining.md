---
name: Contract review & redlining
command: research-contract-review-redlining
description: Plain-English contract review with severity-ranked red flags and suggested edits
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
body_status: authored
---

## When to use

Reading a contract to find what needs changing, from a commercial rather than a legal standpoint.

## Read for the clauses that bite

In roughly this order:

- **Term, renewal and notice** - auto-renewal with a long notice window is the most common trap.
- **Payment** - amounts, timing, uplift mechanism, and whether uplift is capped.
- **Liability** - the cap, what is excluded from it, and whether it is mutual.
- **Indemnities** - who covers what, and whether it is symmetric.
- **Data** - ownership, processing terms, where it is stored, what happens on exit.
- **Termination** - for convenience, for cause, and what you get back.
- **Change control** - can they change the terms or the price unilaterally?

## Mark each issue by severity

Three levels: cannot sign as written, want changed, and noted. Mixing them means the important redlines get traded away alongside the cosmetic ones. For every "cannot sign", say what would make it acceptable, or the negotiation stalls on a rejection with no path.

## Look for what is missing

Absent clauses are harder to spot than bad ones and often matter more: no SLA, no exit assistance, no cap on uplift, no data return obligation, no subcontractor restriction.

## Gotchas

- This is commercial review, not legal advice. Anything material goes to a qualified lawyer, and that should be stated in the output rather than implied.
- Check the definitions section. A favourable clause can be undone by a definition three pages away.
- Order-of-precedence clauses decide which document wins. Read that before reading the schedules.
