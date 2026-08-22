---
name: Help article writing
command: docs-help-article-writing
description: Write a searchable, self-service KB article from a resolved issue or FAQ
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Docs
    transport: api
  - id: Intercom
    transport: browser
  - id: Zendesk
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Documenting how to do something for a user who is stuck right now and is not reading for pleasure.

## Title it as the question they typed

"How do I export my data?" beats "Data export". The title is the search query, and matching it is most of whether the article is found at all.

## Answer in the first paragraph

Give the answer immediately, then the detail. A reader who already knows most of it needs one line; the ones who need the full walkthrough will keep reading. Never open with background.

## Structure the steps to be followed under stress

- Numbered, one action per step.
- Name the thing they click exactly as it appears on screen, in bold.
- Say what they should see after each step, so they know if they went wrong.
- Screenshot only where the interface is genuinely ambiguous; every screenshot is a maintenance liability.

## Cover the failure the reader is probably having

Most people arrive at a help article because something did not work, not because they want to learn. A "if this did not work" section covering the two or three common causes will resolve more tickets than the happy path did.

## Gotchas

- Do not document a workaround without saying it is one and linking the underlying issue.
- Date the article and re-check it when the interface changes; a confidently wrong instruction is worse than no article.
- Say plainly when something is not possible. Users spend far longer looking for a feature that does not exist than they do reading that it does not.
