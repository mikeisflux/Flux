---
name: Architecture decision records (ADRs)
command: engineering-architecture-decision-records-adrs
description: Document a design decision with options, trade-offs, and consequences
categories: [Engineering]
roles: [engineering]
writeScope: readonly
body_status: authored
---

## When to use

A decision has been made that is expensive to reverse, and the reasoning needs to survive the people who made it.

## What warrants one

Not every decision. Write an ADR when the decision is hard to reverse, affects more than one team, or when you can already imagine someone asking "why on earth is it like this" in a year. Choosing a datastore, an auth model, a public API shape, a deployment topology. Not choosing a lint rule.

## The structure

- **Title** - a short noun phrase, numbered.
- **Status** - proposed, accepted, superseded by ADR-N.
- **Context** - the forces at play, stated so a stranger understands the pressure. Constraints, deadlines, existing commitments, what was already true.
- **Decision** - what was chosen, in active voice: "We will...".
- **Consequences** - what becomes easier and what becomes harder. Both. An ADR listing only benefits is a sales document.
- **Alternatives considered** - and specifically why each lost.

## Write it at the time

An ADR written a month later is a reconstruction, and reconstructions launder the actual reasons - the deadline, the person who felt strongly, the thing nobody knew yet. Those are exactly what the reader needs.

## Gotchas

- Never edit an accepted ADR to reflect a new decision. Write a new one and mark the old one superseded; the history is the point.
- Keep them in the repository, numbered and immutable.
- If the consequences section is hard to write, the decision is not understood well enough to make yet.
