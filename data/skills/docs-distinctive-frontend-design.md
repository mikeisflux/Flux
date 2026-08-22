---
name: Distinctive frontend design
command: docs-distinctive-frontend-design
description: Make intentional palette, type, and layout choices that avoid templated looks
categories: [Docs]
roles: [everyone]
writeScope: readonly
body_status: authored
---

## When to use

Building an interface that does not look like every other framework default, without sacrificing usability.

## Distinctiveness comes from a few deliberate choices

Not from decorating everything. Pick two or three and commit:

- **Type** - a genuinely characterful typeface for headings, with a boring one for body. Type does more for identity than any other single choice.
- **Space** - unusually generous or unusually tight, consistently. Density is a personality.
- **One structural idea** - a persistent column, a distinctive card shape, an unusual navigation position.

Everything else stays conventional so the unconventional part is legible as a choice.

## Constrain the palette

Fewer colours applied more confidently beats a full palette. A near-monochrome interface with one accent reads as designed; six accent colours reads as unfinished. Decide what the accent is *for* - one job, consistently - and never use it for anything else.

## Keep the conventions that carry meaning

Underlined links, a focus ring, a back button that goes back, form errors near the field, a primary action on the right in a dialog. These are not stylistic - they are learned behaviour, and breaking them costs users comprehension for no aesthetic gain.

## Gotchas

- Test the distinctive choice at the smallest viewport and the longest string. Most fail there.
- Check contrast on every colour pairing, including the accent, before falling in love with it.
- Motion is the easiest thing to overdo and the first thing to disable under `prefers-reduced-motion`.
