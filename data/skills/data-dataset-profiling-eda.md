---
name: Dataset profiling (EDA)
command: data-dataset-profiling-eda
description: Assess a dataset's shape, quality, and patterns so you trust it before analysis
categories: [Data]
roles: [analysts, engineering]
worksWith:
  - id: Docs
    transport: api
  - id: Sheets
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

A dataset arrives and nobody knows what is in it yet. Before any analysis, the job is to establish shape, quality and the traps.

## Profile in this order

1. **Shape.** Rows, columns, and the grain - one row per what? Confirm it rather than assuming it, with a count of duplicates on the presumed key.
2. **Completeness.** Null rate per column. A column that is 90% null is not a column you can analyse; a column that is 2% null needs a decision.
3. **Cardinality.** Distinct count per column. Cardinality 1 means a constant; cardinality equal to the row count means an identifier.
4. **Distribution.** For numerics: min, max, the quartiles, and the count of zeros and negatives. For dates: min, max, and gaps.
5. **Categoricals.** Top 20 values and their share. This is where `"N/A"`, `"n/a"`, `"NA"` and `""` all show up as different things.

## What to look for

- **Sentinel values** standing in for missing: `-1`, `0`, `9999`, `1970-01-01`, `1900-01-01`.
- **Mixed units** in one column - dollars and cents, seconds and milliseconds.
- **Truncation** - a max string length that is exactly 50 or 255 means data was cut.
- **Timezone drift** - a daily count with a dip every day at the same hour is a UTC boundary.
- **Survivorship** - a table of active customers cannot answer a question about churn.

## Report

One page: the grain, the row count, a table of columns with null rate and cardinality, and a short list of the specific problems found with an example row for each. State what the dataset cannot answer as plainly as what it can.
