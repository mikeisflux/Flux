---
name: Customer Voice to Top Fixes
command: ops-customer-voice-top-fixes
description: Synthesize disputes, tickets, and reviews into themes and a top-3 fixable-issues list
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: PayPal
    transport: browser
  - id: HubSpot
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Turning the accumulated mass of customer feedback into a short ranked list of what to fix.

## Normalise before counting

The same problem arrives as a support ticket, a churn reason, a sales objection and a review. Deduplicate across sources by the underlying problem, not by the words used - otherwise the loudest channel wins rather than the biggest problem.

## Rank by cost, not by volume

Weight each theme by: how many customers, how much revenue those customers represent, whether it caused churn or a lost deal, and how often it recurs per affected customer. A problem hitting three enterprise accounts weekly outranks one that annoys two hundred free users once.

## Distinguish the three kinds

- **Bug** - it does not work as designed. Fix.
- **Gap** - it works as designed and the design is wrong for them. Product decision.
- **Comprehension** - it works and they could not find or understand it. Usually a documentation or interface fix, and the cheapest category by far.

Mixing these produces a list engineering cannot act on.

## Gotchas

- Quote the customer verbatim for each theme. A theme with no quote loses its meaning by the time it reaches a planning meeting.
- Feedback is biased towards people who complain; check the aggregate data before sizing.
- Close the loop with the people who reported it when something ships. That is the part everyone skips and the part that produces the next round of good feedback.
