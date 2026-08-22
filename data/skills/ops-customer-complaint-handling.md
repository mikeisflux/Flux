---
name: Customer complaint handling
command: ops-customer-complaint-handling
description: Work a complaint end-to-end: pull context, draft a reply, suggest an operational fix
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Gmail
    transport: api
  - id: HubSpot
    transport: api
  - id: PayPal
    transport: browser
writeScope: draft
body_status: authored
---

## When to use

A customer is unhappy and has said so. The job is to resolve it and to learn from it.

## Acknowledge before investigating

A fast acknowledgement that says you have read it, understood the specific problem, and when you will come back does more for the outcome than a slow complete answer. Silence while investigating reads as being ignored, which is usually the actual complaint by the time it escalates.

## Establish what happened, from the record

Reconstruct the timeline from logs, tickets and messages before responding on substance. Where the customer's account and the record differ, the difference is usually informative rather than dishonest - it is often a case of the product doing something they did not expect.

## Respond with the four parts

1. What happened, plainly, without jargon.
2. Why, if you know. If you do not yet, say so and say when you will.
3. What you are doing about it for them, specifically.
4. What changes so it does not recur - only if that is actually being done.

Apologise for the impact without asserting liability on anything contractual or safety-related.

## Gotchas

- Do not offer a remedy you have not confirmed you can deliver.
- Escalate on sight anything alleging discrimination, harassment, safety, data loss or regulatory breach - those are not complaint handling.
- Log the root cause in a category. Complaints handled individually and never aggregated means the same complaint forever.
