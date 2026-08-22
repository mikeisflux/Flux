---
name: Marketing campaign execution
command: marketing-marketing-campaign-execution
description: Orchestrate a campaign: sales analysis, content brief, assets, segment, staged send
categories: [Marketing]
roles: [marketing, founders]
worksWith:
  - id: HubSpot
    transport: api
  - id: QuickBooks
    transport: api
  - id: PayPal
    transport: browser
writeScope: draft
body_status: authored
---

## When to use

Running a campaign that has already been planned, and keeping it honest while it is live.

## Pre-flight everything

The checks that catch the expensive mistakes:

- Every link, clicked, in the final environment - including the tracking parameters.
- Conversion tracking firing, verified with a real test conversion.
- Rendering on mobile, in dark mode, and in the two clients that matter.
- Spelling of every proper noun, especially the customer's and the reader's.
- Budget caps and end dates set, not left at default.
- Personalisation tokens with a fallback that reads sensibly when the field is empty.

## Watch the right things in the first 48 hours

Delivery before performance. A campaign that is not being delivered has no performance to read. Then early-signal metrics - open, click, landing page bounce - which are readable long before conversions are. Do not judge conversion rate on a sample too small to distinguish from noise, which is nearly always the first day.

## Change one thing at a time

Mid-flight changes are how a campaign becomes unanalysable. If something must change, note the timestamp and treat before and after as two campaigns. Resist the temptation to fix everything at once on day two.

## Gotchas

- A paused ad set that is restarted usually re-enters learning; check before pausing.
- Suppression lists and frequency caps matter more than the creative for anything sent repeatedly.
- Keep a dated log of every change. Without it the retrospective is guesswork.
