---
name: Turn a Basecamp thread into to-dos
command: ops-basecamp-thread-to-todos
description: Pull the real commitments out of a message and its comments, and put them on the list
categories: [Ops]
roles: [ops, founders]
worksWith:
  - id: Basecamp
    transport: api
writeScope: draft
body_status: authored
---

## When to use

A message board thread has run to thirty comments, several people have agreed to do several things, and none of it is on a to-do list.

## Read the whole thread before writing anything

`/messages/{id}.json` then `/recordings/{id}/comments.json` - the comments are on the *recording*, and a message is a recording, which is why the path does not say "messages". Paginate to the end. The last comment is where a decision gets reversed, and a to-do created from comment four that comment twenty-eight cancelled is worse than no to-do at all.

## What counts as a commitment

A named person, a verb, and something that can be finished. "We should look at pricing" is not a to-do. "Sam is going to send the revised pricing before Friday" is three fields: assignee Sam, content "Send the revised pricing", `due_on` the coming Friday.

If any of the three is missing, still create the to-do - but leave the field empty rather than inventing it, and say in the summary which ones you left blank. A guessed due date reads as a commitment somebody never made.

## Put it on the right list

`GET /projects/{id}.json`, find the `todoset` in the dock, `GET /todosets/{id}/todolists.json`. Match on the thread's subject before creating a new list. Most projects already have the list this belongs on and the failure mode here is a fourteenth list called "Follow-ups".

Then `POST /todolists/{id}/todos.json` with `content`, `description` (a one-line quote of the comment it came from, and a link), `assignee_ids`, and `due_on`.

## Leave notify off, then say so

`notify: false` is the default and the right one here. The people in the thread already had the conversation; a notification for each of nine extracted to-dos is a way to teach them to mute the project. Post one comment back on the thread listing what was created instead - that is the notification, and it is one.

## Gotchas

- `assignee_ids` needs person IDs, not names. `GET /projects/{id}/people.json` for the project's own roster; the account-wide `/people.json` will happily give you the ID of somebody who cannot see the project, and the to-do will be created assigned to a person who never sees it.
- Rich text is HTML, and the allowed tag set is narrow. Wrap the description in `<div>` and keep to `<strong>`, `<em>`, `<a>`, `<ul>`, `<li>`, `<blockquote>`.
- Check the existing to-dos on the list before creating. Threads get re-read, and running this twice on the same thread should add nothing the second time.
