---
name: Sweep a Basecamp project for rot
command: ops-basecamp-todo-hygiene
description: Find the unassigned, undated and long-dead to-dos, and propose a decision for each
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Basecamp
    transport: api
writeScope: draft
body_status: authored
---

## When to use

Before a planning session, or when a project's to-do count has stopped meaning anything because nobody trusts the list.

## The four kinds of rot

Walk every list in the project's todoset and sort each open to-do into one:

- **Unowned** - no `assignee_ids`. Nobody has agreed to do it, so it is a wish.
- **Undated** - no `due_on`. Never late, therefore never urgent, therefore never done.
- **Stale** - `updated_at` older than 60 days with no comments. The world moved.
- **Overdue and untouched** - past `due_on`, no activity since it lapsed. The date was fiction.

A to-do can be in two categories. Report it in the worst one only; a list where items repeat teaches people to skim it.

## Propose a decision, not a cleanup

For each item, one of: **assign**, **date**, **close**, or **keep as backlog**. The point of the sweep is that somebody says "close" out loud - to-dos do not rot because people are careless, they rot because nothing ever forced the close decision.

Batch the proposals into one list for approval. Do not delete or complete anything on your own judgement: completing a to-do is indistinguishable from doing the work, and it notifies the completion subscribers.

## Applying it

On approval:

- Assign and date with `PUT /todos/{id}.json`.
- Close with `POST /todos/{id}/completion.json` - and be honest in the summary that Basecamp has no "abandoned" state, so a closed-as-obsolete to-do looks exactly like a finished one to everyone else. If that matters, add a comment first saying why it was closed.
- Backlog items go to a list named for what they are. Moving them off the active list is most of the value.

## Gotchas

- A to-do list can itself be finished. Check the list's own status before reporting twelve stale to-dos on a list that was completed in March.
- `completed: true` items are not in the default index. That is what you want here, but it also means "the list is empty" can mean "everything is done", not "nothing is planned" - read `completed_ratio` on the list before saying a project has no work.
- To-dos can live directly under the todoset, outside any list. Sweeping only the lists misses them.
