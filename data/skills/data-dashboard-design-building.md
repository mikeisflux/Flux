---
name: Dashboard design & building
command: data-dashboard-design-building
description: Turn a sheet or query results into a clean dashboard with the right chart types
categories: [Data]
roles: [analysts, engineering]
worksWith:
  - id: Docs
    transport: api
  - id: Sheets
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Turning a sheet or a set of queries into a dashboard people will actually use, rather than one they open once.

## Decide who it is for and what it changes

A dashboard with no decision behind it becomes a wall of charts nobody reads. Settle first:

- Who opens this, how often, and what do they do differently based on it?
- What is the one number at the top? If there are five, there is no top.
- What is the refresh cadence, and does the data actually arrive that often?

## Lay it out top-down

1. **Headline row** - two to four numbers with their change against the comparison period.
2. **Trend** - the headline numbers over time, so the reader can see whether today is unusual.
3. **Breakdown** - the same measure cut by the one or two dimensions that explain movement.
4. **Detail table** - the rows behind it, so somebody can check.

Anything that does not serve the decision goes below the fold or into a second tab.

## Build it to be trusted

- Put the data-as-of timestamp on the page, in the reader's timezone.
- Define every metric on the page itself, not in a wiki nobody opens.
- Make filters visible and their default state obvious. A dashboard silently filtered to one region is worse than no dashboard.
- Link each chart to the query behind it.

## Gotchas

- A dashboard that takes more than a few seconds to load will not be used. Pre-aggregate.
- Do not put a metric on it that nobody has agreed the definition of; the first meeting will be about the definition and not the number.
- Review it after a month. The charts nobody looked at should be removed, not defended.
