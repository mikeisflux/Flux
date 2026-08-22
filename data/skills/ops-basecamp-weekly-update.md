---
name: Post the weekly Basecamp update
command: ops-basecamp-weekly-update
description: A project update built from what actually moved, drafted first and posted on approval
categories: [Ops, Docs]
roles: [ops, founders]
worksWith:
  - id: Basecamp
    transport: api
writeScope: send
body_status: authored
---

## When to use

Every week, on a project with a client or a stakeholder who is not in Basecamp daily.

## Build it from movement, not from opinion

The update is a diff of the week. Four reads:

- To-dos completed since the last update - `/todolists/{id}/todos.json?completed=true`, or `/my/assignments/completed.json` if it is one person's project.
- To-dos that became overdue - anything whose `due_on` passed with `completed: false`.
- Messages and documents posted - `/message_boards/{id}/messages.json`, `/vaults/{id}/documents.json`.
- Schedule entries in the coming week - `/schedules/{id}/entries.json`.

Anything you cannot point at one of those four for does not go in the update.

## Structure

Four short sections: **Shipped**, **In flight**, **Blocked or slipped**, **Next week**. Same order every week, so the reader is comparing like with like and can skim to the section they care about.

Slippage goes in its own section with the new date and who moved it. A slipped date buried in a paragraph is the thing every stakeholder learns to distrust the update over.

## Draft, then publish

Create the message **without** a `status` field. Basecamp makes it a draft: nothing is posted and nobody is notified. Show the user the draft. On approval, publish it.

This is not caution for its own sake - a message board post is the most public thing this connector can do, it notifies the whole project by default, and a message cannot be unposted, only edited with the notification already sent.

## Client visibility is a separate decision

If the project has clients enabled, `visible_to_clients` defaults to `false`. An update written for the client and posted invisible to them is a silent no-op that looks like success. Ask, set it explicitly, and say which way it went in the confirmation.

## Gotchas

- `subscriptions` as an array of person IDs narrows who is notified. Omitting it notifies and subscribes **everyone on the project**. On a project with fifteen people and two stakeholders, name the two.
- `category_id` is the message type ("Announcement", "FYI"). Fetch the project's types from `/buckets/{project_id}/categories.json` - one of the few endpoints that is still project-scoped only - and pick one rather than leaving the post untyped.
- Publishing an existing draft is `PUT /messages/{id}.json` with `{"status": "active"}` and nothing else. The update merges, so the draft keeps its subject, content and subscribers - resending them is how you accidentally change who gets notified.
