---
name: Remove me from data broker sites
command: money-data-broker-optout
description: Opt-out requests filed across every major broker, tracked, and re-checked for relistings
categories: [Personal]
roles: [everyone]
worksWith:
  - id: Gmail
    transport: api
  - id: Sheets
    transport: api
writeScope: send
limits:
  min_spacing_seconds: 30
body_status: authored
---
## When to use

The user wants their name, address, phone, and relatives removed from people-
search sites. Usually prompted by finding themselves on one, or by a privacy or
safety concern.

## Why this is worth automating

There are dozens of brokers. Each has its own removal flow, most are
deliberately awkward — email confirmation loops, CAPTCHAs, ID upload demands,
mailed forms — and **they relist you**. A removal done once decays within
months. The re-check is as important as the original request, and it is the
part no human keeps up with.

## Work the list

Maintain the broker list in the sheet; it changes as brokers appear, merge, and
rebrand. For each one:

1. **Search first.** Find the specific profile URL for this person. Confirm it
   is actually them — matching a common name to the wrong record can remove a
   stranger's listing and leave the user's in place. Check age, city history,
   and relatives before proceeding.
2. **Find the opt-out path.** Usually a footer link (`Do Not Sell My Info`,
   `Privacy`, `Opt Out`). If absent, check `/optout` and `/do-not-sell`
   directly, then the privacy policy, which is legally required to describe the
   mechanism.
3. **Submit the request** with the minimum information the form requires.
4. **Never upload government ID.** Some brokers ask for a driver's licence.
   Handing more identity documents to a data broker to reduce your exposure is
   backwards. Flag those for the user to decide, and do not submit.
5. **Confirm by email** where the flow requires it.
6. **Record**: broker, profile URL, date submitted, method, confirmation, and
   status.

## State-law leverage

If the user is in a state with a comprehensive privacy law (California,
Colorado, Connecticut, Virginia, Texas, Oregon and others), cite the deletion
right in the request. Brokers respond materially faster to a request that
names a statute and a deadline than to a generic form submission.

## Re-check on a schedule

Run this monthly. For every previously-removed broker, search again. Relisting
is normal and expected — treat it as routine maintenance, not as a failure of
the first request. Re-file, and note the relist count per broker so the user
can see which ones are worth continuing to fight.

## Heuristics

- One request at a time, spaced out. Hammering a form gets the IP blocked and
  nothing filed.
- Use a dedicated email alias for these. Every form is another address in
  circulation.
- Never submit data the form does not require, and never more than is already
  published.

## Gotchas

Removal from a broker's site does not remove the underlying data — it removes
the public listing. The broker may still sell it. Say this plainly so the user
does not overestimate what was achieved.
