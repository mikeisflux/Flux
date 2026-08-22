---
name: Session Replay UX Research
command: research-session-replay-ux-research
description: Analyze session replays to surface user journeys, friction, and pain points
categories: [Research]
roles: [analysts, founders]
worksWith:
  - id: Session replay
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Watching recorded sessions to find where an interface fails people, without leaning on aggregate metrics.

## Sample deliberately

Random sessions are mostly uneventful. Sample the ones likely to contain a failure: sessions with a rage click or a rapid back, sessions that abandoned at a known drop-off, sessions unusually long for the task, and sessions from a segment that converts poorly. Then watch a handful of successful ones as a control.

## Watch for hesitation, not just error

The signal is where people slow down: cursor hovering between two options, scrolling up and back down, re-reading, opening a field and leaving it. These are comprehension failures and they never appear in an error log.

## Record observations before interpretations

Note the timestamp, what the user did, and what they appeared to be trying to do - separately from why you think it happened. Interpretation written into the observation cannot be re-examined later.

## Quantify before recommending

A behaviour seen in two sessions is a hypothesis. Check how common it is in the aggregate data before proposing a change; replay finds the problem, analytics sizes it, and a recommendation needs both.

## Gotchas

- Replay tools capture real user data. Confirm that sensitive fields are masked before watching, not after.
- Do not identify individual users in a write-up.
- A session that looks confused may be someone being interrupted. Do not over-read a single recording.
