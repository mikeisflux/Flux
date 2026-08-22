---
name: Friday Revenue Recap
command: ops-friday-revenue-recap
description: End-of-week pulse: revenue vs last week, top and bottom sellers, wins and watches
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

A short weekly read on what happened to revenue, for people who will not read a dashboard.

## Lead with the three numbers

Revenue booked this week, against the weekly run rate needed to hit the period, and the cumulative position against target. Everything else is explanation of those three. If the recap does not answer "are we on track" in the first line, it is a report and not a recap.

## Then what changed

- New business closed, named, with amounts.
- What was expected and did not close, with the reason and the new date.
- Anything churned or downgraded.
- Anything that moved into or out of the current period.

## Keep it the same every week

The value of a weekly recap is comparability. Same format, same metrics, same definitions, sent at the same time. A recap that restructures itself weekly cannot be read at a glance, which is its only purpose.

## Gotchas

- Distinguish booked from invoiced from collected. Three different numbers, and using them interchangeably is how a recap loses credibility.
- A week is a small sample; do not narrate noise as a trend.
- If nothing happened, say so in one line. Padding a quiet week teaches people to skim.
