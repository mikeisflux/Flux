---
name: Support ticket triage
command: ops-support-ticket-triage
description: Categorize a ticket, set P1–P4 priority, check duplicates, and route it
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Zendesk
    transport: browser
  - id: Intercom
    transport: browser
  - id: Linear
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

A queue of tickets and the job is to get each to the right place quickly, not to solve them all.

## Separate severity from priority

**Severity** is how broken it is. **Priority** is how soon we act, which also weighs who is affected and what is contractually owed. A cosmetic issue for a customer in an escalation can outrank a functional bug for a free user, and conflating the two makes the queue unreadable.

## Triage each ticket into one of four

1. **Answerable now** - answer it and close it.
2. **Known issue** - link to the existing ticket, add the customer, respond with the status.
3. **Needs investigation** - route with the reproduction, the account and the timestamps attached.
4. **Not a support issue** - billing, sales, legal, abuse. Route and tell the customer where it went.

The failure mode is a fifth bucket - things that sit unclassified - so every ticket leaves triage in one of the four.

## Respond even when you cannot resolve

An acknowledgement with a realistic next step resets the customer's clock. Most dissatisfaction with support is about silence rather than about resolution time.

## Gotchas

- Check for a linked existing incident before investigating; parallel investigation of one outage is the most common waste in support.
- Tag the root cause category at close, or the queue can never tell you what to fix.
- A ticket reopened is a ticket that was closed wrongly. Track that rate; it says more than first-response time.
