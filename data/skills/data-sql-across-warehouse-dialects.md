---
name: SQL across warehouse dialects
command: data-sql-across-warehouse-dialects
description: Correct, performant SQL for Postgres, Snowflake, BigQuery, Redshift, Databricks
categories: [Data]
roles: [analysts, engineering]
writeScope: readonly
body_status: authored
---

## When to use

A query has to run on a warehouse other than the one it was written for, or the same logic has to work across Snowflake, BigQuery, Postgres and Redshift.

## What actually differs

Most of a query ports unchanged. These are the parts that do not:

| Concern | Snowflake | BigQuery | Postgres | Redshift |
|---|---|---|---|---|
| Identifier case | folds upper | case-sensitive | folds lower | folds lower |
| Date add | `DATEADD(day, 1, d)` | `DATE_ADD(d, INTERVAL 1 DAY)` | `d + INTERVAL '1 day'` | `DATEADD(day, 1, d)` |
| String concat | `\|\|` | `CONCAT` | `\|\|` | `\|\|` |
| Arrays | `ARRAY_AGG` | `ARRAY_AGG` + `UNNEST` | `array_agg` | no native arrays |
| Semi-structured | `VARIANT` | `JSON` / `STRUCT` | `jsonb` | `SUPER` |

Quoting an identifier freezes its case, which is how a query that works in one warehouse fails to find a column in another.

## Port in this order

1. **Date and time functions** - the biggest source of silent differences, because a wrong date function usually returns something rather than erroring.
2. **Window function syntax** - frame clauses (`ROWS` vs `RANGE`) differ in default behaviour.
3. **Type casts** - implicit casting rules vary; make every cast explicit.
4. **Semi-structured access** - rewrite rather than translate; the models are genuinely different.

## Gotchas

- Redshift is Postgres-derived but is not Postgres: no `jsonb`, different distribution semantics, and `DISTKEY`/`SORTKEY` change what is fast.
- BigQuery charges by bytes scanned, so a `SELECT *` that is merely untidy elsewhere is expensive there. Partition filters are not optional.
- Division of integers truncates in Postgres and Redshift and does not in BigQuery.
- Run both versions against the same date range and diff the results before declaring a port done.
