---
name: Lead Triage & Call List
command: sales-lead-triage-call-list
description: Score inbound leads by engagement, fit, and urgency into a ranked call list
categories: [Sales]
roles: [sales, founders]
worksWith:
  - id: HubSpot
    transport: api
  - id: Gmail
    transport: api
  - id: Google Calendar
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

A pile of inbound or list-sourced leads and limited hours. The job is an ordered call list, not a scored spreadsheet.

## Rank on two axes only

**Fit** - do they look like the customers who succeed? **Intent** - is there evidence they are in motion right now?

Intent beats fit for ordering today's calls: a mediocre-fit lead who requested a demo this morning is a better call than a perfect-fit lead who downloaded a PDF last quarter. Fit decides whether they stay on the list; intent decides the order.

## Signals worth weighting

- Demo or pricing page request - highest, and decays within days.
- Multiple people from the same domain in a short window - a buying group forming.
- Reply to an earlier sequence, even a negative one.
- A trigger event at the account.
- Job title matching the economic buyer rather than an end user.

## Produce the list

One row per lead, ordered, with: name, company, why now (the specific signal and its date), the opening line, and the next step if they do not answer. Cap it at what can genuinely be called today - a list of 200 is a list nobody works.

## Gotchas

- Deduplicate by domain, not by email. Three people from one company is one account.
- Route rather than discard poor-fit leads with real intent; they are often referrals waiting to happen.
- Freshness decays fast. A list built on Monday is a different list by Thursday, so rebuild rather than work down.
