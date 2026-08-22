---
name: System design
command: engineering-system-design
description: Framework for requirements, high-level design, scale, and trade-offs
categories: [Engineering]
roles: [engineering]
writeScope: readonly
body_status: authored
---

## When to use

Designing something before it is built, at the level where the decisions are expensive to reverse.

## Establish the constraints first

A design is only assessable against constraints, so get numbers before drawing anything:

- **Load** - requests per second at peak, not average. Read/write ratio.
- **Data** - volume today, growth rate, retention requirement.
- **Latency** - what the user notices, at the 99th percentile rather than the mean.
- **Consistency** - what must be immediately correct, and what can be eventually correct. This one decides most of the architecture.
- **Failure tolerance** - what is allowed to be down, for how long, and what must never be lost.

## Design the data first

The data model outlives every service around it. Settle the entities, their keys, their relationships and their access patterns before choosing components. A component chosen before the access patterns are known is a guess.

## Then the components, and their failure modes

For each component: what it owns, what it calls, and what happens when the thing it calls is slow, down, or returning wrong answers. Timeouts, retries with backoff and jitter, and a bound on the queue - every unbounded queue is an outage with a delay on it.

## Write down what was rejected

The alternatives considered and why they lost is the most useful part of a design document and the first part that gets skipped. Six months later it is the only thing that stops the same option being re-proposed.

## Gotchas

- Do not design for load you do not have; do design so that the scaling path exists and is stated.
- Every cache introduces a staleness question. Answer it in the design.
- If the design needs distributed transactions, look again - it is usually a sign the boundaries are wrong.
