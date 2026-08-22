---
name: Operational Runbook
command: ops-operational-runbook
description: Turn a recurring task into an exact step-by-step runbook with rollback and escalation
categories: [Ops]
roles: [ops, founders]
writeScope: readonly
body_status: authored
---

## When to use

The document someone follows to operate a system - routinely or when it misbehaves.

## Separate routine from exceptional

Two different documents, or two clearly marked sections. Routine operations are read calmly and can carry explanation; exception handling is read under pressure and must be pure procedure. Mixing them means the emergency steps are buried in prose.

## Make every step executable

Exact commands with placeholders clearly marked, the expected output, and what to do if the output differs. Link directly to the dashboards, consoles and logs rather than describing where they are - nobody navigates by description at 3am.

## Include the things people are afraid to write down

- The manual intervention that is technically not supposed to be needed.
- The known flaky step and how many retries is normal.
- What breaks downstream if this is done at the wrong time.
- Who actually knows about this system, by name.

These are the parts that only exist in someone's head, and they are the reason the runbook is worth writing.

## Gotchas

- Test it by having someone unfamiliar execute it; hesitation marks the defects.
- State the blast radius of each destructive step before the step, not after.
- Date it, own it, and review it whenever the system changes. A stale runbook is followed, which makes it worse than none.
