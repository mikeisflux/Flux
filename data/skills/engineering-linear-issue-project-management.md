---
name: Linear issue & project management
command: engineering-linear-issue-project-management
description: Read, create, and update Linear issues, projects, and cycles for triage and planning
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Linear
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Keeping issues and projects in a state where the tool can be trusted to answer questions about what is happening.

## Write issues that survive being read later

- **Title** states the outcome, not the activity: "Checkout fails on expired card" rather than "Look at checkout".
- **Body** has the reproduction or the requirement, and the definition of done.
- **Estimate** only where it changes a decision.
- **One issue per shippable change.** An issue that cannot be closed in a cycle is a project.

Link related issues rather than describing the relationship in prose; the graph is queryable and the prose is not.

## Keep state true

The value of the tracker is entirely in whether its state matches reality. That means: move an issue when the work moves, not at the end of the week; close what is done; and cancel what will not be done with a reason rather than leaving it to rot in the backlog.

A backlog nobody prunes becomes a place issues go to be forgotten, which is worse than deleting them because it looks like a plan.

## Projects

A project needs a target date, an owner, and a description that says what will be true when it is finished. Without the last one there is no way to tell whether it is done. Update the project status in words weekly - the status field alone does not tell anyone why.

## Gotchas

- Do not use labels for things that should be state; you end up with two sources of truth that disagree.
- An issue assigned to a team rather than a person is unassigned.
- If a cycle consistently carries over half its issues, the problem is the planning, not the execution.
