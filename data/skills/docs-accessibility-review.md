---
name: Accessibility review
command: docs-accessibility-review
description: WCAG 2.1 AA audit: contrast, keyboard, focus, touch targets, screen readers
categories: [Docs]
roles: [everyone]
worksWith:
  - id: Figma
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Checking an interface against what people using it non-visually or non-mouse actually need.

## Run the keyboard pass first

It finds the most, fastest. Tab through the whole page:

- Is every interactive element reachable?
- Is the focus indicator visible on every one, including on coloured backgrounds?
- Is the order logical, matching the visual order?
- Do dialogs trap focus, and return it to the trigger on close?
- Does Escape close what it should?

Anything that cannot be reached or actioned by keyboard is a blocking defect, not a nice-to-have.

## Then structure

- One `h1`, and headings that descend without skipping.
- Landmarks: header, nav, main, footer.
- Lists marked up as lists, tables as tables with real headers.
- Every image either has meaningful alt text or is explicitly decorative with `alt=""`.
- Every form input has a programmatically associated label - a placeholder is not a label.

## Then colour and motion

Contrast of at least 4.5:1 for body text and 3:1 for large text and interface components. No information carried by colour alone. Respect `prefers-reduced-motion`. Check that the page is usable at 200% zoom and at 320px wide.

## Gotchas

- An automated checker finds perhaps a third of real issues. It cannot tell whether alt text is *right*, only whether it exists.
- ARIA added on top of the wrong element is usually worse than no ARIA. Use the native element first.
- A custom control needs the full keyboard behaviour of the native one it replaces, not just a click handler.
- Test with a screen reader at least once. Reading the spec is not the same as hearing the page.
