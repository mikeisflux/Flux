---
name: Analysis QA & validation
command: data-analysis-qa-validation
description: Methodology, calculation, and bias checks before a stakeholder presentation
categories: [Data]
roles: [analysts, engineering]
writeScope: readonly
body_status: authored
---

## When to use

An analysis is finished and about to be shown to someone who will act on it. The job is to try to break it before they do.

## Check the number against something else

A number is only trustworthy if it agrees with an independent path to the same fact:

- Recompute a total a different way - sum of parts against the aggregate.
- Cross-check against a system of record the analysis did not use.
- Compare against the prior period. An unexplained step change is a bug until proven otherwise.
- Reconcile to a known external figure where one exists, such as a bank statement or an invoice total.

## Interrogate the population

- What was filtered out, and was that deliberate?
- Are test, internal and refunded records included consistently everywhere in the analysis?
- Does the denominator cover the same population as the numerator? Mismatched populations are the most common cause of a plausible-looking wrong rate.
- Is the date range the same in every chart on the page?

## Look for the classic distortions

- **Simpson's paradox** - the trend reverses within every segment.
- **Survivorship** - the sample only contains the entities that lasted.
- **Small denominators** - a 100% conversion rate on three users.
- **Double counting** from a join fan-out.
- **A chart truncated at a non-zero y-axis** exaggerating a small change.

## Gotchas

State the checks you ran and the ones you could not. An analysis that says "reconciled to the ledger, could not verify the regional split" is more useful than one that says nothing and is quietly wrong in one column.
