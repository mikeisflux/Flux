---
name: Design critique
command: docs-design-critique
description: Structured feedback on usability, hierarchy, consistency, and accessibility
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Figma
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Reviewing a design in progress, so that the feedback improves it rather than just registering an opinion.

## Establish what is being asked

Before any comment: what stage is this, and what feedback is useful now? Critiquing button radii on a flow that is still deciding its steps wastes everyone's time and drowns the useful comment. Ask for the goal, the constraints, and the specific question.

## Critique against the goal, not against taste

Frame every comment as an observation and a consequence, not a preference:

- Not "I don't like the grey" - "the grey on white is around 2.5:1, which fails contrast, so the label will be unreadable for some people".
- Not "move this up" - "the primary action is below the fold at the most common viewport, so most users will not see it".

If a comment cannot be tied to a user consequence or a constraint, mark it explicitly as taste.

## Cover what gets skipped

- The empty, loading and error states.
- The longest realistic content, and the shortest.
- Keyboard and screen reader paths.
- What happens on a small viewport.
- Whether it uses existing components or invents new ones, and whether the invention is justified.

## Gotchas

- Separate blocking from non-blocking explicitly, every time.
- The designer has usually already considered the obvious alternative. Ask before proposing it.
- One person collecting and de-duplicating the group's comments prevents the designer receiving eleven versions of the same note.
