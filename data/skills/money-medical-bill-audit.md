---
name: Check a medical bill against my EOB
command: money-medical-bill-audit
description: A line-by-line comparison flagging duplicates, upcoding, and charges your plan already covered
categories: [Personal]
roles: [everyone]
worksWith:
  - id: Sheets
    transport: api
writeScope: readonly
caution: >
  Produces findings and questions to raise, never a determination that
  billing was improper. Coding disputes are resolved between the provider,
  the insurer, and the patient.
body_status: authored
---
## When to use

A medical bill has arrived and the user wants to know whether it matches what
their insurer actually processed, before paying it.

## Get both documents

You need the **itemized bill** and the **Explanation of Benefits**. A summary
bill showing "amount due" is not enough — the whole audit happens at line
level, and providers frequently send the summary by default. If only the
summary exists, tell the user to request the itemized version; they are
entitled to it.

## Reconcile line by line

For every line on the bill:

1. Match it to an EOB line by CPT/HCPCS code and date of service.
2. Compare the **allowed amount**, not the billed amount. The billed figure is
   a list price almost nobody pays; the allowed amount is what the contract
   says.
3. Confirm the patient responsibility on the bill equals the EOB's
   patient-responsibility figure. A bill exceeding it is the most common and
   most recoverable error.

Then flag:

- **Duplicates** — the same code, same date, billed more than once.
- **Unbundling** — a panel billed as a bundle *and* its components billed
  separately.
- **Lines with no EOB match** — either never submitted to insurance, or denied.
  These are very different situations and the user needs to know which.
- **Balance billing** — an in-network provider charging above the allowed
  amount. Frequently not permitted.
- **Out-of-network charges at an in-network facility** — anesthesia, radiology,
  pathology. Often covered by surprise-billing protections.
- **Dates of service the user was not there.**

## Report

A table: line, code, description, billed, allowed, plan paid, patient owes,
what the EOB says, and the discrepancy. Then a short list of **questions to
ask**, phrased as questions:

> "Line 4 shows CPT 80053 twice on 03/12 — was this drawn twice, or is one a
> duplicate?"

That framing is both more accurate and more effective than an accusation.

## Heuristics

- Never state that fraud or upcoding occurred. State what does not reconcile.
- An unmatched line is a question, not an error — claims get resubmitted.
- If the numbers reconcile cleanly, say so. A clean bill is a real result.

## Gotchas

EOBs lag bills, often by weeks. If no EOB exists yet, the correct advice is to
wait rather than to pay — paying before the insurer processes forfeits leverage
and can complicate a refund.
