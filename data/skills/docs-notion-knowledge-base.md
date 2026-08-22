---
name: Notion knowledge base
command: docs-notion-knowledge-base
description: Turn conversations and decisions into structured, linkable Notion pages for reuse
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Notion
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Building or fixing an internal knowledge base so that people find things and the content does not rot.

## Structure around finding, not around the org chart

People search for a task, not for a department. Organise by the question being asked - "how do I get access to X", "what is our policy on Y" - and keep the hierarchy shallow. Three levels is the practical limit; beyond that people stop navigating and start asking in chat, which is the failure mode.

## Every page needs an owner and a review date

Both visible on the page. A knowledge base without them accumulates confidently wrong pages, and one wrong page teaches people to distrust all of them. A quarterly sweep that archives anything past its review date is more valuable than any amount of new content.

## Make duplication impossible rather than discouraged

One canonical page per topic, linked from everywhere else. Where two pages overlap, merge them and leave a redirect. The moment two pages both partly answer a question, both become unreliable.

## Gotchas

- Databases with consistent properties beat nested pages: they can be filtered, sorted and reviewed in bulk.
- Templates for recurring document types keep structure consistent without policing.
- Archive rather than delete, but get archived content out of search.
- Measure what is actually opened. The pages nobody reads should be removed, not improved.
