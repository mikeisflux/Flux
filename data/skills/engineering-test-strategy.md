---
name: Test strategy
command: engineering-test-strategy
description: Balance coverage, speed, and maintenance across the testing pyramid
categories: [Engineering]
roles: [engineering]
writeScope: readonly
body_status: authored
---

## When to use

Deciding what to test and at what level, for a project or a change, rather than testing everything badly.

## Choose the level by what can break

Test at the lowest level that can actually catch the failure:

- **Unit** - branching logic, calculations, parsing, edge cases. Fast, and where most of the tests should be.
- **Integration** - anything crossing a boundary: the database, an API, a queue. Where the interesting bugs live.
- **End-to-end** - the two or three flows whose breakage would be an incident. Expensive and flaky in proportion to their number.

A test at the wrong level is slow, brittle, or both, and tells you less.

## Test the behaviour, not the implementation

Assert on what is visible from outside the unit. A test that breaks when internals are refactored, with no behaviour change, is a tax on every future change and will eventually be deleted rather than fixed.

## Cover the cases people skip

Empty, one, many. Zero, negative, maximum. Unicode and very long strings. Concurrent access. The second call after a failure. Clock crossing midnight, month end, and a leap day.

## Gotchas

- Coverage measures lines executed, not assertions made. It is a floor, not a goal.
- A flaky test is worse than no test: it trains people to re-run rather than look. Fix or delete it, never retry it.
- When a bug reaches production, ask which level should have caught it. That is where the regression test goes.
