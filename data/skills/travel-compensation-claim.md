---
name: Claim compensation for my delayed or cancelled flight
command: travel-compensation-claim
description: A filed claim with the airline, backed by the regulation that applies and evidence of the delay
categories: [Personal]
roles: [everyone]
worksWith:
  - id: Gmail
    transport: api
  - id: Docs
    transport: api
writeScope: draft
body_status: authored
---
## When to use

A flight was delayed, cancelled, overbooked, or bags went missing, and the
user wants to know what they are owed and to file for it.

## Establish the facts first

Compensation turns on specifics, so gather them before assessing anything:

- Booking reference, ticket number, and the **operating** carrier (which is
  often not the one that sold the ticket — codeshares matter here)
- Scheduled and actual departure and arrival times. **Arrival delay is what
  most rules key on**, not departure, and the two frequently differ.
- Origin, destination, and any connections, with the great-circle distance
- What the airline said the cause was, and what it said at the time
- Whether the user was rerouted, and what they were offered
- Receipts for anything they had to buy

Evidence sources: the boarding pass, the airline's own app history, the email
trail, and public flight-status records for the actual times.

## Work out which rules apply

The applicable regime depends on the route and the carrier, not on where the
user lives:

- **EU261** covers departures from an EU airport on any carrier, and arrivals
  into the EU on an EU carrier. Fixed compensation by distance, triggered at
  three hours' arrival delay, plus a duty of care (meals, hotel) that applies
  regardless of cause.
- **UK261** mirrors it for UK departures and UK-carrier arrivals.
- Several other jurisdictions have their own passenger-rights regimes; check
  the one matching the route.
- **US** has no federal delay-compensation mandate, but does require refunds
  for cancellations and significant changes, and denied-boarding compensation
  is separately regulated.
- **The Montreal Convention** covers international baggage delay, damage, and
  loss, and can apply where a local regime does not.

Where more than one could apply, identify which is more favourable and file
under that one.

## Assess honestly

The main exclusion is **extraordinary circumstances** — weather, air traffic
control, security, strikes outside the airline. Technical faults generally do
*not* qualify, and airlines assert this exclusion far more often than it holds.

State the strength of the claim plainly. "The airline cited a technical issue,
which is usually within their control and not an extraordinary circumstance" is
useful. "You are owed €600" is not something to assert before the airline has
responded.

## Draft the claim

Use the airline's own claims form where one exists — claims filed off-channel
get lost. Include:

- The route, date, and booking reference
- The delay in arrival hours, stated precisely
- The regulation and article being claimed under
- The amount, with the distance band it derives from
- Receipts for care expenses, itemised
- A clear statement of what is being asked for

Keep it factual and short. Long letters do not improve outcomes; correct
citations do.

## Then follow up

Airlines commonly reject first claims, including valid ones. Note the response
deadline for the jurisdiction and diary a follow-up. If rejected on
extraordinary circumstances, the next step is usually the national enforcement
body or an alternative dispute resolution scheme — name it for the user.

## Heuristics

- Arrival delay, not departure delay.
- The operating carrier is liable, not the seller.
- Duty of care (meals, accommodation) is owed even when compensation is not.
- Claim windows are long — often years — so an old flight may still be live.

## Gotchas

Airlines sometimes offer vouchers worth less than the statutory cash amount and
present them as settlement. Flag the cash equivalent whenever a voucher is
offered so the user can compare properly.
