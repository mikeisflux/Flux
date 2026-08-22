---
name: CRM activity logging
command: sales-crm-activity-logging
description: Log calls, emails, and meeting notes into your CRM from inbox and calendar context
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

Getting what actually happened into the CRM, promptly and consistently enough to be worth querying later.

## Log the things that are queryable

Free-text notes are unsearchable at scale. The fields that earn their keep:

- **Activity type and date** - call, email, meeting, demo.
- **Contacts involved**, linked rather than typed.
- **Stage change with a reason**, if one happened.
- **Next step and its date.**
- **Loss or risk reason**, from a fixed list rather than prose.

Put the narrative in the note, but never put a fact in the note that belongs in a field.

## Do it same day

Recall degrades within hours and CRM hygiene is entirely a timeliness problem. Same-day partial logging beats complete logging next week, which in practice means never.

## Gotchas

- Match on domain and existing contact records rather than creating new ones; duplicate contacts are how pipeline reporting quietly breaks.
- Do not log a stage change without the evidence that justifies it.
- Auto-captured email and calendar activity is not a substitute for a next step, which no integration can infer.
