---
name: Check everything I own for safety recalls
command: money-recall-sweep
description: A checked list of your vehicles, appliances, and gear against open recall notices
categories: [Personal, Monitoring]
roles: [everyone]
worksWith:
  - id: Sheets
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

Periodically, or when the user wants to know whether anything they own has an
open safety recall. Nobody registers products and nobody checks, so recalls
reach almost no one.

## What to check

**Vehicles** — by VIN, which gives an exact answer rather than a
year/make/model guess. Check the manufacturer's own lookup as well as the
national database; both miss things the other has.

**Child products** — car seats, cribs, strollers, carriers. Highest stakes on
the list and the most frequently recalled. Model and manufacture date are on
the sticker, not the box.

**Appliances and home** — anything with a heating element, a battery, or a
motor. Dryers, space heaters, power banks, e-bike batteries.

**Food and drugs** — only worth checking for things kept long term: supplements,
frozen goods, pantry staples.

## How to check

1. Build the inventory. If the user has no list, start with vehicles, child
   gear, and anything with a lithium battery — that covers most real risk.
2. For each, search the relevant recall database by model and manufacture date.
   Recalls are usually scoped to a date or serial range, so an unqualified
   model match is a maybe, not a yes.
3. Record: item, identifier, recall status, recall number, hazard described,
   and the remedy offered.

## Report by severity, not alphabetically

Lead with anything involving fire, crash, strangulation, or child products.
A dishwasher recall and a car seat recall are not the same finding and should
not be presented as a flat list.

For each hit, give the **remedy and how to claim it** — remedies are free and
usually include shipping, but they expire in practice as stock runs out.

## Heuristics

- No recall found is a real, useful answer. Say it clearly.
- A model match outside the affected serial range is not a recall. Do not alarm.
- Check the manufacture date, not the purchase date.

## Gotchas

Recalls are issued after purchase, so a clean check today means nothing in six
months. This belongs on a schedule — quarterly is reasonable — not as a
one-off.
