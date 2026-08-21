---
name: Watch my hotel rate and rebook when it drops
command: travel-rate-rebooking
description: Your booking automatically moved to a lower rate whenever one appears, with the old one cancelled
categories: [Personal, Monitoring]
roles: [everyone]
worksWith:
  - id: Sheets
    transport: api
  - id: Gmail
    transport: api
writeScope: purchase
limits:
  free_cancellation_only: true
  max_price_increase: 0
  check_frequency_hours: 12
  stop_before_deadline_hours: 48
caution: >
  Books and cancels real reservations. Constrained to free-cancellation
  rates only, so every action is reversible. Never books a non-refundable
  rate and never books at a higher price than the current reservation.
body_status: authored
---
## When to use

The user has a hotel stay booked, or is about to book one, and wants the rate
watched until arrival.

## Why this works

Hotel pricing moves constantly — inventory, demand forecasts, and cancellations
all push rates around, often downward as a date approaches and the property
adjusts. Almost nobody re-checks after booking, so the drop goes unclaimed.

The mechanic that makes it safe is the **free-cancellation rate**. A refundable
booking can be replaced by a cheaper refundable booking with no exposure. That
is the only reason an agent should be completing purchases here at all, and it
is why the constraint is absolute rather than a default.

## Establish the baseline

1. Record the current reservation: property, room type, dates, rate, total
   including taxes and fees, confirmation number, and the **cancellation
   deadline**.
2. Confirm the existing booking is actually refundable. If it is not, this
   skill cannot help — say so and stop. Do not book a second room alongside a
   non-refundable one.
3. Note the exact room type. A cheaper rate for a smaller room or a worse bed
   configuration is not a drop, and this is the most common false positive.

## Check on a schedule

Every twelve hours, price the **same property, same dates, same room type**:

- The direct site, signed in if the user has a loyalty account — member rates
  are frequently lower and are invisible when signed out
- The major booking platforms
- Any rate the user's memberships unlock

Compare **total cost including taxes and fees**, never the nightly headline.
Resort fees and city taxes routinely erase an apparent saving.

## Rebook — order matters absolutely

When a genuinely lower total appears on a free-cancellation rate:

1. **Book the new rate first.** Get the confirmation number in hand.
2. **Verify the new booking exists** — confirmation email, or the reservation
   visible in the account. Not just a success page.
3. **Only then cancel the original**, and capture that cancellation
   confirmation too.

Never cancel first. A cancel-then-book sequence that fails in the middle leaves
the user with no room, and inventory can disappear between the two steps. The
brief overlap of two reservations is the correct trade.

## Stop conditions

- **48 hours before the cancellation deadline**, stop watching and report. Too
  close to arrival, a failed rebooking is unrecoverable.
- If the only cheaper rates are non-refundable, report them and let the user
  decide. Do not book one.
- If the property is nearly sold out, stop. The saving is not worth the risk of
  losing the room.

## Report

Each rebooking: old rate, new rate, saving, both confirmation numbers, and the
new cancellation deadline. At the end of the watch, the cumulative saving.

## Heuristics

- Totals, not nightly rates. Always.
- Same room type or it is not a comparison.
- Two live bookings for ninety seconds is fine. Zero live bookings is not.

## Gotchas

Cancellation policies differ between rates at the same property. The new
booking's deadline may be *earlier* than the old one — read it, record it, and
reset the watch window accordingly rather than assuming it carried over.
