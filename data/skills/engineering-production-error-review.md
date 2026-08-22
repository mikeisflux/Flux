---
name: Production error review
command: engineering-production-error-review
description: Pull and summarize recent production errors and issue health from your error monitor
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Sentry
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

A regular pass over what production actually threw, to catch the slow-burning problems before they become incidents.

## Compare against last time

The value of a recurring review is the delta, not the snapshot:

- What is new since the last review?
- What grew, and by how much?
- What was marked resolved and came back?
- What has been in the queue every week for a quarter without being touched?

That last category is the important one. A persistent error nobody owns is a decision to tolerate it, and it should be made explicitly.

## Look past the error queue

The most serious production problems often do not raise errors: elevated latency, a rising retry rate, a queue growing slowly, a background job that silently stopped. Include those signals in the review or it only covers the failures loud enough to notice.

## Close the loop

Every reviewed item ends in one of four states: fixed, ticketed with an owner and a date, filtered as noise with the filter applied, or explicitly accepted with a reason. Nothing stays in the review without a state.

## Gotchas

- Error counts are not comparable across a traffic change; normalise by request volume.
- A deploy in the review window changes the baseline, so annotate the timeline with deploys.
- If the same class of bug appears repeatedly, the finding is a missing test or a missing type, not the individual bugs.
