---
name: Change Request & Rollback Plan
command: ops-change-request-rollback-plan
description: Document a system/process change with impact analysis, risks, and rollback
categories: [Ops]
roles: [ops, founders]
writeScope: readonly
body_status: authored
---

## When to use

Planning a change to a production system, including how to undo it.

## Write the change down precisely

What is changing, on what, when, who is doing it, and how long it takes. Vague changes cannot be reviewed and cannot be rolled back. Include the exact commands or the exact configuration diff, not a description of them.

## Write the rollback before the change

And make it specific: the exact steps to return to the previous state, how long they take, and who can authorise them. Then answer the question people avoid: **is the rollback still safe after data has been written under the new version?** If it is not, that is a schema or a migration problem to solve before the change, not after.

## Define the abort criteria and who calls it

What observation means stop - an error rate, a latency threshold, a failed verification step - and who has the authority to invoke rollback without further approval. In the moment nobody wants to be the one to call it, so it has to be decided beforehand.

## Verify, then watch

A verification step immediately after, testing the actual behaviour rather than that the deploy completed. Then a defined watch period with a named person and the specific metric they are watching.

## Gotchas

- Backwards-compatible migrations first, in a separate change, so the rollback of the code does not require a rollback of the data.
- A change with no rollback is not a change, it is a commitment - say so explicitly and get it approved on that basis.
- Record what actually happened against the plan. That record is what makes the next change plan realistic.
