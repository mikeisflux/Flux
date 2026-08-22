---
name: Data warehouse context
command: data-data-warehouse-context
description: Build a reusable reference of your tables, metrics, terminology, and gotchas
categories: [Data]
roles: [analysts, engineering]
writeScope: readonly
body_status: authored
---

## When to use

Working in an unfamiliar warehouse, where the hard part is not SQL but knowing which table is the real one.

## Discover before querying

1. Inventory the schemas and the row counts. Size and recency tell you which tables are live.
2. Find the freshness of each candidate table - `max(updated_at)` against now. A stale table is the most common wrong answer.
3. Identify the layers: raw ingest, staged, and modelled marts. Query the marts and only drop to raw to explain a discrepancy.
4. Look for the naming convention and its exceptions. The exceptions are usually the interesting tables.

## Interview for the things that are not written down

Ask whoever owns the data:

- Which table do you personally use for this, and which one looks right but is not?
- What is deliberately excluded from it?
- What broke recently, and is the fix backfilled or only forward?
- Which columns are deprecated but still populated?

Every warehouse has knowledge that exists only in someone's head. That is the knowledge that stops an analysis being wrong.

## Write it down as you go

Keep a running note per table: grain, key, freshness, owner, known gotchas, and one verified example query. This is the artefact that makes the second analysis fast, and it is worth more than the first analysis.

## Gotchas

- Two tables with the same name in different schemas rarely mean the same thing.
- A view can be slow and expensive without looking it.
- Soft deletes mean `WHERE deleted_at IS NULL` belongs in almost every query, and forgetting it inflates every count.
