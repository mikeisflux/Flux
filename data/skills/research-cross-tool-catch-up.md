---
name: Cross-tool catch-up digest
command: research-cross-tool-catch-up
description: Daily or weekly digest of action items, decisions, and mentions
categories: [Research]
roles: [analysts, founders]
worksWith:
  - id: Gmail
    transport: api
  - id: Slack
    transport: api
  - id: Google Drive
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Coming back after being away, and needing to know what happened across several systems without reading everything.

## Sweep the systems in order of consequence

Not chronologically, and not by tool. In order of what could need action today:

1. Things addressed to you directly and still unanswered.
2. Decisions made in your absence that affect your work.
3. Anything that changed state on work you own - a deal, a ticket, a release, an incident.
4. Everything else, as a count rather than a list.

## Collapse threads to outcomes

A forty-message thread is one line: what was decided and by whom. Reproducing the thread defeats the purpose. Where nothing was decided, say that - an unresolved thread needs you more than a resolved one does.

## Separate needs-you from for-information

Two sections, and the first should be short. The point of a catch-up is to get to action quickly; a single undifferentiated list means reading everything anyway.

## Gotchas

- Respect the boundaries of what you should be reading. A catch-up is not a licence to sweep private channels.
- Note the window covered explicitly, so the gap between the last catch-up and this one is visible.
- Flag anything with a deadline that has already passed at the very top.
