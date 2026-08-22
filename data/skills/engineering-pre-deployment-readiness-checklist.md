---
name: Pre-Deployment Readiness Checklist
command: engineering-pre-deployment-readiness-checklist
description: Verify tests, approvals, migrations, and rollback triggers before shipping
categories: [Engineering]
roles: [engineering]
writeScope: readonly
body_status: authored
---

## When to use

Before a release goes out, especially one that is risky or that nobody will be awake for.

## The checks that catch real problems

- **Migrations** are backwards-compatible with the currently running version, so a rollback does not corrupt data. This is the single most common cause of an unrecoverable deploy.
- **Feature flags** default to off, and the kill switch has been tested rather than just written.
- **Config** required by the new code exists in the target environment. Missing config is the most common preventable failure.
- **Dependencies** - anything the release needs deployed first is already out.
- **Rollback** is understood, and someone has said out loud what it involves.

## Observability before, not after

The dashboard and alerts for the new behaviour exist *before* the deploy. Shipping something you cannot observe means the first sign of a problem is a customer. Know which metric would move if this went wrong, and have it on a screen.

## Human factors

- Who is watching, for how long, and how are they contacted?
- Is anyone else deploying into the same surface in the window?
- Is the timing sensible - not Friday afternoon, not during a peak, not while the person who wrote it is on a plane.

## Gotchas

- A checklist everyone ticks without reading is worse than none. Keep it short enough to be genuinely done.
- "Tested in staging" is only meaningful if staging has comparable data volume and the same config shape.
- Record what was skipped and why. That record is the input to the next incident review.
