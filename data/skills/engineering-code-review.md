---
name: Code review
command: engineering-code-review
description: Structured review for security, performance, correctness, and maintainability
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: GitHub
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Reviewing someone's change before it merges, in a way that catches what matters and does not waste their week.

## Read in this order

1. **The description.** What is this trying to do, and is that the right thing to do? A well-implemented wrong change is the most expensive kind.
2. **The tests.** They tell you what the author believes the change does, and what they think can break.
3. **The interfaces** - new public functions, schema changes, API shapes. These are the parts that are expensive to change later.
4. **The implementation**, last. It is the easiest part to fix and the part reviewers over-index on.

## What to actually look for

- **Correctness at the edges**: empty, null, one, many, concurrent, and the error path. The happy path is usually fine.
- **What happens on failure** - is a partial write possible, and is it recoverable?
- **Data changes** - a migration that is not reversible, or that locks a large table.
- **Security** - anything taking user input into a query, a path, a shell, or a template.
- **Removed tests or assertions**, which are almost never incidental.

## How to say it

Separate what blocks merge from what does not, explicitly, on every comment. Say why, not just what - a reviewer's reasoning is what makes the next change better. Ask rather than assert when you are not sure: "what happens if this is called twice?" is better than "this is not idempotent" when it might be.

## Gotchas

- Do not review formatting a linter could catch. If it is not automated, that is the finding.
- A very large diff gets a worse review than three small ones, and saying so is more useful than pretending otherwise.
- Approving with unaddressed questions teaches everyone that questions are optional.
