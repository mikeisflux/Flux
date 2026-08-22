---
name: Design system documentation
command: docs-design-system-documentation
description: Audit consistency, document components, or design new patterns that fit
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Figma
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Documenting a component so that someone can use it correctly without asking the person who built it.

## Per component, in this order

1. **What it is for**, in one sentence, and the decision it supports.
2. **When to use it, and when not** - with the component to use instead. This section prevents more misuse than any other.
3. **Anatomy** - the parts, named, on an annotated example.
4. **Variants and states** - default, hover, focus, active, disabled, loading, error, and what triggers each.
5. **Content rules** - length limits, capitalisation, what happens on overflow.
6. **Accessibility** - the role, the keyboard behaviour, what a screen reader announces.
7. **The code**, last, with a runnable example.

## Show the wrong usage

A do-and-don't pair teaches faster than a paragraph. The don'ts should be real mistakes seen in the product, not invented ones - the real ones are the ones that will recur.

## Gotchas

- Documentation that lives away from the component goes stale. Generate what can be generated from the source: props, tokens, variants.
- Every example must be one that has actually been rendered.
- Version the documentation with the component, and say when a variant is deprecated and what replaces it.
