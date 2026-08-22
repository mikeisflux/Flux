---
name: Tech-debt assessment
command: engineering-tech-debt-assessment
description: Categorize debt and rank it by impact, risk, and effort
categories: [Engineering]
roles: [engineering]
writeScope: readonly
body_status: authored
---

## When to use

Someone wants to know what the debt is and what to do about it, and needs an answer that survives contact with a roadmap.

## Inventory what is actually costing something

Debt is only debt if it charges interest. For each item, establish what it costs *now*:

- Time lost on every change to the area.
- Incidents or bugs traceable to it, with dates.
- Work that is blocked or being routed around.
- Onboarding time it adds.

An ugly module nobody touches and nothing depends on costs nothing. It is not debt, it is just ugly.

## Rank on cost and risk, not on offence

Score each item on: **interest** (cost per month if untouched), **principal** (cost to fix), and **risk** (probability and blast radius of a failure). Rank by interest against principal, and pull risk items forward regardless of that ratio. The most annoying code is rarely the most expensive.

## Propose work that fits a sprint

A proposal to "rewrite the billing module" will never be scheduled. Break each item into changes that ship independently and leave the system working at every step. Attach each to a piece of feature work that already has to touch the area - debt paid alongside a feature gets approved and debt paid alone does not.

## Gotchas

- Distinguish deliberate debt taken to hit a date from accidental debt from not knowing better; the conversation about each is different.
- A rewrite is not a debt-reduction strategy, it is a new project with the old project's requirements only partly known.
- Re-run the assessment quarterly. Items drop off because the code stopped being touched, and that is a legitimate resolution.
