---
name: Book this trip and stop before payment
command: travel-book-staged
description: Your flight or hotel selected, filled in, and staged on the payment page for your final click
categories: [Personal]
roles: [everyone]
worksWith:
  - id: Docs
    transport: api
writeScope: purchase
limits:
  stop_at_payment: true
  never_submit_payment: true
caution: >
  Fills a booking to the payment step and stops. Never submits payment.
  Travel bookings are frequently non-refundable and name changes are
  expensive or impossible, so the final commitment stays with the user.
body_status: authored
---
## When to use

The user has decided what to book and wants the tedious part done — search,
selection, passenger details, seat choice — without handing over the final
commitment.

## Why it stops at payment

Travel is the worst combination for autonomous purchase: often non-refundable,
frequently expensive, and unforgiving of small errors. A name that does not
match the passport can require rebooking at full fare. A date off by one is a
wasted ticket. The marginal effort of one click is trivial; the cost of getting
it wrong is not.

So this fills everything and stops on the payment page with the total visible.

## Confirm the parameters before searching

Read the request back explicitly and get confirmation on:

- **Exact dates**, stated as full dates with day names. "Next Friday" is
  ambiguous and overnight flights shift the return date.
- **Airports**, by code, where a city has several. The cheap fare into the
  distant airport is often not the cheaper trip.
- **Passenger names exactly as printed on the passport**, including middle
  names where the passport shows them.
- Cabin, bag requirements, and any loyalty numbers.

Do not infer any of these. An assumption here is what produces the expensive
mistake.

## Search and present the real comparison

Search across the airline or property direct site and the major platforms.
Present a shortlist compared on **total cost including bags, seat selection,
and taxes** — not the headline fare. Direct booking is often slightly dearer
and materially easier to change later; say so where the gap is small.

Note for each: refundability, change fees, and what is actually included.

## Fill it in

Once the user picks:

1. Enter passenger details exactly as given. Re-read them against what the user
   supplied before continuing.
2. Apply loyalty numbers and any known-traveller or redress numbers.
3. Select seats if requested. Do not silently accept paid seat selection —
   report the cost and let the user decide.
4. Decline every upsell by default: insurance, priority boarding, bundles.
   Report anything genuinely relevant instead of accepting it.
5. Advance to the payment page.

## Stop and hand over

Report:

- The exact itinerary, with dates, times, and airports written out in full
- The total, itemised
- What is refundable and what is not, and by when
- Anything that could not be filled and needs the user's input
- A clear statement that payment has **not** been submitted

Then stop. Do not enter payment details even if they are stored.

## Verify before handing over

Re-read the staged booking against the confirmed parameters: names, dates,
airports, cabin. Catching a transposed date on the payment page costs nothing;
catching it after purchase can cost the fare.

## Heuristics

- Prices change between search and checkout. If the total moved, say so before
  handing over rather than letting the user discover it.
- Never accept a default that costs money.
- If a fare disappears mid-flow, re-search rather than taking the next thing
  offered.

## Gotchas

Some sites hold inventory for a limited window at checkout. Note the hold
expiry when handing over, so the user knows how long they have before the
staged booking has to be redone.
