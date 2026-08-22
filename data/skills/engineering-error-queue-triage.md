---
name: Error queue triage
command: engineering-error-queue-triage
description: Archive non-actionable noise from your error tracker's queue, approval-gated
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Sentry
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

An error tracker full of noise, and the job is to get to the ones that mean something.

## Group before reading

Never read the queue chronologically. Aggregate by fingerprint and sort by:

1. **New in this release** - regressions are the most actionable thing in the queue.
2. **Rate of change** - a small error growing fast beats a large flat one.
3. **Users affected** rather than event count. Ten thousand events from one bot is not an incident.
4. **Blast radius** - does it break a flow, or does it log and continue?

## Classify each group

- **Real bug** - assign, link to a ticket, and note the release it appeared in.
- **Expected and unhandled** - a network timeout, a cancelled request. Should not be an error; fix the logging, not the code path.
- **Client noise** - browser extensions, ancient browsers, bots. Filter at the source, not by ignoring.
- **Already fixed, awaiting deploy** - mark resolved in the next release so it reopens if it comes back.

## Keep the queue honest

A tracker where most entries are noise trains everyone to ignore all of it, which is worse than not having one. Every triage pass should end with the noise categories filtered out at ingestion, not just marked ignored.

## Gotchas

- Errors spiking after a deploy are that deploy until proven otherwise.
- An error rate that drops to zero is more often broken reporting than a fix.
- Check whether the stack trace is symbolicated before spending time on it; an unsymbolicated trace is unreadable and the fix is in the build, not the code.
