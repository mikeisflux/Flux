---
name: Developer handoff specs
command: docs-developer-handoff-specs
description: Spec layout, tokens, states, responsive behavior, edge cases, and motion
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Figma
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Turning a finished design into something that can be built without a stream of clarifying questions.

## Specify the states, not just the screens

A screen is a snapshot; a build needs the whole state machine. For every view: default, empty, loading, partial, error, permission-denied, and success. For every input: valid, invalid, disabled, and what the validation message says and when it appears.

## Give measurements as rules

Absolute pixel positions do not survive a different viewport or a longer string. Specify spacing as tokens, layout as rules ("16px gap, wraps below 640px"), and text as truncation behaviour rather than as a fixed width. Where a component from the design system is used, name it rather than redrawing it.

## Cover the behaviour a static file cannot show

- What is clickable and what happens.
- Transitions and their duration, or explicitly none.
- Focus order and what has focus on open.
- Scroll behaviour, and what is sticky.
- What happens on slow network.

## Gotchas

- Provide the real content and the worst-case content, not lorem ipsum. Layouts break on real strings.
- Export assets at the densities actually needed, and say which format and why.
- List the open questions on the handoff itself. An unanswered question becomes an invented answer.
