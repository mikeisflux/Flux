---
name: Check I can actually enter the countries on my trip
command: travel-entry-requirements
description: A per-country check of visas, passport validity, and transit rules against your actual itinerary
categories: [Personal, Research]
roles: [everyone]
worksWith:
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

Before any international trip, and specifically before booking anything
non-refundable. Also worth running again two months out, since requirements
change.

## Gather the itinerary properly

Entry rules depend on details people do not think of as relevant:

- **Passport**: issuing country, expiry date, and blank pages remaining
- Every country **entered**, in order, with dates
- Every country **transited**, including airport-only connections
- Purpose of travel, and whether any work is involved
- Recent travel history, for countries that care about it

## Check each country

For each, verify against official sources — the destination's immigration
authority, and the user's own government travel advisory:

1. **Visa requirement** for this passport, this purpose, this length of stay.
2. **Electronic authorization** — several countries now require a pre-approved
   travel authorization that is not a visa and is easy to miss entirely.
   Note the processing time; some are instant, some are not.
3. **Passport validity rule.** Many countries require six months' validity
   beyond the date of *departure from* that country. A passport that is valid
   the whole trip can still fail this test, and airlines enforce it at
   check-in.
4. **Blank pages.** Some require two or more.
5. **Onward or return ticket** requirements.
6. **Proof of funds or accommodation**, where asked for.
7. **Vaccination or health documentation.**

## Transit is where people get caught

Airport transit frequently requires its own visa, even without leaving the
airside area, and it depends on passport nationality. Check every connection
point separately — a transit through a country the user never intended to
"visit" is the single most common way a trip breaks at the gate.

Also check whether the connection requires collecting and re-checking bags,
since that forces immigration entry and changes the requirement entirely.

## Watch cumulative-stay rules

Some regions limit total days across a rolling window rather than per entry.
Where the itinerary re-enters such a region, count the total across the whole
window, including any prior trips the user mentions. This is easy to breach
accidentally on a multi-country trip.

## Report

A table per country: entry requirement, authorization needed, cost, processing
time, passport validity needed vs the user's actual expiry, and a
**pass / action needed / blocked** status. Then an ordered action list with
deadlines, working backwards from departure.

Cite the official source for each. Requirements change and third-party summaries
go stale; the user needs to be able to re-check.

## Heuristics

- Airlines enforce entry rules at check-in. Being technically right at the
  border does not help if boarding is refused.
- Check the passport expiry against the six-month rule before anything else.
  It is the most common blocker and the slowest to fix.
- Rules are by passport nationality, not residence.

## Gotchas

Dual nationals may face different rules on each passport, including a
requirement to enter a country of citizenship on that country's passport. If
the user holds two, ask which they intend to travel on before checking.
