---
name: Standup updates
command: engineering-standup-updates
description: Turn recent activity into a clean yesterday / today / blockers update
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: GitHub
    transport: api
  - id: Slack
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

The daily or async update, written so it is worth someone else's thirty seconds.

## Three lines, in this order

1. **What moved** - what is now true that was not yesterday. Not what you touched; what changed state.
2. **What is next** - the specific thing today, not the epic.
3. **What is in the way** - named, with what you need and from whom.

If nothing is in the way, say nothing rather than writing "no blockers".

## Write it for the reader

The reader wants to know whether anything affects them and whether anything needs their help. So: name the ticket or the surface, not the internal detail; say if a date moved; and put the blocker first if there is one, because that is the only part that needs action today.

## Gotchas

- "Continuing to work on X" for three days is a blocker that has not been named yet. Say what is actually hard.
- Do not list every commit. A standup is not a changelog.
- If something slipped, say so on the day it slipped, not on the day it was due.
