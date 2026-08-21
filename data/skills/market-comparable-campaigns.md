---
name: Study campaigns like mine and pull the playbook
command: market-comparable-campaigns
description: A breakdown of comparable crowdfunding campaigns: funding, tiers, cadence, and what actually drove pledges
categories: [Marketing, Research]
roles: [marketing, founders]
worksWith:
  - id: Sheets
    transport: api
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

Before launching or relaunching a crowdfunded product, to ground the plan in
campaigns that already worked in the same niche rather than in general advice.

## Why comparables beat advice

Generic crowdfunding advice is written for the median campaign across every
category. A campaign in your specific niche tells you what your actual buyers
respond to: the price they accept, the tiers they pick, the art they stop
scrolling for. That is available and public, and almost nobody reads it
systematically.

## Find the comparables

Search the platforms for finished campaigns in the same category and format.
Aim for 15-25. Include failures deliberately — a campaign that missed its goal
is more informative than another success, and the successes are the only ones
anyone ever looks at.

Filter to genuine comparables: similar audience, similar format, similar
first-time-vs-established creator status. A campaign from a creator with an
existing 50k following is not a comparable for a first launch, however similar
the product looks.

## Pull for each

- Funding goal, amount raised, backer count, **average pledge**
- Whether it funded, and how fast — day-one percentage is the strongest single
  signal, since campaigns are largely decided in the first 48 hours
- **Tier structure**: price points, what was in each, which tiers took the most
  backers. Backer counts per tier are usually visible.
- Add-ons and stretch goals, and whether they landed
- Update cadence during the campaign
- Page structure: where the art sits, video length, how far down the ask is
- Delivery: promised date vs actual. Overruns are near-universal and worth
  calibrating against.
- Comments: what backers asked before pledging. This is the objection list,
  written by the buyers themselves.

## Report

A comparison table plus a short written read on:

1. **The price the market accepts** — where the backer mass actually sat, not
   the highest tier offered.
2. **What the top tier looked like** in campaigns with strong average pledges.
3. **The recurring pre-pledge objections**, which belong on the page as answers.
4. **Realistic delivery** based on what comparable creators actually shipped.

## Heuristics

- Backer count matters more than dollars raised for judging reach.
- A campaign that funded in a day had an audience before it launched. Note
  where that audience came from — that is the real lesson.
- Ignore outliers with celebrity attachment. They are not a comparable.

## Gotchas

Platform search buries finished campaigns; going through category and sort
filters, or an external tracker, finds far more than the default search does.
