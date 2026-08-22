---
name: Customer escalation packaging
command: ops-customer-escalation-packaging
description: Bundle full reproduction context so engineering can act on an escalation fast
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Linear
    transport: api
  - id: GitHub
    transport: api
  - id: Zendesk
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

An issue needs to go up - to engineering, to management, or to a vendor - and the packaging determines how fast it moves.

## Lead with impact, not with the symptom

Who is affected, how many, since when, and what it is costing - revenue at risk, work blocked, contractual exposure. The technical detail matters, but it does not set priority; impact does, and an escalation that opens with a stack trace gets triaged on the stack trace.

## Give them everything they need to start

- The reproduction, or the exact steps that led to it.
- Account, environment, timestamps with timezone, request or trace identifiers.
- What has already been tried and ruled out.
- Logs and screenshots attached, not described.
- The customer's own words about what they need.

## State the ask and the deadline

"Please investigate" produces nothing. Say what you need - a root cause, a workaround, a decision, a date - and by when, and what happens if that is missed. An escalation without a deadline joins a queue.

## Gotchas

- Do not escalate the same issue in three channels; it produces three partial investigations.
- Keep the customer informed on a cadence even when there is nothing new. "No update yet, next update Thursday" preserves the relationship.
- Redact customer data that the escalation does not need.
