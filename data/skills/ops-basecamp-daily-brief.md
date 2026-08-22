---
name: Start the day from Basecamp
command: ops-basecamp-daily-brief
description: One brief of what is overdue, due today, and what was said while you were away
categories: [Ops]
roles: [ops, founders, everyone]
worksWith:
  - id: Basecamp
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

First thing in the morning, or after a day away, when the question is "what needs me today" and the honest answer is spread across four projects.

## Read in this order

`/my/assignments/due.json?scope=overdue` first, then `due_today`. Overdue comes first because it is the only part of the brief where the answer might be "this should have shipped and did not". Then `/my/assignments.json`, whose `priorities` array is the user's own Up Next list - what they already decided mattered, which outranks anything the brief infers.

Then `/my/readings.json` for what happened while they were away: `unreads` is the real inbox, `bubble_ups` is what Basecamp itself thought was worth resurfacing.

## Write it as a decision list, not a feed

Three sections, and nothing else:

- **Late** - each item with how many days, the project, and who else is on it.
- **Today** - due today, plus anything from Up Next that has no date.
- **Waiting on you** - unread mentions and comments where a reply unblocks someone.

Everything else in the notification list is FYI and belongs in a single line at the bottom: "17 other updates across 4 projects." A brief that reprints the notification feed has not done the work.

## Say what is actually late

`due_on` is a date, not a datetime. Something due today is not late. Something due yesterday is one day late, and saying "overdue" without the number lets a two-month-old to-do hide behind a one-day one.

## Gotchas

- Campfire chatter is not in `/my/readings.json`. If a project runs on its Campfire, read `/chats/{id}/lines.json` for that project explicitly - otherwise the brief will confidently miss the conversation where the decision was made.
- `priorities` being empty means the user has not set an Up Next, not that nothing is urgent. Do not report it as "nothing prioritised".
- Assignments span every project the user can see, including archived-adjacent ones they have stopped caring about. Group by project so a dead project's backlog does not pad the count.
