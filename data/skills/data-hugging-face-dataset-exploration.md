---
name: Hugging Face dataset exploration
command: data-hugging-face-dataset-exploration
description: Browse, search, filter, and paginate any Hugging Face dataset via the Dataset Viewer
categories: [Data]
roles: [analysts, engineering]
worksWith:
  - id: Hugging Face
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Evaluating a Hugging Face dataset before using it - for fine-tuning, evaluation, or as a source.

## Read the card before the data

The dataset card answers most of the disqualifying questions:

- **Licence.** This decides whether the rest of the exercise matters. Check the licence on the dataset *and* on its upstream sources; they differ more often than not.
- **Provenance.** Scraped, synthetic, human-annotated, or model-generated? Model-generated data used for evaluation is circular.
- **Splits.** Are train/validation/test predefined, and is the split random or by some grouping?
- **Known issues.** The community tab usually has the problems the card does not.

## Then look at the rows

Load a sample and actually read fifty examples. Every automated check misses what reading catches.

- Field types and lengths; truncation at a round number means data was cut.
- Duplicate rate, including near-duplicates.
- Label balance, and whether labels are single or multi.
- Language mix, if it claims to be monolingual.

## Check for contamination

If the dataset will be used for evaluation, check for overlap with common pretraining corpora and with any training set you use. Contaminated evaluation data produces numbers that are indistinguishable from real progress and are not.

## Gotchas

- Dataset size on the card is often before deduplication.
- `streaming=True` avoids downloading hundreds of gigabytes to discover a licence problem.
- A dataset can be updated in place; pin a revision hash for anything reproducible.
