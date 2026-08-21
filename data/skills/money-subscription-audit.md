---
name: Audit my subscriptions and cancel the dead ones
command: money-subscription-audit
description: Every recurring charge found across your statements, with the forgotten ones cancelled
categories: [Personal, Ops]
roles: [everyone]
worksWith:
  - id: Gmail
    transport: api
  - id: Sheets
    transport: api
writeScope: send
limits:
  require_approval_per_cancellation: true
body_status: authored
---
## When to use

Recurring charges have accumulated and no single place lists them. The user
wants the full picture and wants the dead ones gone.

## Find every recurring charge

Statements are the source of truth, not memory — the whole problem is that
people cannot remember what they signed up for.

1. Pull 12 months of transactions. A year, not 90 days, because annual
   renewals are exactly the ones people forget.
2. Group by normalized merchant. Descriptors drift (`GOOGLE *YOUTUBEPRE`,
   `GOOGLE*YouTube`), so match on the stem, not the string.
3. Keep any merchant billing on a regular cadence — monthly, quarterly, or
   annual. Note the amount, the cadence, and the next expected date.
4. Sweep the inbox for `receipt`, `your subscription`, `renews on`,
   `payment confirmation`. This catches trials that have not billed yet, which
   statements cannot show you.
5. Flag **price increases**: same merchant, higher amount than six months ago.
   These are almost never noticed and are frequently the reason a subscription
   is no longer worth it.

## Build the picture before touching anything

Produce a sheet: merchant, amount, cadence, annualized cost, first seen, last
seen, price change, and a **used / unsure / dead** column. Sort by annualized
cost — a $9/month forgotten subscription outranks a $40 one that gets used.

Do not guess at usage. If there is no evidence either way, mark it `unsure`
and let the user decide. Cancelling something in use is a much worse outcome
than leaving one extra month on something dead.

## Cancel, one at a time, with approval each time

Cancellation flows are built to exhaust you: buried links, retention offers,
"are you sure" chains, and some that require chat or a phone call. That
tedium is the entire reason this is worth automating.

For each item the user approved:

1. Find the cancellation path — account settings, then billing, then
   subscription. If the site hides it, search the help centre for
   "cancel <service>" rather than hunting the UI.
2. Work through the retention flow. **Capture any retention offer and stop.**
   A 50%-off-for-12-months offer changes the decision, and the user should
   make it, not you. Report the offer and wait.
3. Complete the cancellation and **screenshot the confirmation**.
4. Record the confirmation number and the effective end date. Access usually
   runs to the end of the paid period — that is not a failed cancellation.

If a service can only be cancelled by phone or live chat, say so plainly and
hand over the number and the account details. Do not pretend it was handled.

## Verify

A cancellation is not done until it is confirmed. Check for the confirmation
email, and re-check the account a week later — a non-trivial number of
cancellations silently do not take.

## Heuristics

- Annualize everything. $12/month reads as small; $144/year does not.
- A free trial with a card on file is a subscription. Include it.
- Never cancel anything not explicitly approved, even when it is obviously dead.

## Gotchas

App Store and Play Store subscriptions cannot be cancelled from the merchant's
site — they must be cancelled in the store account, and the merchant will not
tell you that. Check there for anything showing as `APPLE.COM/BILL` or
`GOOGLE *`.
