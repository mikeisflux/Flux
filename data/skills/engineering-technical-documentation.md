---
name: Technical documentation
command: engineering-technical-documentation
description: READMEs, API docs, runbooks, architecture docs, and onboarding guides
categories: [Engineering]
roles: [engineering]
writeScope: readonly
body_status: authored
---

## When to use

Writing documentation for a system that someone other than its author has to work with.

## Write to the reader's job, not the system's structure

Four kinds of document, and mixing them is why most documentation fails:

- **Tutorial** - get a newcomer to a first success. Linear, opinionated, no alternatives.
- **How-to** - accomplish a specific task. Assumes competence.
- **Reference** - what every parameter does. Complete and boring.
- **Explanation** - why it works this way. The part that ages best and is written least.

## Lead with the thing that works

A runnable example in the first screen. Not the architecture, not the philosophy - a command or a snippet the reader can paste and see succeed. Everything after that is read with more patience.

## Write down the parts that surprised you

The single highest-value content is the thing that was not obvious: the gotcha, the ordering requirement, the flag that has to be set. Whoever wrote the system knows these and assumes they are obvious. They are the whole reason to read the document.

## Gotchas

- Documentation that is not near the code goes stale silently. Put it in the repository and review it in the same pull request.
- Every code sample should be one that has actually been run. Untested samples are the most damaging kind of wrong.
- Say what the system does *not* do. The absence of a capability is the hardest thing to discover from documentation and the most annoying to discover late.
