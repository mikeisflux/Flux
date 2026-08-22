---
name: Unified cross-tool search
command: research-unified-cross-tool-search
description: One question, parallel searches across chat, email, docs, and trackers
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

Finding something when you do not remember which system it is in.

## Search by what you remember, in the right form

People remember fragments: a phrase, a person, a rough date, an attachment. Different systems index different things, so translate the fragment per system - a phrase into full text search, a person into a sender or assignee filter, a date into a range with slack either side because memory of dates is unreliable.

## Search in order of likelihood

Where does this kind of thing usually live? A decision is in chat or a document; a commitment is in email or a ticket; a number is in a sheet or a dashboard. Searching everywhere at once returns noise; searching the likely two systems first usually ends the search.

## Widen deliberately when it fails

- Drop the least certain term first, not the most.
- Try the synonym the other team would use.
- Search for the person rather than the content, then scan their output around the date.
- Look for the thing that referenced it rather than the thing itself.

## Gotchas

- Most search tools stem and stop-word differently; an exact-phrase search that fails may succeed unquoted, and vice versa.
- Archived and deleted items are usually excluded by default and are often where the answer is.
- Only search what you are entitled to see. Finding something you should not have access to is a finding to report, not to use.
