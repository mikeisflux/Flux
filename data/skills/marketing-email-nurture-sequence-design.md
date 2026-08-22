---
name: Email nurture sequence design
command: marketing-email-nurture-sequence-design
description: Plan a multi-email sequence with timing, branching, and copy that converts
categories: [Marketing]
roles: [marketing, founders]
worksWith:
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Designing a sequence of emails that moves someone from signing up to being ready to buy.

## Design around what they need to believe

List the beliefs someone must hold before they will buy - that the problem is worth solving, that this approach works, that it works for someone like them, that it is worth the price, that switching is survivable. One email per belief, in that order. A sequence organised around your feature list instead of their objections converts badly.

## Set the cadence to the decision, not to a calendar

A considered purchase needs weeks; an impulse one needs days. Front-load: the first email within minutes of signup, when intent is highest, and space widening after. Every email must be worth sending on its own - a sequence padded to reach seven emails trains people to unsubscribe.

## Write each one to stand alone

Assume none of the previous emails were read. One idea, one call to action, and enough context to make sense cold. The subject line describes the value in the email, not the campaign.

## Build in the exits

- Someone who converts leaves the sequence immediately. Nothing damages trust like being sold something you have bought.
- Someone who replies goes to a human.
- Someone who has not opened five in a row should be suppressed, not sent a sixth.

## Gotchas

- Test the plain-text rendering; a meaningful share of recipients see it.
- Personalisation tokens need a fallback that reads naturally, or the first email says "Hi ,".
- Measure to the outcome, not to opens. Open rate has been unreliable since privacy-protecting mail clients became the default.
