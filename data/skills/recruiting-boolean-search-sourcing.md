---
name: Boolean search sourcing
command: recruiting-boolean-search-sourcing
description: Build precise boolean strings to surface candidates on LinkedIn, GitHub, and more
categories: [Recruiting]
roles: [recruiting]
worksWith:
  - id: LinkedIn
    transport: browser
  - id: GitHub
    transport: api
  - id: Google
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Finding candidates who are not applying, by searching precisely rather than broadly.

## Build the query from the work, not the title

Titles are unreliable and vary by company. Search for the evidence of the work: the tools, the certifications, the specific responsibilities. A query built on "Kubernetes AND Terraform AND (SRE OR reliability OR platform)" finds people a title search misses and excludes people a title search wrongly includes.

## Structure it so it can be tuned

```
(title OR synonym OR synonym)
AND (skill AND skill)
AND (location OR remote)
NOT (recruiter OR "seeking opportunities" OR intern)
```

Change one clause at a time and watch the result count. A query changed in three places at once cannot be debugged. Keep the versions that worked.

## Iterate from the results

Read the first twenty profiles. The vocabulary those people use for their own work is better than the vocabulary you guessed - feed it back into the synonym list. This loop is where most of the improvement comes from, not from operator cleverness.

## Gotchas

- Quote multi-word phrases or they are treated as separate terms.
- Different platforms honour different operators; a query that works on one silently degrades on another.
- Exclude your own employees and current candidates before reviewing, not after.
- Boolean finds people who describe their work in writing. That skews by function and by seniority, and it is worth saying so rather than treating the result as the market.
