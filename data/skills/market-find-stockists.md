---
name: Find shops that would carry my product
command: market-find-stockists
description: A researched sheet of independent retailers who stock comparable products, with buyer contact and pitch angle
categories: [Sales, Marketing]
roles: [sales, founders]
worksWith:
  - id: Sheets
    transport: api
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

The user makes a physical product and wants wholesale or consignment placement
in independent shops.

## Build the target list from evidence

The qualifying signal is simple: **do they already stock things like this?**
A shop carrying comparable independent products has a buyer who is open to
them, an existing customer base for them, and no need to be convinced the
category exists.

Find them by:

1. **Comparable products' stockist lists.** Many small makers publish "where to
   buy". That list is a pre-qualified target list.
2. **Directories and association member lists** for the retail category.
3. **Local search across target cities**, then reading each shop's site and
   social to see what they actually carry.
4. **Event vendor lists** — shops that exhibit at trade or fan events buy from
   small makers by definition.

## Qualify before adding

For each shop, confirm from public evidence:

- They carry independent or small-press products, not only major distributors
- They are trading — recent posts, current hours, reviews within the year
- Rough size, so the pitch and the minimum order are realistic
- **Their submission process.** Many publish exactly how to pitch, and
  following it matters more than the pitch itself.

Drop anything failing these. A long list of unqualified shops is worse than a
short qualified one — it produces a low hit rate that reads as "wholesale does
not work".

## Find the right person

Ask for the buyer or owner by name where it is public. A pitch to
`info@` from a stranger is deleted. Small shops are often owner-buyers, and the
owner's name is usually on the About page or the local press coverage.

## Write a per-shop angle

The one line that makes the pitch worth reading is the specific reason it fits
**this** shop. Something they already carry, a local connection, an event they
run. Generic pitches to independents fail at a rate that makes the whole
exercise pointless.

## Report

A sheet: shop, city, what they carry that is comparable, size, buyer name,
contact, stated submission process, the angle, and a fit score. Sorted by fit,
not alphabetically.

## Heuristics

- Evidence of comparable stock beats every other signal.
- Follow the stated submission process exactly, even when a shortcut exists.
- Consignment is a realistic opening for a first placement; do not treat
  wholesale-only as the bar.

## Gotchas

Terms vary enormously — wholesale discount, minimums, payment timing, returns.
Gather them where public so the user is not negotiating from zero, and flag
where they are not.
