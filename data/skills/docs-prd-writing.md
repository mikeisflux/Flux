---
name: PRD writing
command: docs-prd-writing
description: Turn a rough idea into a structured spec with goals, scope, and success metrics
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

A piece of work needs specifying before it is built, and the specification has to survive being read by engineering, design and a stakeholder who was not in the room.

## Lead with the problem, in evidence

A PRD that opens with a solution has already skipped the argument. Open with:

- **Who has the problem**, specifically, and how often.
- **The evidence** it is real - a support volume, a funnel drop, a set of quotes with dates. Not "users have told us".
- **What they do today instead**, which is the true competitor.
- **What changes if we fix it**, as a number you could later check.

## Specify behaviour, not implementation

Describe what the system does from outside: the states, the transitions, and what the user sees in each. Include the unhappy paths explicitly - empty, error, permission denied, offline, and partially complete. Those are the parts that get invented under time pressure if the document does not settle them.

## Say what is out of scope

The out-of-scope list does more work than the scope list. Every plausible adjacent thing that will be suggested, listed, with a one-line reason. Without it, the same three suggestions arrive in every review.

## Define done

Launch criteria that are checkable, and the metric that decides whether it worked, with the value that counts as success stated before launch rather than after.

## Gotchas

- If you cannot state the problem without naming the solution, the problem is not understood yet.
- A PRD is a decision record, not a living document. When it changes materially, note what changed and why.
- Every open question should have an owner and a date, or it will be answered by whoever is implementing it, silently.
