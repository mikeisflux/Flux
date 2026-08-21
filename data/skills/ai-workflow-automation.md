---
name: Work out what to automate and build it
command: ai-workflow-automation
description: A shortlist of what is worth automating, with the highest-value one specified and built
categories: [Ops, Engineering]
roles: [ops, founders]
worksWith:
  - id: Sheets
    transport: api
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

Someone is doing repetitive work and wants it automated, but has not decided
what to automate or how. Also the right starting point when someone asks to
automate one specific thing — it is often not the best candidate.

## Pick the right target

The instinct is to automate the most annoying task. The right choice is the
one scoring highest across three factors, and annoyance is not one of them:

- **Frequency.** Daily beats monthly by an order of magnitude. Automating a
  quarterly task almost never repays the build.
- **Stability.** Does the process change often? Automating a moving target
  produces something that breaks constantly and is worse than doing it
  manually.
- **Determinism.** How much judgement does each run need? Fully mechanical
  work automates cleanly; work needing a real decision every time does not.

Multiply, do not add. A daily task that changes every week is a bad
candidate regardless of frequency.

Then check **cost of being wrong**. A task that is high-frequency, stable, and
mechanical but silently ruins data when it misfires needs verification built in
from the start, not added later.

## Watch it being done once

Before specifying anything, have the person do the task while narrating. This
consistently surfaces things nobody mentions when describing it from memory:

- The check they do without thinking about it
- The exception they handle differently and did not consider worth saying
- The step that only happens on Mondays, or at month end
- Where they actually get the data, versus where they said they get it

The gap between the described process and the observed one is where automation
breaks.

## Specify it precisely

Write down, concretely enough to test:

- **Trigger** — schedule, event, or manual
- **Inputs** — where they come from, what they look like, what happens when
  they are missing or malformed
- **Steps** — in order, with the decision rule at each branch
- **Output** — exactly what is produced and where it goes
- **Exceptions** — what happens when something is unexpected. The default
  should be **stop and surface it**, never guess and continue.
- **Verification** — what proves the run actually worked

The exception handling is what separates automation that survives from
automation that quietly corrupts things for a month.

## Choose the mechanism, cheapest first

- **A script or a built-in feature.** If the steps are fixed, no model is
  needed. Model calls in a deterministic pipeline add cost, latency, and
  variance for nothing.
- **A pipeline with model calls at specific points** — for steps needing
  language understanding (classify this, summarise that) inside an otherwise
  fixed sequence. This covers most real cases.
- **An agent** — only when the path genuinely varies per run.

Most requests for "an AI automation" are best served by the second option, and
plenty by the first.

## Build it to fail loudly

- Run it **alongside the manual process** first and compare outputs until they
  agree. Do not cut over on the strength of one successful test run.
- **Log every run** with inputs, outputs, and duration.
- **Alert on failure and on silence.** A job that stops running produces no
  errors, and nobody notices for weeks. Absence of a run is the failure mode
  that hurts most.
- Make it **re-runnable** without side effects, so a failed run can simply be
  repeated.

## Heuristics

- Automate the step, not the whole job, when the whole job needs judgement.
- If it takes longer to build than it will save in a year, do not build it.
- The person who does the task should be able to tell whether the run was
  correct. If they cannot, the output is not in the right form.
- Anything on a schedule needs a monitor, or it will fail silently.

## Gotchas

Automation shifts the work rather than removing it: someone now maintains the
automation, checks its output, and fixes it when the source changes. Name who
that is before building. Unowned automation degrades quietly and is usually
discovered by the damage it caused.
